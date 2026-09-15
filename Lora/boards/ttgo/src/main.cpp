#include <Arduino.h>
#include <LoRa.h>
#include <WiFi.h>
#include <esp_system.h>

#include <setup.h>
#include <protocol.h>
#include <crypto.h>
#include "ap_server.h"
#include "pair_status.h"
#include "factory_reset.h"
#include "gateway_time.h"

#include "device_id.h"
#include "lora_handler.h"
#include "wifi_config.h"

// ==================== Configuración de hardware ====================

#define FACTORY_RESET_PIN 38 // Pin físico para factory reset (a GND)

// ==================== Estado global interno ====================

// Control del apagado diferido del AP de configuración
static bool ap_should_stop = false;
static unsigned long ap_stop_at = 0;
static bool ap_off_scheduled = false;

// Estado de lógica de red / integración
static bool g_is_paired = false;    // Estado de emparejamiento (clave AES en NVS)

static bool g_time_started = false;


// ==================== Callback de emparejamiento ====================

/**
 * @brief Callback invocado cuando se completa el proceso de emparejamiento.
 *
 * Lo llama ap_server.cpp (pairTask) cuando:
 *  - Se deriva la clave desde la contraseña y se guarda en NVS (ok == true).
 *  - O cuando hay fallo de derivación/almacenamiento (ok == false).
 *
 * @param ok true si la derivación y almacenamiento de la clave han sido correctos.
 */
static void on_paired(bool ok)
{
  if (ok)
  {
    Serial.println("Pair callback: derivación OK");
    g_is_paired = true;
  }
  else
  {
    Serial.println("Pair callback: derivación FALLIDA");
  }
}

// ==================== Inicialización del dispositivo ====================

/**
 * @brief Inicialización principal del gateway.
 *
 * Flujo:
 *  1. Inicia puerto serie y pin de factory reset.
 *  2. Calcula DEVICE_ID a partir de la MAC.
 *  3. Intenta cargar la clave AES de NVS (emparejamiento).
 *  4. Intenta cargar credenciales WiFi de NVS y conectar en STA.
 *  5. Si no está emparejado o la WiFi no conecta -> arranca AP de configuración.
 *  6. Inicializa LoRa.
 *  7. Si hay WiFi conectada desde el arranque, inicia servidor HTTP para HA.
 */
void setup()
{
  Serial.begin(115200);
  delay(200);

  // Configurar pin de factory reset (pulsador a GND)
  pinMode(FACTORY_RESET_PIN, INPUT_PULLUP);

  // Obtener ID único del dispositivo (de device_id.cpp)
  DEVICE_ID = getDeviceIdFromChip();
  Serial.print("Gateway Device ID: ");
  Serial.println(DEVICE_ID);

  bool isPaired = false;

  // ---------------------------------------------------------------------------
  // ¿Tiene clave? (clave AES en NVS)
  // ---------------------------------------------------------------------------
  if (load_device_key_from_nvs())
  {
    Serial.println("Device key loaded from NVS -> PAIRED, AES_KEY listo");
    isPaired = true;
  }
  else
  {
    Serial.println("No device key in NVS -> NOT PAIRED");
    isPaired = false;
  }

  g_is_paired = isPaired;


  // ---------------------------------------------------------------------------
  // Intentar conectar a la WiFi guardada
  // ---------------------------------------------------------------------------
  bool wifiConnected = wifiConfigConnectStored();

  // ---------------------------------------------------------------------------
  // Si NO estoy emparejado o NO tengo WiFi conectada -> encender AP
  // ---------------------------------------------------------------------------
  if (!isPaired || !wifiConnected)
  {
    Serial.println("Arrancando AP de configuración (pair + WiFi)...");
    ap_set_pair_callback(on_paired);
    ap_start("LoRaSetupTTGO", "Config123!", 4);
  }
  else
  {
    Serial.println("Arrancando directamente en STA, sin AP");
  }

  // ---------------------------------------------------------------------------
  // Inicializar LoRa (radio)
  // ---------------------------------------------------------------------------
  lora_begin_basic();
  Serial.println("TTGO GATEWAY ROBUST MODE (BINARY V1)");

  // Inicializar handler de LoRa (buffer duplicados, etc.)
  lora_handler_init(DEVICE_ID);

  // ---------------------------------------------------------------------------
  // Si ya hay WiFi conectada desde el arranque, arrancar http_server ya
  // ---------------------------------------------------------------------------
  if (wifiConnected)
  {
    gateway_time_init();
    g_time_started = true;
  }
}

// ==================== Bucle principal ====================

/**
 * @brief Bucle principal del gateway.
 *
 * Responsabilidades:
 *  - Gestionar botón de factory reset.
 *  - Procesar recepción LoRa y enviar ACKs.
 *  - Detectar conexión WiFi STA y arrancar http_server cuando proceda.
 *  - Programar y ejecutar el apagado del AP tras conexión WiFi.
 *  - Atender peticiones HTTP para Home Assistant.
 */
void loop()
{
  static unsigned long pressedAt = 0;

  // ---------------------------------------------------------------------------
  // Gestión de factory reset (pulsación larga > 3 s)
  // ---------------------------------------------------------------------------
  if (digitalRead(FACTORY_RESET_PIN) == LOW)
  { // botón a GND
    if (pressedAt == 0)
    {
      pressedAt = millis();
    }
    else if (millis() - pressedAt > 3000)
    { // 3 s pulsado
      do_factory_reset();
    }
  }
  else
  {
    pressedAt = 0;
  }

  // ---------------------------------------------------------------------------
  // Recepción de paquetes LoRa
  // ---------------------------------------------------------------------------
  int pkt = LoRa.parsePacket();

  if (pkt)
  {
    uint8_t packet[LORA_MAX_PACKET_SIZE];
    size_t packetLen = 0;

    // Leer todos los bytes recibidos
    while (LoRa.available() && packetLen < LORA_MAX_PACKET_SIZE)
    {
      packet[packetLen++] = static_cast<uint8_t>(LoRa.read());
    }

    int rssi = LoRa.packetRssi();
    float snr = LoRa.packetSnr();

    if (packetLen == 0)
    {
      Serial.println("LoRa: paquete vacío recibido");
    }

    // -----------------------------------------------------
    // PROTOCOLO BINARIO V1
    // -----------------------------------------------------
    else if (packet[OFFSET_VERSION] == LORA_PROTOCOL_VERSION)
    {
      lora_handle_binary_packet(packet, packetLen, rssi, snr);
    }

    // -----------------------------------------------------
    // FORMATO DESCONOCIDO
    // -----------------------------------------------------
    else
    {
      Serial.printf("LoRa: formato desconocido, primer byte=0x%02X, longitud=%u\n", packet[0], (unsigned int)packetLen);
    }
  }

  // ---------------------------------------------------------------------------
  // Gestión de WiFi / HTTP / apagado del AP
  // ---------------------------------------------------------------------------

  // Detectar si la STA está conectada en este ciclo
  bool wifi_now_connected = (WiFi.status() == WL_CONNECTED);

  if (wifi_now_connected && !g_time_started)
  {
    Serial.println("WiFi STA conectada -> inicializando hora del gateway");

    gateway_time_init();
    g_time_started = true;
  }

  // Programar apagado del AP cuando la WiFi se conecta por primera vez
  if (g_is_paired && wifi_now_connected && !ap_off_scheduled && !ap_should_stop)
  {
    Serial.println("WiFi conectada correctamente. Programando apagado del AP en 15s...");
    ap_should_stop = true;
    ap_stop_at = millis() + 15000; // 15 s para que el usuario vea el estado en la web
  }

  // Apagado diferido del AP tras emparejamiento + WiFi OK
  if (ap_should_stop && millis() > ap_stop_at)
  {
    Serial.println("Apagando AP tras emparejamiento (timeout)");
    ap_stop();
    ap_should_stop = false;
    ap_off_scheduled = true;
  }

  // Pequeña espera para no saturar la CPU
  delay(10);
}