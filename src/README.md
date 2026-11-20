# DispositivoBase

Biblioteca base (ESP8266 + C++/PlatformIO) para dispositivos do campus Ceará-Mirim.
Usa **espMqttClient** para o MQTT e cria as entidades automaticamente no **Home Assistant**.

## Estrutura (padrão PlatformIO)

```
platformio.ini
include/
  DadosMqtt.h            credenciais (Wi-Fi, broker, nome, tópico base)
  FechaduraNFC.h         exemplo de classe filha
src/
  main.cpp
  FechaduraNFC.cpp
lib/
  DispositivoBase/       <- a biblioteca
    library.json
    src/
      DispositivoBase.h/.cpp          classe mãe abstrata (Wi-Fi, MQTT, ciclo de vida)
      SinalizacaoLed.h/.cpp           LED de conexão, LED RGB e buzzer (não bloqueante)
      HomeAssistantDiscovery.h/.cpp   MQTT Discovery do Home Assistant
  PN532/ PN532_I2C/      bibliotecas do leitor NFC (do projeto original)
```

## Criando um novo dispositivo

A classe filha implementa apenas **`atuador()`** e **`sensor()`**:

```cpp
class MeuDispositivo : public DispositivoBase {
 public:
  using DispositivoBase::DispositivoBase;

 protected:
  void atuador(const String& payload) override {   // comando em <base>/<nome>/cmd
    if (payload == "ON") digitalWrite(5, HIGH);
  }
  void sensor() override {                         // chamado a cada intervaloSensorMs
    publicarEmDispositivo("temperatura", String(lerTemperatura()));
  }
};
```

Ganchos opcionais (`override`): `aoIniciar()`, `aoConectar()`, `montarEstado(doc)`,
`configurarHomeAssistant(ha)`, `aoReceberMensagem(topico, payload)`.

Em `main.cpp`: `dispositivo.iniciar()` no `setup()` e `dispositivo.loop()` no `loop()`.

## Tópicos

| Tópico | Conteúdo |
|---|---|
| `<base>/<nome>` | estado em JSON (RSSI, heap, stack, uptime + campos da filha), periódico |
| `<base>/<nome>/cmd` | comandos recebidos (chamam `atuador()`) |
| `<base>/<nome>/available` | `online` (retido) / `offline` (LWT) |
| `<base>/<nome>/ota` | resultado da atualização por HTTP (`iniciando`, `recusado`, `sem atualizacao`, `falha: ...`) |
| `homeassistant/<componente>/<nome>/<id>/config` | descoberta do Home Assistant (retida) |

## Atualização de firmware (OTA)

- **ArduinoOTA** (`cfg.ota = true`, senha em `cfg.senhaOta`): envio direto pelo PlatformIO com
  `upload_protocol = espota` e `upload_port = <nome-do-cliente>.local`.
- **OTA por HTTP** (`cfg.otaUrlPermitida`, ou `URL_FIRMWARE_PERMITIDA` no `DadosMqtt.h`): publique
  `UPDATE http://<servidor>/firmware/firmware.bin` em `<base>/<nome>/cmd` e o dispositivo baixa o
  `.bin` e reinicia. Só são aceitas URLs que começam com o prefixo configurado (ex.:
  `http://192.168.0.10:8000/firmware/`); com o prefixo vazio o recurso fica desativado.
  O `.bin` fica em `.pio/build/esp12e/firmware.bin`; para servir: `python3 -m http.server 8000`.
  Atenção: HTTP não é criptografado nem autentica o servidor, use apenas em rede confiável e
  proteja o tópico `cmd` com usuário/senha ou ACL no broker. Compile com
  `-D DISPOSITIVO_BASE_SEM_OTA_HTTP` para remover o recurso do binário.

## Diferenças em relação ao código anterior (EspMQTTClient)

- `EspMQTTClient` → `espMqttClient`. Ele **não** gerencia Wi-Fi, OTA nem web updater: o Wi-Fi e a reconexão
  agora ficam na classe base e o OTA usa `ArduinoOTA` (`cfg.ota = true`). O web updater HTTP não foi portado.
- A disponibilidade agora é **por dispositivo** (`<base>/<nome>/available`), usada no LWT, na publicação
  periódica e no `availability` do Home Assistant (antes o LWT e o HA usavam `TOPIC_AVAILABLE`, um tópico global que
  podia não ser o mesmo em que `availableSignal()` publicava). `TOPIC_AVAILABLE` foi removido de `DadosMqtt.h`.
- O tópico de descoberta passou a incluir o `idObjeto` (`.../<nome>/porta/config`) e o `unique_id` é
  `<nome>_porta`. Ao migrar, remova no broker o tópico retido antigo `homeassistant/lock/<nome>/config`
  para não duplicar a entidade.
- A sinalização não usa mais `delay()`; o único bloqueio que sobrou é o pulso de 500 ms da fechadura.
- Dependências: ArduinoJson 7 (`JsonDocument`) e espMqttClient ≥ 1.7. As bibliotecas de `lib/` que eram
  submódulos (ArduinoJson, EspMQTTClient) agora vêm do `lib_deps`.

## Observação sobre memória

O espMqttClient recusa publicações se a memória livre estiver abaixo de `EMC_MIN_FREE_MEMORY`
(padrão 16384). No `platformio.ini` o valor foi reduzido para 8192 por causa do ESP8266.
