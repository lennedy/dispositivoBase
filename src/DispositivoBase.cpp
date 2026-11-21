#include "DispositivoBase.h"

#include <stdarg.h>
#include <stdio.h>

#ifndef DISPOSITIVO_BASE_SEM_OTA
#include <ArduinoOTA.h>
#endif

namespace {
const size_t kTamanhoMaximoComando = 512;  // bytes; mensagens maiores são descartadas
}

DispositivoBase::DispositivoBase(const ConfigDispositivo& config, const PinosSinalizacao& pinos)
    : _cfg(config), _mqtt(), _sinal(pinos), _ha(_mqtt) {}

// ---------------------------------------------------------------------------
// Ciclo de vida
// ---------------------------------------------------------------------------

void DispositivoBase::iniciar() {
  if (_cfg.anexarMac) {
    String mac = WiFi.macAddress();   // "A1:B2:C3:D4:E5:F6"
    mac.replace(":", "");
    mac.toLowerCase();
    if (_cfg.macCurto) mac = mac.substring(6);
    _cfg.nomeCliente += "-" + mac;
    _cfg.anexarMac = false;           // evita anexar duas vezes se iniciar() for chamado de novo
  }

  _sinal.iniciar();

  if (_cfg.otaServidor && _cfg.otaServidorSenha.length() == 0) {
    log("OTA servidor desativado: defina otaServidorSenha");
    _cfg.otaServidor = false;
  }

  montarTopicos();

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.hostname(_cfg.nomeCliente);
  WiFi.begin(_cfg.ssid.c_str(), _cfg.senhaWifi.c_str());

  configurarMqtt();

  HomeAssistantDiscovery::InfoDispositivo info;
  info.id = _cfg.nomeCliente;
  info.nome = _cfg.nomeHA.length() > 0 ? _cfg.nomeHA : _cfg.nomeCliente;
  info.modelo = _cfg.modelo;
  info.fabricante = _cfg.fabricante;
  info.versaoSw = _cfg.versaoSw;
  info.versaoHw = _cfg.versaoHw;
  info.topicoDisponibilidade = _topicoDisponibilidade;
  _ha.configurar(info, _cfg.prefixoHomeAssistant);
  _ha.habilitar(_cfg.homeAssistant);

  aoIniciar();
}

void DispositivoBase::loop() {
  const uint32_t agora = millis();

  gerenciarConexao(agora);
  _mqtt.loop();

#ifndef DISPOSITIVO_BASE_SEM_OTA
  if (_cfg.ota && wifiConectado()) {
    if (!_otaIniciado) {
      ArduinoOTA.setHostname(_cfg.nomeCliente.c_str());
      if (_cfg.senhaOta.length() > 0) ArduinoOTA.setPassword(_cfg.senhaOta.c_str());
      ArduinoOTA.begin();
      _otaIniciado = true;
    }
    ArduinoOTA.handle();
  }
#endif

  if (_cfg.otaServidor && wifiConectado()) {
    if (!_otaServidor.ativo()) {
      _otaServidor.iniciar(_cfg.otaServidorPorta, _cfg.otaServidorUsuario, _cfg.otaServidorSenha);
    }
    _otaServidor.loop();
  }

  if (_mqtt.connected()) {
    if (agora - _tEstado >= _cfg.intervaloEstadoMs) {
      publicarEstado();
    }
    if (agora - _tDisponibilidade >= _cfg.intervaloDisponibilidadeMs) {
      publicarDisponibilidade();
    }
  }

  if (agora - _tSensor >= _cfg.intervaloSensorMs) {
    _tSensor = agora;
    sensor();
  }

  _sinal.definirConexao(wifiConectado(), _mqtt.connected());
  _sinal.atualizar();
}

// ---------------------------------------------------------------------------
// Configuração
// ---------------------------------------------------------------------------

void DispositivoBase::montarTopicos() {
  _topicoDispositivo = _cfg.topicoBase + "/" + _cfg.nomeCliente;
  _topicoEstado = _topicoDispositivo;
  _topicoComando = _topicoDispositivo + "/cmd";
  _topicoDisponibilidade = _topicoDispositivo + "/available";
}

void DispositivoBase::configurarMqtt() {
  // O espMqttClient guarda apenas ponteiros: as Strings abaixo são membros
  // do objeto e não são alteradas depois de iniciar().
  _mqtt.setServer(_cfg.brokerHost.c_str(), _cfg.brokerPorta);
  _mqtt.setClientId(_cfg.nomeCliente.c_str());
  _mqtt.setKeepAlive(_cfg.keepAliveSeg);
  _mqtt.setCleanSession(true);
  _mqtt.setWill(_topicoDisponibilidade.c_str(), 1, true, "offline");

  if (_cfg.brokerUsuario.length() > 0) {
    _mqtt.setCredentials(_cfg.brokerUsuario.c_str(), _cfg.brokerSenha.c_str());
  }

  _mqtt.onConnect([this](bool sessaoExistente) { aoConectarMqtt(sessaoExistente); });
  _mqtt.onDisconnect([this](espMqttClientTypes::DisconnectReason motivo) { aoDesconectarMqtt(motivo); });
  _mqtt.onMessage([this](const espMqttClientTypes::MessageProperties&, const char* topico,
                         const uint8_t* payload, size_t tamanho, size_t indice, size_t total) {
    aoMensagemMqtt(topico, payload, tamanho, indice, total);
  });
}

void DispositivoBase::gerenciarConexao(uint32_t agora) {
  if (!wifiConectado()) return;
  if (!_mqtt.disconnected()) return;  // conectado ou em processo de conexão

  if (!_tentouConectar || agora - _tConexao >= _cfg.intervaloReconexaoMs) {
    _tentouConectar = true;
    _tConexao = agora;
    log("Conectando ao broker %s:%u...", _cfg.brokerHost.c_str(), _cfg.brokerPorta);
    _mqtt.connect();
  }
}

// ---------------------------------------------------------------------------
// Eventos MQTT
// ---------------------------------------------------------------------------

void DispositivoBase::aoConectarMqtt(bool sessaoExistente) {
  (void)sessaoExistente;
  log("MQTT conectado");

  _mqtt.subscribe(_topicoComando.c_str(), _cfg.qosComando);

  publicarDisponibilidade();
  sincronizarHomeAssistant();
  publicarEstado();

  aoConectar();
}

void DispositivoBase::aoDesconectarMqtt(espMqttClientTypes::DisconnectReason motivo) {
  log("MQTT desconectado: %s", espMqttClientTypes::disconnectReasonToString(motivo));
  _tConexao = millis();
  _tentouConectar = true;  // respeita o intervalo antes de tentar de novo
}

void DispositivoBase::aoMensagemMqtt(const char* topico, const uint8_t* payload, size_t tamanho,
                                     size_t indice, size_t total) {
  // O espMqttClient pode entregar a mensagem em vários pedaços.
  if (indice == 0) {
    _bufferRx = "";
    _descartarMensagem = total > kTamanhoMaximoComando;
    if (!_descartarMensagem) _bufferRx.reserve(total);
  }
  if (_descartarMensagem) return;

  for (size_t i = 0; i < tamanho; i++) {
    _bufferRx += static_cast<char>(payload[i]);
  }

  if (indice + tamanho >= total) {
    log("Mensagem em %s: %s", topico, _bufferRx.c_str());
    aoReceberMensagem(topico, _bufferRx);
  }
}

void DispositivoBase::aoReceberMensagem(const char* topico, const String& payload) {
  if (_topicoComando != topico) return;

  if (_cfg.otaHttp && payload.startsWith("UPDATE ")) {
    String url = payload.substring(7);
    url.trim();
    atualizarViaHttp(url);
    return;
  }

  atuador(payload);
}

bool DispositivoBase::atualizarViaHttp(const String& url) {
  if (!_cfg.otaHttp) return false;

  return _otaCliente.atualizar(url, _cfg.otaUrlPermitida, [this](const String& status) {
    log("OTA HTTP: %s", status.c_str());
    publicarEmDispositivo("ota", status);
    if (status == "iniciando") {
      _mqtt.loop();  // dá chance de enviar a mensagem antes de bloquear no download
      _sinal.corRgb(CorLed::Azul);
    } else if (status != "recusado") {
      _sinal.corRgb(CorLed::Desligado);
    }
  });
}

// ---------------------------------------------------------------------------
// Publicações padrão
// ---------------------------------------------------------------------------

void DispositivoBase::publicarDisponibilidade() {
  _tDisponibilidade = millis();
  publicar(_topicoDisponibilidade, "online", true, 1);
}

bool DispositivoBase::publicarEstado() {
  _tEstado = millis();

  JsonDocument doc;
  doc["id"] = WiFi.getHostname(); 
  JsonObject saude = doc["health"].to<JsonObject>();
  saude["RSSI"] = WiFi.RSSI();
  saude["heap"] = ESP.getFreeHeap();
  saude["stack"] = ESP.getFreeContStack();
  saude["uptime"] = millis() / 1000;
  montarEstado(doc);

  String payload;
  serializeJson(doc, payload);
  return publicar(_topicoEstado, payload);
}

void DispositivoBase::sincronizarHomeAssistant() {
  // Entidade padrão presente em todos os dispositivos: intensidade do sinal Wi-Fi.
  EntidadeHA rssi;
  rssi.componente = "sensor";
  rssi.idObjeto = "rssi";
  rssi.nome = "Sinal Wi-Fi";
  rssi.topicoEstado = _topicoEstado.c_str();
  rssi.templateValor = "{{ value_json.RSSI }}";
  rssi.classeDispositivo = "signal_strength";
  rssi.unidade = "dBm";
  rssi.classeEstado = "measurement";
  rssi.categoria = "diagnostic";
  _ha.publicar(rssi);

  configurarHomeAssistant(_ha);
}

// ---------------------------------------------------------------------------
// Ferramentas para as classes filhas
// ---------------------------------------------------------------------------

bool DispositivoBase::publicar(const String& topico, const String& payload, bool retain, uint8_t qos) {
  if (!_mqtt.connected()) return false;
  return _mqtt.publish(topico.c_str(), qos, retain, payload.c_str()) != 0;
}

bool DispositivoBase::publicarEmBase(const String& sufixo, const String& payload, bool retain) {
  return publicar(_cfg.topicoBase + "/" + sufixo, payload, retain);
}

bool DispositivoBase::publicarEmDispositivo(const String& sufixo, const String& payload, bool retain) {
  return publicar(_topicoDispositivo + "/" + sufixo, payload, retain);
}

bool DispositivoBase::assinar(const String& topico, uint8_t qos) {
  if (!_mqtt.connected()) return false;
  return _mqtt.subscribe(topico.c_str(), qos) != 0;
}

void DispositivoBase::log(const char* formato, ...) {
  if (!_depuracao) return;
  char buffer[160];
  va_list args;
  va_start(args, formato);
  vsnprintf(buffer, sizeof(buffer), formato, args);
  va_end(args);
  Serial.println(buffer);
}
