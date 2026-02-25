#pragma once
#include <Arduino.h>

/**
 * @brief Crea el JSON de un mensaje de datos cifrado.
 *
 * Construye y devuelve un String JSON que contiene:
 *  - seq: número de secuencia
 *  - retry: contador de reintentos
 *  - type: "data"
 *  - iv: IV en hex (AES_IV_SIZE bytes)
 *  - tag: tag de autenticación en hex (AES_TAG_SIZE bytes)
 *  - payload: ciphertext en hex (puede estar vacío)
 *  - crc: CRC16 en hexadecimal calculado sobre el JSON sin el campo "crc"
 *
 * Flujo interno:
 *  1. Obtiene payload y su longitud.
 *  2. Genera IV con generateSecureIV().
 *  3. Construye AAD con build_aad(seq, TYPE_CODE_DATA, retry, ...).
 *  4. Cifra con aes_gcm_encrypt -> produce ciphertext y tag.
 *  5. Convierte iv/tag/ciphertext a hex y arma el JSON.
 *  6. Calcula CRC16 del JSON (sin crc) y lo añade en hex.
 *
 * Notas:
 *  - Usa malloc/free para el buffer del ciphertext (si payloadLen > 0).
 *  - Devuelve "{}" en caso de error (malloc o encriptación).
 *
 * @param seq   Número de secuencia del paquete.
 * @param retry Contador de reintentos.
 * @param deviceId Identificador del dispositivo emisor.
 * @param payload Texto plano a cifrar (puede ser vacío).
 * @return String JSON completo listo para enviar.
 */
String createDataMessage_JSON(uint32_t seq, uint8_t retry, const String& deviceId, const String& payload);

/**
 * @brief Crea el JSON de un ACK autenticado (sin payload).
 *
 * Construye y devuelve un String JSON que contiene:
 *  - type: "ack"
 *  - seq: número de secuencia confirmado
 *  - iv: IV en hex (AES_IV_SIZE bytes)
 *  - tag: tag de autenticación en hex (AES_TAG_SIZE bytes)
 *  - crc: CRC16 en hexadecimal calculado sobre el JSON sin el campo "crc"
 *
 * Flujo interno:
 *  1. Genera IV con generateSecureIV().
 *  2. Construye AAD con build_aad(seq, TYPE_CODE_ACK, 0, ...).
 *  3. Llama a aes_gcm_encrypt con payload vacío para obtener tag.
 *  4. Convierte iv y tag a hex, arma el JSON y calcula/añade CRC.
 *
 * @param seq Número de secuencia que se confirma.
 * @param deviceId Identificador del dispositivo que envia el ACK
 * @return String JSON del ACK listo para enviar.
 */
String createAckMessage_JSON(uint32_t seq, const String& deviceId);