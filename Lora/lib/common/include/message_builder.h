#pragma once
#include <Arduino.h>


/**
 * @brief Construye un mensaje DATA usando el protocolo binario compacto.
 *
 * Formato:
 *  [HEADER 12 B]
 *  [IV AES_IV_SIZE B]
 *  [CIPHERTEXT N B]
 *  [TAG AES_TAG_SIZE B]
 *
 * La cabecera binaria se utiliza también como AAD de AES-GCM.
 *
 * @param seq Número de secuencia.
 * @param deviceId Identificador hexadecimal del dispositivo.
 * @param payload Payload en texto plano que será cifrado.
 * @param outPacket Buffer donde se escribirá la trama binaria.
 * @param outCapacity Capacidad total del buffer.
 * @param outLen Longitud real de la trama generada.
 *
 * @return true si se construyó correctamente.
 */
bool createDataMessageBinary(uint32_t seq, const String &deviceId, const String &payload, uint8_t *outPacket, size_t outCapacity, size_t &outLen);

/**
 * @brief Construye un ACK usando el protocolo binario compacto.
 *
 * Formato:
 *  [HEADER 12 B]
 *  [IV AES_IV_SIZE B]
 *  [TAG AES_TAG_SIZE B]
 *
 * La cabecera se usa como AAD de AES-GCM.
 *
 * @param seq Número de secuencia confirmado.
 * @param deviceId Identificador hexadecimal del dispositivo que envía el ACK.
 * @param outPacket Buffer de salida.
 * @param outCapacity Capacidad total del buffer.
 * @param outLen Longitud real generada.
 *
 * @return true si se construyó correctamente.
 */
bool createAckMessageBinary(uint32_t seq, const String &deviceId, uint8_t *outPacket, size_t outCapacity, size_t &outLen);