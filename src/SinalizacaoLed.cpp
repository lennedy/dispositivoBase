#include "SinalizacaoLed.h"

namespace {

const uint8_t kNotasSucesso = 3;
const uint8_t kNotasTentativa = 1;
const uint8_t kNotasFracasso = 1;

}  // namespace

SinalizacaoLed::SinalizacaoLed(const PinosSinalizacao& pinos) : _pinos(pinos) {}

void SinalizacaoLed::configurarSaida(int8_t pino) {
  if (pino >= 0) pinMode(pino, OUTPUT);
}

void SinalizacaoLed::iniciar() {
  configurarSaida(_pinos.ledConexao);
  if (_pinos.rgbHabilitado) {
    configurarSaida(_pinos.vermelho);
    configurarSaida(_pinos.verde);
    configurarSaida(_pinos.azul);
  }
  configurarSaida(_pinos.buzzer);

  escreverRgb(CorLed::Desligado);
  escreverLedConexao(false);
}

void SinalizacaoLed::atualizar() {
  const uint32_t agora = millis();
  atualizarLedConexao(agora);
  atualizarRgb(agora);
  atualizarSom(agora);
}

// ---------------------------------------------------------------------------
// LED de conexão
// ---------------------------------------------------------------------------

void SinalizacaoLed::definirConexao(bool wifiConectado, bool mqttConectado) {
  _wifi = wifiConectado;
  _mqtt = mqttConectado;
}

void SinalizacaoLed::escreverLedConexao(bool ligado) {
  _ledLigado = ligado;
  if (_pinos.ledConexao < 0) return;
  const bool nivel = _pinos.ledConexaoAtivoBaixo ? !ligado : ligado;
  digitalWrite(_pinos.ledConexao, nivel ? HIGH : LOW);
}

void SinalizacaoLed::atualizarLedConexao(uint32_t agora) {
  if (!_wifi) {
    escreverLedConexao(false);
  } else if (!_mqtt) {
    escreverLedConexao(true);
  } else if (passou(agora, _ultimoPisca + _intervaloPisca)) {
    _ultimoPisca = agora;
    escreverLedConexao(!_ledLigado);
  }
}

// ---------------------------------------------------------------------------
// LED RGB
// ---------------------------------------------------------------------------

void SinalizacaoLed::escreverRgb(CorLed cor) {
  if (!_pinos.rgbHabilitado) return;

  bool r = false, g = false, b = false;

  switch (cor) {
    case CorLed::Vermelho: r = true; break;
    case CorLed::Verde:    g = true; break;
    case CorLed::Azul:     b = true; break;
    case CorLed::Amarelo:  r = true; g = true; break;
    case CorLed::Magenta:  r = true; b = true; break;
    case CorLed::Ciano:    g = true; b = true; break;
    case CorLed::Branco:   r = true; g = true; b = true; break;
    case CorLed::Desligado:
    default: break;
  }

  auto escrever = [this](int8_t pino, bool ligado) {
    if (pino < 0) return;
    const bool nivel = _pinos.rgbAnodoComum ? !ligado : ligado;
    digitalWrite(pino, nivel ? HIGH : LOW);
  };

  escrever(_pinos.vermelho, r);
  escrever(_pinos.verde, g);
  escrever(_pinos.azul, b);
}

void SinalizacaoLed::corRgb(CorLed cor) {
  if (!_pinos.rgbHabilitado) return;
  _rgbTemporario = false;
  escreverRgb(cor);
}

void SinalizacaoLed::piscarCor(CorLed cor, uint32_t duracaoMs) {
  if (!_pinos.rgbHabilitado) return;
  escreverRgb(cor);
  _rgbTemporario = true;
  _rgbFim = millis() + duracaoMs;
}

void SinalizacaoLed::atualizarRgb(uint32_t agora) {
  if (_rgbTemporario && passou(agora, _rgbFim)) {
    _rgbTemporario = false;
    escreverRgb(CorLed::Desligado);
  }
}

// ---------------------------------------------------------------------------
// Sinalizações prontas
// ---------------------------------------------------------------------------

void SinalizacaoLed::sinalizarAcessoPermitido() {
  piscarCor(CorLed::Verde, 1000);
  somSucesso();
}

void SinalizacaoLed::sinalizarAcessoNegado() {
  piscarCor(CorLed::Vermelho, 1000);
  somFracasso();
}

// ---------------------------------------------------------------------------
// Buzzer
// ---------------------------------------------------------------------------

void SinalizacaoLed::somSucesso() {
  static const Nota notas[kNotasSucesso] = {{1000, 200}, {1500, 200}, {2000, 200}};
  iniciarSom(notas, kNotasSucesso);
}

void SinalizacaoLed::somTentativa() {
  static const Nota notas[kNotasTentativa] = {{1200, 100}};
  iniciarSom(notas, kNotasTentativa);
}

void SinalizacaoLed::somFracasso() {
  static const Nota notas[kNotasFracasso] = {{500, 500}};
  iniciarSom(notas, kNotasFracasso);
}

void SinalizacaoLed::iniciarSom(const Nota* notas, uint8_t quantidade) {
  if (_pinos.buzzer < 0 || quantidade == 0) return;
  _notas = notas;
  _quantidadeNotas = quantidade;
  _indiceNota = 0;
  _tocando = true;
  tocarNotaAtual(millis());
}

void SinalizacaoLed::tocarNotaAtual(uint32_t agora) {
  const Nota& nota = _notas[_indiceNota];
  tone(_pinos.buzzer, nota.frequencia);
  _fimNota = agora + nota.duracaoMs;
}

void SinalizacaoLed::atualizarSom(uint32_t agora) {
  if (!_tocando || !passou(agora, _fimNota)) return;

  _indiceNota++;
  if (_indiceNota >= _quantidadeNotas) {
    noTone(_pinos.buzzer);
    _tocando = false;
  } else {
    tocarNotaAtual(agora);
  }
}
