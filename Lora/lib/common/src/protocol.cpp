#include "protocol.h"
#include "message_builder.h"
#include "message_parser.h"

/**
 * @brief Invoca createDataMessage_JSON para construir el JSON del mensaje de datos.
 */
String LoRaProtocol::createDataMessage(uint32_t seq, uint8_t retry, const String& deviceId, const String& payload) {
  return createDataMessage_JSON(seq, retry, deviceId, payload);
}

/**
 * @brief Invoca createAckMessage_JSON para construir el JSON del ACK.
 */
String LoRaProtocol::createAckMessage(uint32_t seq, const String& deviceId) {
  return createAckMessage_JSON(seq, deviceId);
}

/**
 * @brief Wrapper que delega en parseDataMessage_JSON para parsear y descifrar mensajes de datos.
 */
bool LoRaProtocol::parseMessage(const String& jsonStr, LoRaMessage& msg) {
  return parseDataMessage_JSON(jsonStr, msg);
}

/**
 * @brief Wrapper que delega en parseAckMessage_JSON para validar ACKs autenticados.
 */
bool LoRaProtocol::parseAck(const String& jsonStr, LoRaAck& ack) {
  return parseAckMessage_JSON(jsonStr, ack);
}

/**
 * @brief Wrapper para la función verifyCRC (definida en message_parser).
 *
 * Este método solo reexpone la funcionalidad de verificación de CRC para
 * mantener la interfaz agrupada en la clase LoRaProtocol.
 */
bool LoRaProtocol::verifyCRC(JsonDocument& doc, const String& crcRecv) {
  return verifyCRC(doc, crcRecv);
}