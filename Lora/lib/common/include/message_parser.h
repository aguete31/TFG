#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include "protocol.h"

/**
 * @brief Parsea y valida un JSON tipo "data", descifra y rellena una estructura LoRaMessage.
 *
 * Pasos:
 *  - Deserializar JSON.
 *  - Extraer campos: seq, retry, type, crc, iv, tag, payload.
 *  - Verificar CRC llamando a verifyCRC(doc, crcStr).
 *  - Convertir iv/tag/payload (hex) a bytes con hexToBytes.
 *  - Construir AAD con build_aad(seq, TYPE_CODE_DATA, retry, ...).
 *  - Llamar a aes_gcm_decrypt y, si es correcto, asignar msg.payload con el plaintext.
 *
 * @param jsonStr JSON entrante
 * @param msg     Estructura LoRaMessage a rellenar
 * @return true si el mensaje es válido y la descifrado fue correcto, false en caso contrario
 */
bool parseDataMessage_JSON(const String& jsonStr, LoRaMessage& msg);

/**
 * @brief Parsea y valida un JSON tipo "ack" y verifica autenticidad.
 *
 * Pasos:
 *  - Deserializar JSON.
 *  - Extraer campos: seq, type, crc, iv, tag.
 *  - Verificar CRC con verifyCRC.
 *  - Convertir iv/tag de hex a bytes.
 *  - Construir AAD con build_aad(seq, TYPE_CODE_ACK, 0, ...).
 *  - Llamar a aes_gcm_decrypt con payload vacío para verificar tag.
 *
 * @param jsonStr JSON entrante del ACK
 * @param ack     Estructura LoRaAck a rellenar (solo seq/type/crc)
 * @return true si el ACK es válido y pasa la verificación de autenticidad, false en caso contrario
 */
bool parseAckMessage_JSON(const String& jsonStr, LoRaAck& ack);

/**
 * @brief Verifica el CRC recibido comparándolo con el calculado.
 *
 * Implementación:
 *  - Elimina temporalmente el campo "crc" del JsonDocument.
 *  - Serializa el documento a String y calcula crc con calcCRC16.
 *  - Compara con crcRecvStr (hex).
 *  - Restaura el campo "crc" en el documento.
 *
 * @param doc        JsonDocument ya deserializado del mensaje
 * @param crcRecvStr Cadena hexadecimal del CRC recibida en el JSON
 * @return true si CRC calculado == CRC recibido, false en caso contrario
 */
bool verifyCRC(JsonDocument& doc, const String& crcRecvStr);