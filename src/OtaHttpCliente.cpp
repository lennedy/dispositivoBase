#include "OtaHttpCliente.h"

#ifndef DISPOSITIVO_BASE_SEM_OTA_HTTP
#include <ESP8266WiFi.h>
#include <ESP8266httpUpdate.h>
#endif

bool OtaHttpCliente::atualizar(const String& url, const String& urlPermitida, const Status& status) {
#ifdef DISPOSITIVO_BASE_SEM_OTA_HTTP
  (void)url;
  (void)urlPermitida;
  (void)status;
  return false;
#else
  // Aceita apenas URLs do servidor de firmware configurado.
  String permitida = urlPermitida;
  if (permitida.length() == 0 || !url.startsWith("http://")) {
    status("recusado");
    return false;
  }
  if (!permitida.endsWith("/")) permitida += "/";  // evita "http://host" casar com "http://host.outro.com"
  if (!url.startsWith(permitida)) {
    status("recusado");
    return false;
  }

  status("iniciando");

  WiFiClient cliente;
  ESPhttpUpdate.rebootOnUpdate(true);
  const t_httpUpdate_return resultado = ESPhttpUpdate.update(cliente, url);

  // Só chega aqui se NÃO reiniciou (falha ou nada a atualizar).
  if (resultado == HTTP_UPDATE_NO_UPDATES) {
    status("sem atualizacao");
  } else {
    status("falha: " + ESPhttpUpdate.getLastErrorString());
  }
  return false;
#endif
}
