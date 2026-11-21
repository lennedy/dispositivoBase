#include "OtaHttpServidor.h"

void OtaHttpServidor::iniciar(uint16_t porta, const String& usuario, const String& senha) {
  if (_servidor) return;

  _servidor.reset(new ESP8266WebServer(porta));
  _atualizador.setup(_servidor.get(), "/update", usuario, senha);

  // Atalho: a raiz leva ao formulário de envio.
  ESP8266WebServer* servidor = _servidor.get();
  _servidor->on("/", HTTP_GET, [servidor]() {
    servidor->sendHeader("Location", "/update");
    servidor->send(302);
  });

  _servidor->begin();
}

void OtaHttpServidor::loop() {
  if (_servidor) _servidor->handleClient();
}
