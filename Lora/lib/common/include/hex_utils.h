#pragma once
#include <Arduino.h>

/**
 * @brief Convierte un array de bytes a representación hexadecimal en mayúsculas.
 * 
 * @param data Array de bytes a convertir
 * @param len  Longitud del array
 * @return Representación hexadecimal como String
 */
String bytesToHex(const uint8_t* data, size_t len);

/**
 * @brief Convierte una cadena hexadecimal a array de bytes.
 * 
 * @param hexStr   Cadena hexadecimal (debe tener longitud par)
 * @param outBytes Buffer de salida donde se guardarán los bytes
 * @param maxLen   Tamaño máximo del buffer de salida
 * @return true si la conversión fue exitosa, false si hay error
 */
bool hexToBytes(const String& hexStr, uint8_t* outBytes, size_t maxLen);

/**
 * @brief Comprueba si una cadena contiene únicamente caracteres hexadecimales.
 *
 * @param hexStr Cadena a validar.
 * @return true si todos los caracteres son 0-9, A-F o a-f.
 */
bool isValidHex(const String &hexStr);