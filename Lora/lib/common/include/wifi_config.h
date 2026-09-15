#pragma once

#include <Arduino.h>

enum WifiConfigState
{
  WIFI_CFG_IDLE,
  WIFI_CFG_CONNECTING,
  WIFI_CFG_CONNECTED,
  WIFI_CFG_FAILED
};

/**
 * Guarda las credenciales WiFi en NVS e inicia la conexión STA.
 * Mantiene el AP activo usando WIFI_AP_STA.
 */
bool wifiConfigSaveAndConnect(const String &ssid, const String &password);

/**
 * Actualiza el estado de la conexión WiFi.
 * Debe llamarse periódicamente mientras se consulta el estado.
 */
void wifiConfigUpdate();

/**
 * Devuelve el estado actual de configuración/conexión.
 */
WifiConfigState wifiConfigGetState();

/**
 * Devuelve el SSID configurado durante esta sesión.
 */
String wifiConfigGetSsid();

/**
 * Convierte el estado a texto.
 */
const char *wifiConfigStateToString(WifiConfigState state);

/**
 * Carga las credenciales WiFi guardadas en NVS e intenta conectar.
 *
 * @param timeoutMs Tiempo máximo de espera.
 * @return true si conecta correctamente.
 */
bool wifiConfigConnectStored(unsigned long timeoutMs = 15000UL);