#include "OtaHttpServidor.h"

namespace {
// Evita que caracteres especiais do nome/tópico quebrem o HTML.
String escapar(const String& texto) {
  String saida;
  saida.reserve(texto.length() + 8);
  for (size_t i = 0; i < texto.length(); i++) {
    const char c = texto[i];
    if (c == '&') saida += F("&amp;");
    else if (c == '<') saida += F("&lt;");
    else if (c == '>') saida += F("&gt;");
    else if (c == '"') saida += F("&quot;");
    else saida += c;
  }
  return saida;
}
}  // namespace

void OtaHttpServidor::iniciar(uint16_t porta, const String& usuario, const String& senha,
                              const String& nome, const String& topicoStatus) {
  if (_servidor) return;

  _usuario = usuario;
  _senha = senha;
  _nome = nome;
  _topicoStatus = topicoStatus;

  _servidor.reset(new ESP8266WebServer(porta));
  ESP8266WebServer* servidor = _servidor.get();

  // Página própria de envio. Precisa ser registrada ANTES do _atualizador.setup():
  // o servidor usa o primeiro handler que casa, então esta GET /update vem antes da
  // página padrão. O POST (upload do .bin) continua tratado pelo _atualizador.
  _servidor->on("/update", HTTP_GET, [this, servidor]() {
    if (!servidor->authenticate(_usuario.c_str(), _senha.c_str())) {
      return servidor->requestAuthentication();
    }
    servidor->send(200, "text/html", paginaEnvio());
  });

  _atualizador.setup(servidor, "/update", usuario, senha);

  // Atalho: a raiz leva ao formulário de envio.
  _servidor->on("/", HTTP_GET, [servidor]() {
    servidor->sendHeader("Location", "/update");
    servidor->send(302);
  });

  _servidor->begin();
}

String OtaHttpServidor::paginaEnvio() const {
  String html;
  html.reserve(900);
  html += F("<!DOCTYPE html><html><head><meta charset='utf-8'>"
            "<meta name='viewport' content='width=device-width,initial-scale=1'>"
            "<title>Atualizar firmware</title></head><body>"
            "<h2>Atualizar firmware</h2>"
            "<p><b>Dispositivo:</b> ");
  html += escapar(_nome);
  html += F("<br><b>T&oacute;pico de status:</b> ");
  html += escapar(_topicoStatus);
  html += F("</p>"
            "<form method='POST' action='/update' enctype='multipart/form-data'>"
            "<input type='file' name='firmware' accept='.bin' required> "
            "<input type='submit' value='Enviar'>"
            "</form></body></html>");
  return html;
}

void OtaHttpServidor::loop() {
  if (_servidor) _servidor->handleClient();
}
