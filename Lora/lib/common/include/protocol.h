#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>

/**
 * @brief Estructura que representa un mensaje de datos LoRa.
 *
 * Campos:
 *  - seq: número de secuencia del paquete (uint32_t).
 *  - retry: contador de reintentos asociado al envío (uint8_t).
 *  - type: tipo de mensaje en texto ("data" esperado).
 *  - deviceId: identificador unico del dispositivo emisor, en hex
 *              (derivado de la MAC del ESP32)
 *  - payload: payload en texto plano ya descifrado (llenado por el parser).
 *  - crc: CRC en hex presente en el JSON recibido/enviado.
 *  - iv: IV del AES en formato hex (incluido en el JSON).
 *  - tag: tag de autenticación AES-GCM en formato hex.
 */
struct LoRaMessage {
  uint32_t seq;
  uint8_t retry;
  String type;
  String deviceId;
  String payload;
  String crc;
  String iv;
  String tag;
};

/**
 * @brief Estructura que representa un ACK LoRa.
 *
 * Campos:
 *  - seq: número de secuencia confirmado.
 *  - type: tipo de mensaje ("ack").
 *  - deviceId: identificador del dispositivo que envia el ACK
 *              (normalmente el gateway)
 *  - crc: CRC en hex presente en el JSON del ACK.
 *  - iv: IV en hex usado para autenticar el ACK.
 *  - tag: tag AES-GCM en hex para verificar autenticidad.
 */
struct LoRaAck {
  uint32_t seq;
  String type;
  String deviceId;
  String crc;
  String iv;
  String tag;
};

/**
 * @brief Clase de conveniencia que agrupa las funciones del protocolo.
 *
 * Esta clase actúa como fachada (wrapper) sobre las funciones de creación
 * y parseo que están implementadas en message_builder/message_parser.
 */
class LoRaProtocol {
public:

  /**
   * @brief Crea un JSON de mensaje de datos (envolviendo createDataMessage_JSON).
   * @param seq Número de secuencia.
   * @param retry Contador de reintentos.
   * @param deviceId Identificador del dispositivo emisor en hex.
   * @param payload Texto plano a cifrar.
   * @return String JSON listo para enviar.
   */
  static String createDataMessage(uint32_t seq, uint8_t retry, const String& deviceId, const String& payload);

  /**
   * @brief Crea un JSON de ACK autenticado (envolviendo createAckMessage_JSON).
   * @param seq Número de secuencia confirmado.
   * @param deviceId Identificador del dispositivo que envia el ACK.
   * @return String JSON del ACK.
   */
  static String createAckMessage(uint32_t seq, const String& deviceId);

  /**
   * @brief Parsea un JSON de mensaje de datos y rellena una LoRaMessage.
   * @param jsonStr JSON entrante.
   * @param msg Referencia a la estructura LoRaMessage que se rellenará.
   * @return true si parse y verificación/descifrado fueron correctos.
   */
  static bool parseMessage(const String& jsonStr, LoRaMessage& msg);

  /**
   * @brief Parsea un JSON de ACK y rellena una LoRaAck.
   * @param jsonStr JSON del ACK.
   * @param ack Referencia a la estructura LoRaAck que se rellenará.
   * @return true si el ACK es válido y autenticado.
   */
  static bool parseAck(const String& jsonStr, LoRaAck& ack);

  /**
   * @brief Wrapper para verificación de CRC en un JsonDocument.
   * 
   * Nota: delega la verificación al helper verifyCRC definido en message_parser.
   * @param doc JsonDocument ya deserializado.
   * @param crcRecv CRC recibido en formato hexadecimal.
   * @return true si el CRC calculado coincide con crcRecv.
   */
  static bool verifyCRC(JsonDocument& doc, const String& crcRecv);
};