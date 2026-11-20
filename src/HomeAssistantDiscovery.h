#ifndef HOME_ASSISTANT_DISCOVERY_H
#define HOME_ASSISTANT_DISCOVERY_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <espMqttClient.h>

/**
 * Descrição de uma entidade do Home Assistant (MQTT Discovery).
 * Campos não usados ficam em nullptr e são omitidos do JSON.
 *
 * Os ponteiros só precisam ser válidos durante a chamada de publicar().
 */
struct EntidadeHA {
  const char* componente = nullptr;       // "lock", "switch", "sensor", "binary_sensor", "button"...
  const char* idObjeto = nullptr;         // identificador único dentro do dispositivo (ex.: "porta")
  const char* nome = nullptr;             // nullptr = entidade principal (usa o nome do dispositivo)

  const char* topicoEstado = nullptr;     // stat_t
  const char* topicoComando = nullptr;    // cmd_t
  const char* templateValor = nullptr;    // val_tpl (ex.: "{{ value_json.state }}")

  const char* classeDispositivo = nullptr;  // dev_cla (ex.: "signal_strength")
  const char* unidade = nullptr;            // unit_of_meas (ex.: "dBm")
  const char* classeEstado = nullptr;       // stat_cla (ex.: "measurement")
  const char* categoria = nullptr;          // ent_cat ("diagnostic" ou "config")
  const char* icone = nullptr;              // ic (ex.: "mdi:door")
};

/**
 * Publica as mensagens de MQTT Discovery para criar (ou remover)
 * automaticamente as entidades do dispositivo no Home Assistant.
 *
 * Tópico de descoberta:
 *   <prefixo>/<componente>/<idDispositivo>/<idObjeto>/config
 */
class HomeAssistantDiscovery {
 public:
  explicit HomeAssistantDiscovery(espMqttClient& mqtt);

  struct InfoDispositivo {
    String id;                      // identificador único (usado nos unique_id)
    String nome;                    // nome mostrado no Home Assistant
    String modelo = "ESP8266";
    String fabricante;
    String versaoSw;
    String versaoHw;
    String topicoDisponibilidade;   // tópico com "online"/"offline"
  };

  void configurar(const InfoDispositivo& info, const String& prefixo = "homeassistant");

  // true: publica as entidades; false: remove-as do Home Assistant.
  void habilitar(bool habilitado) { _habilitado = habilitado; }
  bool habilitado() const { return _habilitado; }

  /**
   * Cria a entidade (ou remove, se a descoberta estiver desabilitada).
   * `extras` permite acrescentar chaves específicas do componente
   * (ex.: payload_lock, state_on...) sem alterar a classe.
   */
  bool publicar(const EntidadeHA& entidade, const JsonDocument* extras = nullptr);

  // Remove a entidade do Home Assistant (payload vazio retido).
  bool remover(const char* componente, const char* idObjeto);

 private:
  String topicoConfig(const char* componente, const char* idObjeto) const;

  espMqttClient& _mqtt;
  InfoDispositivo _info;
  String _prefixo = "homeassistant";
  bool _habilitado = true;
};

#endif
