#include "RelogioNTP.h"
#include <time.h>

#ifdef ESP8266
#include <coredecls.h>   // settimeofday_cb
#endif

RelogioNTP* RelogioNTP::_instancia = nullptr;

void RelogioNTP::aoSincronizar() {
  RelogioNTP* r = _instancia;
  if (!r) return;

  // O relógio do sistema já foi ajustado quando este callback é chamado.
  const uint32_t agora = (uint32_t)time(nullptr);
  const uint32_t ms = millis();

  // Só a 1ª sincronização fixa o horário de ligação (testa _primeiraSync, não _ultimaSync).
  if (r->_primeiraSync == 0) {
    r->_atrasoPrimeiraSyncMs = ms;          // ms
    r->_inicio = agora - ms / 1000;         // s: converte ms para s antes de subtrair
    r->_primeiraSync = agora;
  }
  r->_ultimaSync = agora;
  r->_sincronizado = true;
}

void RelogioNTP::begin(const char* s1, const char* s2, const char* s3) {
  const char* entrada[MAX_SERVIDORES] = {s1, s2, s3};
  String copia[MAX_SERVIDORES];     // copia antes de mexer, pois a entrada pode apontar para _servidores
  uint8_t n = 0;
  for (uint8_t i = 0; i < MAX_SERVIDORES; i++) {
    if (entrada[i] && entrada[i][0] != '\0') copia[n++] = entrada[i];  // compacta, sem buracos
  }
  if (n == 0) copia[0] = "pool.ntp.br";
  for (uint8_t i = 0; i < MAX_SERVIDORES; i++) _servidores[i] = copia[i];

  _instancia = this;
  _sincronizado = false;          // a cada boot, tenta sincronizar de novo
  settimeofday_cb(RelogioNTP::aoSincronizar);
  // UTC; o SNTP começa quando houver rede e repete até conseguir.
  configTime(0, 0,
             _servidores[0].c_str(),
             _servidores[1].length() ? _servidores[1].c_str() : nullptr,
             _servidores[2].length() ? _servidores[2].c_str() : nullptr);
}

bool RelogioNTP::aguardarSincronizacao(uint32_t timeoutMs) {
  const uint32_t inicio = millis();
  while (!horarioValido() && (millis() - inicio) < timeoutMs) {
    delay(50);                    // delay() cede CPU ao Wi-Fi/lwIP
  }
  return horarioValido();
}

bool RelogioNTP::horarioValido() const {
  // O relógio do sistema continua contando sem NTP, então após a 1ª sincronização
  // este valor permanece válido mesmo com a rede fora do ar.
  return (uint32_t)time(nullptr) >= EPOCH_MINIMO_VALIDO;
}

uint32_t RelogioNTP::epoch() const {
  const uint32_t t = (uint32_t)time(nullptr);
  return t >= EPOCH_MINIMO_VALIDO ? t : 0;
}

bool RelogioNTP::formatarISO8601(uint32_t epoch, char* buf, size_t tam) {
  if (!buf || tam < TAM_ISO8601) {
    if (buf && tam) buf[0] = '\0';
    return false;
  }
  // Conversão dias -> data civil (algoritmo de H. Hinnant), sem gmtime/time_t.
  const uint32_t segDia = epoch % 86400UL;
  int32_t z = (int32_t)(epoch / 86400UL) + 719468;
  const int32_t era = z / 146097;                              // z >= 0 sempre
  const uint32_t doe = (uint32_t)(z - era * 146097);           // [0, 146096]
  const uint32_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  int32_t ano = (int32_t)yoe + era * 400;
  const uint32_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  const uint32_t mp = (5 * doy + 2) / 153;
  const uint32_t dia = doy - (153 * mp + 2) / 5 + 1;
  const uint32_t mes = mp < 10 ? mp + 3 : mp - 9;
  if (mes <= 2) ano++;

  snprintf(buf, tam, "%04d-%02u-%02uT%02u:%02u:%02uZ",
           (int)ano, (unsigned)mes, (unsigned)dia,
           (unsigned)(segDia / 3600), (unsigned)((segDia % 3600) / 60), (unsigned)(segDia % 60));
  return true;
}

bool RelogioNTP::horarioISO8601(char* buf, size_t tam) const {
  const uint32_t t = epoch();
  if (t == 0) {
    if (buf && tam) buf[0] = '\0';
    return false;
  }
  return formatarISO8601(t, buf, tam);
}

String RelogioNTP::horarioISO8601() const {
  char buf[TAM_ISO8601];
  return horarioISO8601(buf, sizeof(buf)) ? String(buf) : String();
}
