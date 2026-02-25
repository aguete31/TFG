#include "aad_utils.h"
#include "hex_utils.h"

/**
 * @brief Implementación de build_aad: serializa los datos en formato big-endian.
 */
void build_aad(uint32_t seq, uint8_t type_code, uint8_t retry, const String &deviceIdHex, uint8_t *out_aad, size_t *out_len)
{
  // Serializa el número de secuencia en big-endian (MSB primero)
  out_aad[0] = (uint8_t)((seq >> 24) & 0xFF); // byte más significativo
  out_aad[1] = (uint8_t)((seq >> 16) & 0xFF);
  out_aad[2] = (uint8_t)((seq >> 8) & 0xFF);
  out_aad[3] = (uint8_t)(seq & 0xFF); // byte menos significativo

  // Agrega tipo de mensaje y número de reintentos
  out_aad[4] = type_code;
  out_aad[5] = retry;

  // Pasar de hexadecimal a bytes
  String devHex = deviceIdHex.substring(0, 12);
  while (devHex.length() < 12)
  {
    devHex += "0";
  }

  uint8_t devBytes[6] = {0};
  hexToBytes(devHex, devBytes, 6);

  // Copia al AAD
  for (int i = 0; i < 6; i++)
  {
    out_aad[6 + i] = devBytes[i];
  }

  // Devuelve la longitud del AAD generado
  *out_len = 12;
}