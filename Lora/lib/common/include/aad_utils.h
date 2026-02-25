#pragma once
#include <Arduino.h>

// Tipos de mensajes: datos y confirmación (ACK)
#define TYPE_CODE_DATA 1
#define TYPE_CODE_ACK  2

/**
 * @brief Construye el AAD (Additional Authenticated Data) para criptografía.
 * 
 * El AAD contiene metadatos del mensaje que deben ser autenticados pero no cifrados.
 * Formato (6 bytes):
 *  - Bytes 0-3: número de secuencia (big-endian)
 *  - Byte 4: tipo de mensaje (DATA o ACK)
 *  - Byte 5: número de reintentos
 *  - Byte 6-11: deviceId en binario (primeros 6 bytes del hex de deviceId)
 * 
 * @param seq       Número de secuencia del paquete
 * @param type_code Tipo de mensaje (TYPE_CODE_DATA o TYPE_CODE_ACK)
 * @param retry     Contador de reintentos
 * @param deviceIdHex String con el ID de dispositivo en hexadecimal (12 chars)
 * @param out_aad   Buffer de salida donde se escribirán los 6 bytes del AAD
 * @param out_len   Puntero donde se guardará la longitud del AAD (siempre 6)
 */
void build_aad(uint32_t seq, uint8_t type_code, uint8_t retry, const String& deviceIdHex, uint8_t* out_aad, size_t* out_len);