#pragma once
#include <Arduino.h>

/**
 * Relógio sincronizado por NTP para ESP8266 (core Arduino >= 3.0).
 *
 * - begin() é não bloqueante: o SNTP do lwIP tenta sincronizar assim que o
 *   Wi-Fi conectar e repete a tentativa sozinho até conseguir.
 * - Depois da primeira sincronização o relógio interno continua contando
 *   mesmo sem conexão com o servidor NTP (o SNTP apenas o reajusta de tempos
 *   em tempos quando a rede está disponível).
 * - O horário é sempre UTC.
 * - O valor numérico é o Unix epoch em segundos, uint32_t (4 bytes, vale até 2106).
 *   Não use float: com 24 bits de mantissa ele perde precisão (~128 s) em 1,7e9.
 */
class RelogioNTP {
public:
  // 2025-01-01 00:00:00 UTC: qualquer valor menor é considerado "não sincronizado".
  static constexpr uint32_t EPOCH_MINIMO_VALIDO = 1735689600UL;
  // Tamanho do buffer para "AAAA-MM-DDTHH:MM:SSZ" + '\0'.
  static constexpr size_t TAM_ISO8601 = 21;

  static constexpr uint8_t MAX_SERVIDORES = 3;

  /**
   * Inicia o SNTP com até 3 servidores (nome ou IP). Não bloqueia.
   * Servidores nulos ou vazios são ignorados; sem nenhum válido, usa "pool.ntp.br".
   * Os nomes são copiados para o objeto (o lwIP guarda apenas o ponteiro).
   * Pode ser chamado de novo para trocar os servidores em tempo de execução.
   */
  void begin(const char* servidor1 = "pool.ntp.br",
             const char* servidor2 = "a.st1.ntp.br",
             const char* servidor3 = "time.google.com");

  /** Servidor configurado na posição i (0..2); "" se não usado. */
  const char* servidor(uint8_t i) const { return i < MAX_SERVIDORES ? _servidores[i].c_str() : ""; }

  /** Bloqueia até sincronizar ou estourar o tempo. Opcional, útil no boot. */
  bool aguardarSincronizacao(uint32_t timeoutMs);

  /** true se o horário é confiável (já sincronizou ao menos uma vez desde o boot). */
  bool horarioValido() const;

  /** true se a última tentativa/ajuste veio do NTP desde o boot. */
  bool sincronizadoDesdeBoot() const { return _sincronizado; }

  /** Unix epoch (s, UTC). Retorna 0 se ainda não houver horário válido. */
  uint32_t epoch() const;

  /** Epoch da última sincronização bem-sucedida (0 se nunca). */
  uint32_t ultimaSincronizacao() const { return _ultimaSync; }

  /** Escreve "2026-11-17T16:30:00Z" em buf. Retorna false (e buf="") se sem horário válido. */
  bool horarioISO8601(char* buf, size_t tam) const;

  /** Conveniência (usa heap). Retorna "" se sem horário válido. */
  String horarioISO8601() const;

  /** Formata um epoch qualquer em ISO 8601 UTC. Função pura, sem dependência de hardware. */
  static bool formatarISO8601(uint32_t epoch, char* buf, size_t tam);

private:
  static void aoSincronizar();     // callback do SNTP (contexto lwIP: manter mínimo)
  static RelogioNTP* _instancia;

  String _servidores[MAX_SERVIDORES];
  volatile bool _sincronizado = false;
  volatile uint32_t _ultimaSync = 0;
};
