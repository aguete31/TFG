#include "hex_utils.h"

/**
 * @brief Convierte un array de bytes a su representación hexadecimal.
 */
String bytesToHex(const uint8_t *data, size_t len)
{
  String hexStr = "";
  const char hexChars[] = "0123456789ABCDEF"; // tabla de caracteres hexadecimales
  for (size_t i = 0; i < len; i++)
  {
    // Extrae nibble alto y bajo
    hexStr += hexChars[(data[i] >> 4) & 0x0F]; // nibble alto
    hexStr += hexChars[data[i] & 0x0F];        // nibble bajo
  }
  return hexStr;
}

/**
 * @brief Convierte una cadena hexadecimal a array de bytes.
 */
bool hexToBytes(const String &hexStr, uint8_t *outBytes, size_t maxLen)
{
  // Verifica que la longitud sea par
  if (hexStr.length() % 2 != 0)
    return false;

  size_t bytesLen = hexStr.length() / 2; // cantidad de bytes esperados
  if (bytesLen > maxLen)
    return false; // verifica espacio suficiente

  for (size_t i = 0; i < bytesLen; i++)
  {
    char high = hexStr.charAt(2 * i);    // carácter del nibble alto
    char low = hexStr.charAt(2 * i + 1); // carácter del nibble bajo

    // Convierte carácter a valor numérico (0-15)
    uint8_t highVal = (high >= '0' && high <= '9') ? high - '0' : (uint8_t)(toupper((unsigned char)high) - 'A' + 10);
    uint8_t lowVal = (low >= '0' && low <= '9') ? low - '0' : (uint8_t)(toupper((unsigned char)low) - 'A' + 10);

    // Combina ambos nibbles en un byte
    outBytes[i] = (highVal << 4) | lowVal;
  }
  return true;
}