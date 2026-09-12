#include "hex_utils.h"

/**
 * @brief Convierte un array de bytes a su representación hexadecimal.
 */
String bytesToHex(const uint8_t *data, size_t len)
{
  String hexStr;
  hexStr.reserve(len * 2);

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
  // La longitud hexadecimal debe ser par
  if ((hexStr.length() % 2) != 0)
    return false;

  // Solo se permiten caracteres hexadecimales
  if (!isValidHex(hexStr))
    return false;

  size_t bytesLen = hexStr.length() / 2;

  // Comprobar capacidad del buffer
  if (bytesLen > maxLen)
    return false;

  for (size_t i = 0; i < bytesLen; i++)
  {
    char high = hexStr.charAt(2 * i);
    char low = hexStr.charAt(2 * i + 1);

    uint8_t highVal = (high >= '0' && high <= '9') ? high - '0' : toupper((unsigned char)high) - 'A' + 10;

    uint8_t lowVal = (low >= '0' && low <= '9') ? low - '0' : toupper((unsigned char)low) - 'A' + 10;

    outBytes[i] = (highVal << 4) | lowVal;
  }

  return true;
}

bool isValidHex(const String &hexStr)
{
  for (size_t i = 0; i < hexStr.length(); i++)
  {
    char c = hexStr.charAt(i);

    bool valid =
        (c >= '0' && c <= '9') ||
        (c >= 'A' && c <= 'F') ||
        (c >= 'a' && c <= 'f');

    if (!valid)
      return false;
  }

  return true;
}