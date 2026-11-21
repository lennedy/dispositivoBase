#ifndef OTA_HTTP_CLIENTE_H
#define OTA_HTTP_CLIENTE_H

#include <Arduino.h>
#include <functional>

/**
 * OTA por HTTP — modo CLIENTE.
 *
 * O dispositivo baixa o firmware (.bin) de um servidor externo. Normalmente é
 * disparado pelo comando MQTT "UPDATE <url>" (ver DispositivoBase).
 *
 * Habilite com ConfigDispositivo::otaHttp = true e defina otaUrlPermitida.
 * Para não compilar este modo: -D DISPOSITIVO_BASE_SEM_OTA_HTTP
 */
class OtaHttpCliente {
 public:
  /** Recebe o andamento: "recusado", "iniciando", "sem atualizacao" ou "falha: <motivo>". */
  using Status = std::function<void(const String&)>;

  /**
   * Baixa o firmware de `url` e reinicia se a atualização der certo.
   * Só aceita URLs que começam com `urlPermitida` (vazio = nenhuma).
   * Retorna false se a URL for recusada ou a atualização falhar.
   * Bloqueia durante o download.
   */
  bool atualizar(const String& url, const String& urlPermitida, const Status& status);
};

#endif
