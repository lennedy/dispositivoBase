#include "HomeAssistantDiscovery.h"

HomeAssistantDiscovery::HomeAssistantDiscovery(espMqttClient& mqtt) : _mqtt(mqtt) {}

void HomeAssistantDiscovery::configurar(const InfoDispositivo& info, const String& prefixo) {
  _info = info;
  _prefixo = prefixo;
}

String HomeAssistantDiscovery::topicoConfig(const char* componente, const char* idObjeto) const {
  String topico = _prefixo;
  topico += "/";
  topico += componente;
  topico += "/";
  topico += _info.id;
  topico += "/";
  topico += idObjeto;
  topico += "/config";
  return topico;
}

bool HomeAssistantDiscovery::remover(const char* componente, const char* idObjeto) {
  if (!componente || !idObjeto) return false;
  const String topico = topicoConfig(componente, idObjeto);
  return _mqtt.publish(topico.c_str(), 1, true, "") != 0;
}

bool HomeAssistantDiscovery::publicar(const EntidadeHA& e, const JsonDocument* extras) {
  if (!e.componente || !e.idObjeto) return false;

  // Descoberta desabilitada: remove a entidade do Home Assistant.
  if (!_habilitado) return remover(e.componente, e.idObjeto);

  JsonDocument doc;

  if (e.nome) {
    doc["name"] = e.nome;
  } else {
    doc["name"] = nullptr;
  }

  doc["uniq_id"] = _info.id + "_" + e.idObjeto;

  if (e.topicoEstado)      doc["stat_t"] = e.topicoEstado;
  if (e.topicoComando)     doc["cmd_t"] = e.topicoComando;
  if (e.templateValor)     doc["val_tpl"] = e.templateValor;
  if (e.classeDispositivo) doc["dev_cla"] = e.classeDispositivo;
  if (e.unidade)           doc["unit_of_meas"] = e.unidade;
  if (e.classeEstado)      doc["stat_cla"] = e.classeEstado;
  if (e.categoria)         doc["ent_cat"] = e.categoria;
  if (e.icone)             doc["ic"] = e.icone;

  if (_info.topicoDisponibilidade.length() > 0) {
    doc["avty_t"] = _info.topicoDisponibilidade;
    doc["pl_avail"] = "online";
    doc["pl_not_avail"] = "offline";
  }

  JsonObject dispositivo = doc["dev"].to<JsonObject>();
  dispositivo["ids"] = _info.id;
  dispositivo["name"] = _info.nome;
  dispositivo["mdl"] = _info.modelo;
  if (_info.fabricante.length() > 0) dispositivo["mf"] = _info.fabricante;
  if (_info.versaoSw.length() > 0)   dispositivo["sw"] = _info.versaoSw;
  if (_info.versaoHw.length() > 0)   dispositivo["hw"] = _info.versaoHw;

  if (extras) {
    for (JsonPairConst par : extras->as<JsonObjectConst>()) {
      doc[par.key()] = par.value();
    }
  }

  String payload;
  serializeJson(doc, payload);

  const String topico = topicoConfig(e.componente, e.idObjeto);
  return _mqtt.publish(topico.c_str(), 1, true, payload.c_str()) != 0;
}
