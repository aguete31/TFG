#include <Arduino.h>
#include <LoRa.h>
#include <setup.h>
#include <protocol.h>
#include <crypto.h>
#include "ap_server.h"
#include "pair_status.h"
#include <esp_system.h>
#include <Preferences.h>
#include "factory_reset.h"
#include "sensor_dht.h"

// =========================================================
// CONFIGURACIÓN GENERAL
// =========================================================

// Botón PRG del Heltec para hacer factory reset
#define FACTORY_RESET_PIN 0

// Parámetros del sistema de reintentos y temporización
#define MAX_RETRIES 3    // Número máximo de intentos para enviar un mensaje
#define ACK_TIMEOUT 1500 // Tiempo de espera para un ACK (ms)
#define TX_INTERVAL 5000 // Intervalo entre envíos cuando no hay ACK pendiente (ms)

// =========================================================
// VARIABLES GLOBALES DEL SISTEMA
// =========================================================

// Secuencia global de mensajes (incrementa en cada envío correcto)
uint32_t seq = 0;

// Tiempo del último envío
unsigned long lastTx = 0;

// Control espera de ACK
bool waitingAck = false;
uint32_t currentSeq = 0;
uint32_t retryCount = 0;
unsigned long ackTimeout = 0;

// Payload en reintentos
String currentPayload = "";

// Estado de transmisión según emparejamiento
static volatile bool tx_enabled = false;

// Control apagado diferido del AP
static bool ap_should_stop = false;
static unsigned long ap_stop_at = 0;

// Gestión del botón de reset
bool pressed = false;
unsigned long pressStart = 0;

// ID del dispositivo (basado en MAC)
String DEVICE_ID;

// =========================================================
// FUNCIONES AUXILIARES
// =========================================================

/**
 * @brief Obtiene un ID único basado en la MAC del ESP32.
 */
String getDeviceIdFromChip()
{
  uint64_t chipId = ESP.getEfuseMac();
  char idStr[13];
  sprintf(idStr, "%012llX", chipId);
  return String(idStr);
}

/**
 * @brief Callback llamado tras derivación de clave (proceso de pairing).
 *
 * Si ok == true: emparejado → habilitar transmisión.
 */
static void on_paired(bool ok)
{
  if (ok)
  {
    Serial.println("Pair callback: OK → habilitando TX");
    tx_enabled = true;
    pairStatus = WAITING;
  }
  else
  {
    Serial.println("Pair callback: FALLÓ");
    tx_enabled = false;
    pairStatus = FAILED;
  }
}

/**
 * @brief Envío de mensaje LoRa usando el protocolo robusto.
 *
 * Activa la espera de ACK y guarda el estado para posibles reintentos.
 */
void sendMessage(const String &payload)
{
  currentPayload = payload;

  // Crear mensaje JSON con protocolo
  String jsonStr = LoRaProtocol::createDataMessage(currentSeq, retryCount, DEVICE_ID, payload);

  LoRa.beginPacket();
  LoRa.print(jsonStr);
  LoRa.endPacket();

  Serial.printf("TX [%d] retry=%d: %s\n", currentSeq, retryCount, payload.c_str());

  waitingAck = true;
  ackTimeout = millis() + ACK_TIMEOUT;
  lastTx = millis();
}

/**
 * @brief Gestión del timeout cuando no se recibe ACK dentro del tiempo esperado.
 *
 * Reintenta si no se ha alcanzado MAX_RETRIES; si se alcanza, se declara fallo.
 */
void handleTimeout()
{
  retryCount++;

  // Se agotaron los reintentos
  if (retryCount >= MAX_RETRIES)
  {
    Serial.printf("FAIL [%d]: Max retries reached\n", currentSeq);
    waitingAck = false;
    retryCount = 0;
    seq++;

    // Durante emparejamiento → error crítico
    if (pairStatus != PAIRED)
    {
      Serial.println("Pairing failed: no ACK during pairing");
      tx_enabled = false;
      pairStatus = FAILED;
    }
    return;
  }

  // Reintentar
  Serial.printf("TIMEOUT [%d]: retry...\n", currentSeq);
  sendMessage(currentPayload);
}

/**
 * @brief Procesa un ACK recibido desde el gateway.
 *
 * Verifica seq y estado del protocolo, habilita transmisión y controla apagado del AP.
 */
void handleAckReceived(const String &ackStr)
{
  LoRaAck ack;

  // Parsear ACK usando protocolo robusto
  if (!LoRaProtocol::parseAck(ackStr, ack))
    return;

  // Validar seq
  if (ack.type == "ack" && ack.seq == currentSeq)
  {

    Serial.printf("ACK OK [%d]\n", ack.seq);

    waitingAck = false;
    retryCount = 0;
    seq++;

    bool firstPair = (pairStatus != PAIRED);
    tx_enabled = true;

    // Primer emparejamiento → programar apagado AP
    if (firstPair)
    {
      pairStatus = PAIRED;
      ap_should_stop = true;
      ap_stop_at = millis() + 5000;
      Serial.println("Pair OK → apagado AP en 5s");
    }
  }
  else
  {
    Serial.printf("ACK mismatch: recv=%d expect=%d\n", ack.seq, currentSeq);
  }
}

// =========================================================
// SETUP PRINCIPAL
// =========================================================

/**
 * @brief Inicialización general del nodo Heltec TX.
 *
 * - Configura el sensor DHT11
 * - Carga clave AES (si existe)
 * - Arranca AP si no hay clave
 * - Inicia LoRa
 */
void setup()
{
  Serial.begin(115200);
  delay(200);

  // Inicializar módulo de sensor
  dht_init();

  // Configurar botón PRG
  pinMode(FACTORY_RESET_PIN, INPUT_PULLUP);

  // Identificador único del nodo
  DEVICE_ID = getDeviceIdFromChip();
  Serial.println("Device ID: " + DEVICE_ID);

  // Intentar cargar clave desde NVS
  if (load_device_key_from_nvs())
  {
    Serial.println("Clave cargada → PAIRED");
    tx_enabled = true;
    pairStatus = PAIRED;
  }
  else
  {
    // No hay clave → iniciar AP de configuración
    Serial.println("No key → start AP for provisioning");
    pairStatus = IDLE;
    ap_set_pair_callback(on_paired);
    ap_start("LoRaSetupHeltec", "Config123!", 4);
  }

  // Inicializar LoRa
  lora_begin_basic();
  Serial.println("HELTEC TX ROBUST MODE (JSON)");
}

// =========================================================
// LOOP PRINCIPAL
// =========================================================

/**
 * @brief Bucle principal:
 *
 * - Lee botón de reseteo largo
 * - Espera emparejamiento
 * - Envía mensajes periódicos (JSON)
 * - Gestiona recepción de ACKs y reintentos
 * - Apaga AP cuando procede
 */
void loop()
{

  // -----------------------------------------------------
  // FACTORY RESET (pulsación larga 3s en PRG)
  // -----------------------------------------------------
  if (digitalRead(FACTORY_RESET_PIN) == LOW)
  {
    if (!pressed)
    {
      pressed = true;
      pressStart = millis();
    }

    if (millis() - pressStart > 3000)
    {
      Serial.println("Factory reset triggered!");
      do_factory_reset();
    }
  }
  else
  {
    pressed = false;
  }

  // -----------------------------------------------------
  // Esperar emparejamiento antes de transmitir
  // -----------------------------------------------------
  if (!is_key_ready() || !tx_enabled)
  {
    delay(10);
    return;
  }

  // -----------------------------------------------------
  // Envío periódico si no estamos esperando ACK
  // -----------------------------------------------------
  if (!waitingAck && millis() - lastTx > TX_INTERVAL)
  {

    currentSeq = seq;
    retryCount = 0;

    // === Leer sensor DHT11 ===
    float temp, hum;
    bool ok = dht_read(temp, hum);

    if (!ok)
    {
      Serial.println("Error leyendo DHT11");
      temp = hum = -1;
    }

    // === Construir payload JSON ===
    StaticJsonDocument<128> jdoc;
    jdoc["temp"] = temp;
    jdoc["hum"] = hum;
    jdoc["ts"] = millis() / 1000;

    String payload;
    serializeJson(jdoc, payload);

    // Enviar por LoRa
    sendMessage(payload);
  }

  // -----------------------------------------------------
  // Timeout de ACK
  // -----------------------------------------------------
  if (waitingAck && millis() > ackTimeout)
  {
    handleTimeout();
  }

  // -----------------------------------------------------
  // Recepción de ACK por LoRa
  // -----------------------------------------------------
  int pkt = LoRa.parsePacket();
  if (pkt)
  {
    String ackStr;
    while (LoRa.available())
      ackStr += (char)LoRa.read();
    handleAckReceived(ackStr);
  }

  // -----------------------------------------------------
  // Apagado diferido del AP tras emparejar
  // -----------------------------------------------------
  if (ap_should_stop && millis() > ap_stop_at)
  {
    Serial.println("AP OFF");
    ap_stop();
    ap_should_stop = false;
  }

  delay(10);
}