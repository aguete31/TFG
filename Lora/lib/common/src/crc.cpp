#include "crc.h"

/**
 * @brief Implementación del cálculo CRC16 con polinomio 0xA001.
 *
 * Este método recorre cada byte del string y aplica el algoritmo bit a bit.
 */
uint16_t calcCRC16(const String &data)
{
  uint16_t crc = 0xFFFF; // valor inicial típico

  for (size_t i = 0; i < data.length(); i++)
  {
    crc ^= (uint8_t)data[i]; // XOR con el siguiente byte

    // Procesa cada bit del byte actual
    for (uint8_t j = 0; j < 8; j++)
    {
      if (crc & 1)
        crc = (crc >> 1) ^ 0xA001; // si el LSB es 1, aplica polinomio
      else
        crc >>= 1; // si no, solo desplaza
    }
  }
  return crc; // valor final del CRC
}