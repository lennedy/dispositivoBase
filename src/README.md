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
      RelogioNTP.h/.cpp               horário por NTP (UTC): string ISO 8601 e epoch
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

## LED RGB opcional

Nem toda placa tem LED RGB. Em `PinosSinalizacao`, `rgbHabilitado = false` desativa a
configuração dos pinos e o controle do LED (as chamadas de cor viram no-op) e, ao iniciar,
o log (Serial) informa: `LED RGB desabilitado: configuracao e controle do LED RGB ignorados`.
O padrão é `true`.

```cpp
PinosSinalizacao pinos;
pinos.rgbHabilitado = false;
FechaduraNFC dispositivo(cfg, pinos);
```

## Horário (NTP)

O `DispositivoBase` sincroniza o relógio por NTP (`RelogioNTP.h/.cpp`). O horário é sempre **UTC**.

- A sincronização é tentada **a cada boot** (não bloqueia o `iniciar()`): o SNTP começa quando o Wi-Fi
  conectar e repete até conseguir.
- Depois da primeira sincronização o horário **continua correndo** mesmo sem acesso ao servidor NTP.
  Como o ESP8266 não tem RTC, um reinício apaga o horário até a próxima sincronização.
- Servidores configuráveis em `ConfigDispositivo` (até 3, nome ou IP; deixe vazio o que não usar):

```cpp
cfg.ntpServidor1 = "ntp.meucampus.local";  // padrão: pool.ntp.br
cfg.ntpServidor2 = "a.st1.ntp.br";         // padrão
cfg.ntpServidor3 = "";                     // não usar
cfg.ntp = true;                            // false desativa o NTP
```

  Também é possível trocar em tempo de execução: `definirServidoresNtp("10.0.0.1", "pool.ntp.br")`.

| Método | Retorno |
|---|---|
| `horarioValido()` | `true` se já sincronizou desde o boot |
| `horarioEpoch()` | `uint32_t`, segundos desde 1970 (UTC); `0` se inválido |
| `horarioISO8601()` | `String` `"2026-11-17T16:30:00Z"`; vazia se inválido |
| `horarioISO8601(buf, tam)` | mesmo valor em buffer (`RelogioNTP::TAM_ISO8601` = 21 bytes), sem usar heap |
| `horarioStart()` | `uint32_t`, epoch (s, UTC) em que o dispositivo ligou; `0` até a 1ª sincronização |
| `atrasoPrimeiraSyncMs()` | `uint32_t`, **milissegundos** entre o boot e a 1ª sincronização; `0` até lá |
| `horarioUltimaSync()` | `uint32_t`, epoch (s, UTC) da última sincronização; `0` se nunca |
| `relogio()` | acesso a `aguardarSincronizacao(ms)`, `ultimaSincronizacao()` etc. |

O estado JSON publicado em `<base>/<nome>` ganha o campo `"timestamp"` (ISO 8601) quando o horário é válido.
Em `health`, `"uptime"` é o epoch (s) em que o dispositivo ligou e `"ntp_d"` é o tempo (ms) entre o boot e a
1ª sincronização; ambos valem `0` até a 1ª sincronização. O horário de ligação é calculado uma única vez
(1ª sincronização menos o tempo desde o boot) e tem precisão de cerca de 1 s.
O epoch é `uint32_t` (vale até 2106) e não `float`, que perde precisão com valores dessa grandeza.
Requer o core Arduino do ESP8266 3.0 ou superior.

## Tópicos

| Tópico | Conteúdo |
|---|---|
| `<base>/<nome>` | estado em JSON (RSSI, heap, stack, uptime = horário de ligação, ntp_d + campos da filha), periódico |
| `<base>/<nome>/cmd` | comandos recebidos (chamam `atuador()`) |
| `<base>/<nome>/available` | `online` (retido) / `offline` (LWT) |
| `<base>/<nome>/ota` | resultado da atualização por HTTP (`iniciando`, `recusado`, `sem atualizacao`, `falha: ...`) |
| `homeassistant/<componente>/<nome>/<id>/config` | descoberta do Home Assistant (retida) |

## Atualização de firmware (OTA)

- **ArduinoOTA** (`cfg.ota = true`, senha em `cfg.senhaOta`): envio direto pelo PlatformIO com
  `upload_protocol = espota` e `upload_port = <nome-do-cliente>.local`.
- **OTA por HTTP — cliente** (`cfg.otaHttp = true`, `cfg.otaUrlPermitida`, ou `URL_FIRMWARE_PERMITIDA` no `DadosMqtt.h`): publique
  `UPDATE http://<servidor>/firmware/firmware.bin` em `<base>/<nome>/cmd` e o dispositivo baixa o
  `.bin` e reinicia. Só são aceitas URLs que começam com o prefixo configurado (ex.:
  `http://192.168.0.10:8000/firmware/`); com o prefixo vazio o recurso fica desativado.
  O `.bin` fica em `.pio/build/esp12e/firmware.bin`; para servir: `python3 -m http.server 8000`.
  Atenção: HTTP não é criptografado nem autentica o servidor, use apenas em rede confiável e
  proteja o tópico `cmd` com usuário/senha ou ACL no broker. Compile com
  `-D DISPOSITIVO_BASE_SEM_OTA_HTTP` para remover o recurso do binário.
  Código em `OtaHttpCliente.h/.cpp`.
- **OTA por HTTP — servidor** (`cfg.otaServidor = true`, `cfg.otaServidorSenha` obrigatória; opcionais
  `otaServidorPorta`, padrão 80, e `otaServidorUsuario`, padrão `admin`): o dispositivo abre
  `http://<ip>:<porta>/update`, onde o `.bin` é enviado pelo navegador ou por
  `curl -u admin:senha -F "firmware=@.pio/build/esp12e/firmware.bin" http://<ip>/update`.
  Reinicia sozinho após o upload. Autenticação HTTP Basic sem criptografia: só em rede confiável.
  Código em `OtaHttpServidor.h/.cpp`.

Os modos cliente e servidor são independentes: habilite um, os dois ou nenhum.

## Diferenças em relação ao código anterior (EspMQTTClient)

- `EspMQTTClient` → `espMqttClient`. Ele **não** gerencia Wi-Fi, OTA nem web updater: o Wi-Fi e a reconexão
  agora ficam na classe base e o OTA usa `ArduinoOTA` (`cfg.ota = true`). O web updater HTTP agora existe como `OtaHttpServidor` (`cfg.otaServidor`).
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
