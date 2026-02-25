#pragma once
#include <Arduino.h>

/**
 * @brief Calcula el CRC16 de una cadena de texto.
 * 
 * Utiliza el polinomio estándar 0xA001 (reflejado) para calcular el checksum.
 * 
 * @param data Cadena de entrada
 * @return Valor CRC16 calculado
 */
uint16_t calcCRC16(const String& data);