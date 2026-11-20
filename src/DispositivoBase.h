#ifndef DISPOSITIVO_BASE_H
#define DISPOSITIVO_BASE_H

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ArduinoJson.h>
#include <espMqttClient.h>

#include "SinalizacaoLed.h"
#include "HomeAssistantDiscovery.h"

/**
 * Configuração de um dispositivo.
 * Todas as strings são copiadas para dentro do objeto.
 */
struct ConfigDispositivo {
  // Wi-Fi
  String ssid;
  String senhaWifi;

  // Broker MQTT
  String brokerHost;
  uint16_t brokerPorta = 1883;
  String brokerUsuario;
  String brokerSenha;

  // Identificação / tópicos
  String nomeCliente;   // ClientId MQTT, hostname e id do dispositivo no Home Assistant
  String topicoBase;    // ex.: "campus/portas"

  // Home Assistant
  bool homeAssistant = true;          // true: cria as entidades; false: remove
  String prefixoHomeAssistant = "homeassistant";
  String nomeHA;                      // nome do dispositivo no HA (padrão: nomeCliente)
  String modelo = "ESP8266";
  String fabricante;
  String versaoSw = "1.0.0";
  String versaoHw = "1.0";

  // Temporização (ms)
  uint32_t intervaloEstadoMs = 20000;          // publicação do estado (JSON)
  uint32_t intervaloDisponibilidadeMs = 60000; // publicação de "online"
  uint32_t intervaloSensorMs = 5000;           // chamada de sensor()
  uint32_t intervaloReconexaoMs = 5000;        // tentativas de conexão ao broker

  // MQTT
  uint16_t keepAliveSeg = 30;
  uint8_t qosComando = 0;

  // OTA (ArduinoOTA)
  bool ota = false;
  String senhaOta;

  // OTA por HTTP: o dispositivo baixa o firmware (.bin) de um servidor ao receber
  // o comando "UPDATE <url>" em <base>/<nome>/cmd.
  // Só são aceitas URLs que começam com otaUrlPermitida (ex.: "http://192.168.0.10/firmware/").
  // Com otaUrlPermitida vazio, nenhuma URL é aceita.
  bool otaHttp = false;
  String otaUrlPermitida;
};

/**
 * Classe mãe abstrata de todos os dispositivos do campus.
 *
 * Implementa:
 *   - conexão Wi-Fi e reconexão automática;
 *   - comunicação MQTT (espMqttClient): conexão, LWT, disponibilidade,
 *     publicação periódica de estado e recebimento de comandos;
 *   - sinalização padrão (LED de conexão, LED RGB e buzzer);
 *   - criação automática das entidades no Home Assistant;
 *   - OTA opcional.
 *
 * As classes filhas implementam apenas:
 *   - atuador(payload): executa um comando recebido em <base>/<nome>/cmd;
 *   - sensor(): lê o(s) sensor(es) e publica o que for necessário.
 *
 * Tópicos:
 *   <base>/<nome>            estado (JSON)
 *   <base>/<nome>/cmd        comandos
 *   <base>/<nome>/available  "online" / "offline" (LWT)
 *   <base>/<nome>/ota        resultado da atualização por HTTP
 */
class DispositivoBase {
 public:
  explicit DispositivoBase(const ConfigDispositivo& config,
                           const PinosSinalizacao& pinos = PinosSinalizacao());
  virtual ~DispositivoBase() = default;

  DispositivoBase(const DispositivoBase&) = delete;
  DispositivoBase& operator=(const DispositivoBase&) = delete;

  // Chamar em setup() (depois de Serial.begin) e loop(), respectivamente.
  void iniciar();
  void loop();

  void habilitarDepuracao(bool habilitado) { _depuracao = habilitado; }

  bool wifiConectado() const { return WiFi.status() == WL_CONNECTED; }
  bool mqttConectado() { return _mqtt.connected(); }

  // Publica o estado imediatamente (além da publicação periódica).
  bool publicarEstado();

  /**
   * Baixa o firmware da URL (HTTP) e reinicia se a atualização der certo.
   * Retorna false se a URL for recusada ou a atualização falhar.
   * O resultado também é publicado em <base>/<nome>/ota.
   * Bloqueia durante o download.
   */
  bool atualizarViaHttp(const String& url);

 protected:
  // ----- Implementados pelas classes filhas -------------------------------

  /** Executa um comando recebido no tópico <base>/<nome>/cmd. */
  virtual void atuador(const String& payload) = 0;

  /** Lê o(s) sensor(es). Chamado a cada config.intervaloSensorMs. */
  virtual void sensor() = 0;

  // ----- Ganchos opcionais ------------------------------------------------

  /** Chamado ao final de iniciar(): inicialize aqui o hardware da filha. */
  virtual void aoIniciar() {}

  /** Chamado a cada conexão (ou reconexão) ao broker, após o setup padrão. */
  virtual void aoConectar() {}

  /** Acrescenta campos ao JSON de estado publicado periodicamente. */
  virtual void montarEstado(JsonDocument& doc) { (void)doc; }

  /** Cria as entidades do dispositivo no Home Assistant. */
  virtual void configurarHomeAssistant(HomeAssistantDiscovery& ha) { (void)ha; }

  /**
   * Mensagem recebida em qualquer tópico assinado.
   * Por padrão, no tópico de comando trata "UPDATE <url>" (se otaHttp estiver
   * habilitado) e encaminha os demais comandos para atuador().
   */
  virtual void aoReceberMensagem(const char* topico, const String& payload);

  // ----- Ferramentas para as classes filhas -------------------------------

  bool publicar(const String& topico, const String& payload, bool retain = false, uint8_t qos = 0);
  bool publicarEmBase(const String& sufixo, const String& payload, bool retain = false);        // <base>/<sufixo>
  bool publicarEmDispositivo(const String& sufixo, const String& payload, bool retain = false); // <base>/<nome>/<sufixo>
  bool assinar(const String& topico, uint8_t qos = 0);

  SinalizacaoLed& sinalizacao() { return _sinal; }
  HomeAssistantDiscovery& homeAssistant() { return _ha; }
  espMqttClient& mqtt() { return _mqtt; }
  const ConfigDispositivo& config() const { return _cfg; }

  const String& nomeCliente() const { return _cfg.nomeCliente; }
  const String& topicoBase() const { return _cfg.topicoBase; }
  const String& topicoEstado() const { return _topicoEstado; }
  const String& topicoComando() const { return _topicoComando; }
  const String& topicoDisponibilidade() const { return _topicoDisponibilidade; }

  void log(const char* formato, ...);

 private:
  void montarTopicos();
  void configurarMqtt();
  void gerenciarConexao(uint32_t agora);
  void aoConectarMqtt(bool sessaoExistente);
  void aoDesconectarMqtt(espMqttClientTypes::DisconnectReason motivo);
  void aoMensagemMqtt(const char* topico, const uint8_t* payload, size_t tamanho,
                      size_t indice, size_t total);
  void publicarDisponibilidade();
  void sincronizarHomeAssistant();

  ConfigDispositivo _cfg;
  espMqttClient _mqtt;
  SinalizacaoLed _sinal;
  HomeAssistantDiscovery _ha;

  String _topicoDispositivo;
  String _topicoEstado;
  String _topicoComando;
  String _topicoDisponibilidade;

  String _bufferRx;
  bool _descartarMensagem = false;

  uint32_t _tEstado = 0;
  uint32_t _tDisponibilidade = 0;
  uint32_t _tSensor = 0;
  uint32_t _tConexao = 0;
  bool _tentouConectar = false;
  bool _otaIniciado = false;
  bool _depuracao = false;
};

#endif
