#include "factory_reset.h"
#include <Preferences.h>
#include <esp_system.h>
#include "crypto.h"

void do_factory_reset()
{
  Serial.println("FACTORY RESET: wiping NVS (keys + state) and rebooting...");

  Preferences prefs;

  // Borrar clave AES y cualquier dato asociado (namespace de criptografía)
  prefs.begin("lora_crypto", false);
  prefs.clear();
  prefs.end();

  // Borrar estado de protocolo (IV counter, paired, etc.), si existe
  prefs.begin("lora_proto", false);
  prefs.clear();
  prefs.end();

  // Borrar credenciales WiFi guardadas
  prefs.begin("wifi_cfg", false);
  prefs.clear();
  prefs.end();

  // Limpiar clave en RAM
  clear_aes_key();

  Serial.println("FACTORY RESET: done. Restarting...");
  delay(500);
  ESP.restart();
}