#ifndef OTA_HTTP_SERVIDOR_H
#define OTA_HTTP_SERVIDOR_H

#include <Arduino.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
#include <memory>

/**
 * OTA por HTTP — modo SERVIDOR.
 *
 * O dispositivo abre um servidor web e recebe o firmware (.bin) por upload:
 *
 *   - Navegador:  http://<ip-do-dispositivo>:<porta>/update  (formulário de envio)
 *   - Terminal:   curl -u usuario:senha -F "firmware=@firmware.bin" http://<ip>:<porta>/update
 *
 * Após o upload bem-sucedido o dispositivo reinicia sozinho.
 * Habilite com ConfigDispositivo::otaServidor = true (a senha é obrigatória).
 *
 * Atenção: a autenticação é HTTP Basic sem criptografia; use apenas em rede confiável.
 */
class OtaHttpServidor {
 public:
  /**
   * Abre o servidor. Chamar quando o Wi-Fi estiver conectado.
   * `nome` e `topicoStatus` são apenas exibidos na página de envio.
   */
  void iniciar(uint16_t porta, const String& usuario, const String& senha,
               const String& nome, const String& topicoStatus);

  /** Atende as requisições; chamar a cada loop() enquanto ativo(). */
  void loop();

  bool ativo() const { return _servidor != nullptr; }

 private:
  String paginaEnvio() const;

  std::unique_ptr<ESP8266WebServer> _servidor;
  String _usuario;
  String _senha;
  String _nome;
  String _topicoStatus;
  ESP8266HTTPUpdateServer _atualizador;
};

#endif
