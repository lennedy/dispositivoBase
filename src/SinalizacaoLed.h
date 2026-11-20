#ifndef SINALIZACAO_LED_H
#define SINALIZACAO_LED_H

#include <Arduino.h>

/**
 * Cores possíveis do LED RGB.
 */
enum class CorLed : uint8_t {
  Desligado,
  Vermelho,
  Verde,
  Azul,
  Amarelo,
  Magenta,
  Ciano,
  Branco
};

/**
 * Pinos e polaridades usados na sinalização.
 * Use -1 em qualquer pino que não exista no hardware.
 * Os valores padrão são os do projeto da fechadura (ESP-12E).
 */
struct PinosSinalizacao {
  int8_t ledConexao = 2;    // LED de status da conexão (GPIO2 = LED azul da placa)
  int8_t vermelho   = 12;   // LED RGB - vermelho
  int8_t verde      = 14;   // LED RGB - verde
  int8_t azul       = 16;   // LED RGB - azul
  int8_t buzzer     = 15;   // Buzzer

  bool ledConexaoAtivoBaixo = true;  // true: LOW acende o LED de conexão
  bool rgbAnodoComum        = true;  // true: LOW acende cada cor do LED RGB
};

/**
 * Sinalização padrão dos dispositivos (LED de conexão, LED RGB e buzzer).
 *
 * Tudo é NÃO BLOQUEANTE: os métodos apenas agendam o efeito e
 * atualizar() (chamado a cada loop) executa o efeito no tempo certo.
 *
 * LED de conexão:
 *   - Wi-Fi desconectado ........ apagado
 *   - Wi-Fi ok, MQTT desconectado  aceso fixo
 *   - Wi-Fi e MQTT conectados ... piscando
 */
class SinalizacaoLed {
 public:
  explicit SinalizacaoLed(const PinosSinalizacao& pinos = PinosSinalizacao());

  void iniciar();
  void atualizar();

  // --- LED de conexão ---------------------------------------------------
  void definirConexao(bool wifiConectado, bool mqttConectado);
  void definirIntervaloPisca(uint32_t ms) { _intervaloPisca = ms; }

  // --- LED RGB ----------------------------------------------------------
  void corRgb(CorLed cor);                          // cor fixa
  void piscarCor(CorLed cor, uint32_t duracaoMs);   // cor por um tempo, depois apaga

  // --- Sinalizações prontas --------------------------------------------
  void sinalizarAcessoPermitido();
  void sinalizarAcessoNegado();

  // --- Buzzer -----------------------------------------------------------
  void somSucesso();
  void somTentativa();
  void somFracasso();

 private:
  struct Nota {
    uint16_t frequencia;
    uint16_t duracaoMs;
  };

  void configurarSaida(int8_t pino);
  void escreverRgb(CorLed cor);
  void escreverLedConexao(bool ligado);
  void iniciarSom(const Nota* notas, uint8_t quantidade);
  void tocarNotaAtual(uint32_t agora);

  void atualizarLedConexao(uint32_t agora);
  void atualizarRgb(uint32_t agora);
  void atualizarSom(uint32_t agora);

  static bool passou(uint32_t agora, uint32_t alvo) {
    return static_cast<int32_t>(agora - alvo) >= 0;  // seguro contra estouro do millis()
  }

  PinosSinalizacao _pinos;

  // conexão
  bool _wifi = false;
  bool _mqtt = false;
  bool _ledLigado = false;
  uint32_t _intervaloPisca = 1000;
  uint32_t _ultimoPisca = 0;

  // RGB temporário
  bool _rgbTemporario = false;
  uint32_t _rgbFim = 0;

  // buzzer
  const Nota* _notas = nullptr;
  uint8_t _quantidadeNotas = 0;
  uint8_t _indiceNota = 0;
  bool _tocando = false;
  uint32_t _fimNota = 0;
};

#endif
