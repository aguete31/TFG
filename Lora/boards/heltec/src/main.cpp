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
#include <ArduinoJson.h>

// =========================================================
// CONFIGURACIÓN GENERAL
// =========================================================

// Botón PRG del Heltec para hacer factory reset
#define FACTORY_RESET_PIN 0

// Parámetros del sistema de reintentos y temporización
#define MAX_TX_ATTEMPTS 3                      // Número máximo de intentos para enviar un mensaje
#define ACK_TIMEOUT 1500UL                     // Tiempo de espera para un ACK (1.5s)
// #define TX_INTERVAL 300000UL                // Intervalo entre envíos cuando no hay ACK pendiente (5min)
#define TX_INTERVAL 60000UL                     // Intervalo entre envíos cuando no hay ACK pendiente (1min)
#define RETRY_BACKOFF_MIN 300UL
#define RETRY_BACKOFF_MAX 1500UL
// =========================================================
// VARIABLES GLOBALES DEL SISTEMA
// =========================================================

// Secuencia global de mensajes (incrementa en cada envío correcto)
uint32_t seq = 0;

// Momento en que se genero la ultima lectura periodica
unsigned long lastMeasurementAt = 0;

// Control espera de ACK
bool waitingAck = false;
uint32_t currentSeq = 0;
uint32_t txAttempt = 0;
unsigned long ackStartedAt = 0;

// Control del backoff antes de un reintento
bool retryPending = false;
unsigned long retryStartedAt = 0;
unsigned long retryDelay = 0;

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
  snprintf(idStr, sizeof(idStr), "%012llX", chipId);
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

void processAck(const LoRaAck &ack)
{
  // Ignorar ACKs si no hay ninguna transmisión pendiente
  if (!waitingAck && !retryPending)
  {
    Serial.println("ACK ignorado: no hay transmisión pendiente");
    return;
  }

  if (ack.seq == currentSeq)
  {
    Serial.printf("ACK OK [%lu]\n", (unsigned long)ack.seq);

    waitingAck = false;
    retryPending = false;
    txAttempt = 0;
    seq++;

    bool firstPair = (pairStatus == WAITING);
    tx_enabled = true;

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
    Serial.printf(
        "ACK mismatch: recv=%lu expect=%lu\n",
        (unsigned long)ack.seq,
        (unsigned long)currentSeq);
  }
}

void handleAckReceivedBinary(const uint8_t *packet, size_t packetLen)
{
  LoRaAck ack;

  if (!LoRaProtocol::parseAckBinary(packet, packetLen, ack))
  {
    Serial.println("ACK binario inválido");
    return;
  }

  processAck(ack);
}

/**
 * @brief Envío de mensaje LoRa usando el protocolo robusto.
 *
 * Activa la espera de ACK y guarda el estado para posibles reintentos.
 */
void sendMessage(const String &payload)
{
  // Guardar payload para posibles reintentos
  currentPayload = payload;

  // Buffer para la trama binaria LoRa
  uint8_t packet[LORA_MAX_PACKET_SIZE];
  size_t packetLen = 0;

  // Construir DATA binario
  bool ok = LoRaProtocol::createDataMessageBinary(currentSeq, DEVICE_ID, payload, packet, sizeof(packet), packetLen);

  if (!ok)
  {
    Serial.println("TX cancelado: no se pudo construir el mensaje binario");
    return;
  }

  // Enviar bytes binarios por LoRa
  LoRa.beginPacket();
  LoRa.write(packet, packetLen);

  int result = LoRa.endPacket();

  if (result != 1)
  {
    Serial.println("TX error: LoRa.endPacket() falló");
    return;
  }

  Serial.printf("TX BIN [%lu] intento=%lu/%u (%u bytes): %s\n", (unsigned long)currentSeq, (unsigned long)(txAttempt + 1), MAX_TX_ATTEMPTS, (unsigned int)packetLen, payload.c_str());

  // Empezar espera de ACK
  waitingAck = true;
  ackStartedAt = millis();
}

/**
 * @brief Gestión del timeout cuando no se recibe ACK dentro del tiempo esperado.
 *
 *  Reintenta mientras no se haya alcanzado MAX_TX_ATTEMPTS.
 */
void handleTimeout()
{
  txAttempt++;

  // Se agotaron los reintentos
  if (txAttempt >= MAX_TX_ATTEMPTS)
  {
    Serial.printf("FAIL [%lu]: máximo de %u intentos alcanzado\n", (unsigned long)currentSeq, MAX_TX_ATTEMPTS);
    waitingAck = false;
    retryPending = false;
    txAttempt = 0;
    seq++;

    // Si estabamos verificando el pairing, el fallo es crítico
    if (pairStatus == WAITING)
    {
      Serial.println("Pairing failed: no ACK during pairing");
      tx_enabled = false;
      pairStatus = FAILED;
    }
    else
    {
      // Fallo de una lectura normal.
      // No detenemos el nodo, volverá a intentarlo cuando llegue el siguiente intervalo de medida.
      Serial.println("Lectura perdida: se continuará con el siguiente ciclo");
    }

    return;
  }

  // Reintentar
  Serial.printf("TIMEOUT [%lu]: programando reintento\n", (unsigned long)currentSeq);
  
  // No esperar ACK del intento anterior
  waitingAck = false;

  // Tiempo aleatorio antes de retransmitir
  retryDelay = RETRY_BACKOFF_MIN + (esp_random() % (RETRY_BACKOFF_MAX - RETRY_BACKOFF_MIN + 1));

  retryStartedAt = millis();
  retryPending = true;

  Serial.printf("Backoff: %lu ms\n",retryDelay);

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
  Serial.println("HELTEC TX ROBUST MODE (BINARY V1)");

  // Forzar una primera lectura inmediata cuando el nodo esté listo
  lastMeasurementAt = millis() - TX_INTERVAL;
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
  if (!waitingAck && !retryPending && millis() - lastMeasurementAt >= TX_INTERVAL)
  {

    // Registrar el comienzo de un nuevo ciclo de medida
    lastMeasurementAt = millis();
    currentSeq = seq;
    txAttempt = 0;

    // === Leer sensor DHT11 ===
    float temp, hum;
    bool ok = dht_read(temp, hum);

    if (!ok)
    {
      Serial.println("Error leyendo DHT11");
    }

    // === Construir payload JSON ===
    JsonDocument jdoc;
    if (ok)
    {
      jdoc["temp"] = temp;
      jdoc["hum"] = hum;
    }
    else
    {
      jdoc["temp"] = nullptr;
      jdoc["hum"] = nullptr;
    }

    jdoc["uptime_s"] = millis() / 1000;

    String payload;
    serializeJson(jdoc, payload);

    Serial.printf("Payload sensor: %u bytes\n", (unsigned int)payload.length());
    
    // Enviar por LoRa
    sendMessage(payload);
  }

  // -----------------------------------------------------
  // Timeout de ACK
  // -----------------------------------------------------
  if (waitingAck && (millis() - ackStartedAt >= ACK_TIMEOUT))
  {
    handleTimeout();
  }

  // -----------------------------------------------------
  // Reintento después del backoff aleatorio
  // -----------------------------------------------------
  if (retryPending && (millis() - retryStartedAt >= retryDelay))
  {
    retryPending = false;

    Serial.printf("RETRY [%lu] intento=%lu\n", (unsigned long)currentSeq, (unsigned long)(txAttempt + 1));

    sendMessage(currentPayload);
  }

  // -----------------------------------------------------
  // Recepción de ACK por LoRa
  // -----------------------------------------------------
  int pkt = LoRa.parsePacket();

  if (pkt)
  {
    uint8_t packet[LORA_MAX_PACKET_SIZE];
    size_t packetLen = 0;

    while (LoRa.available() && packetLen < LORA_MAX_PACKET_SIZE)
    {
      packet[packetLen++] = static_cast<uint8_t>(LoRa.read());
    }

    if (packetLen == 0)
    {
      Serial.println("ACK vacío recibido");
    }

    // ACK binario
    else if (packet[OFFSET_VERSION] == LORA_PROTOCOL_VERSION)
    {
      handleAckReceivedBinary(packet, packetLen);
    }

    else
    {
      Serial.printf("ACK con formato desconocido: 0x%02X\n", packet[0]);
    }
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