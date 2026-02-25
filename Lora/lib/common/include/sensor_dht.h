#pragma once
#include <Arduino.h>

/**
 * Inicializa el sensor DHT11.
 */
void dht_init();

/**
 * Lee temperatura y humedad del DHT11.
 * 
 * @param temp Referencia donde se almacenará la temperatura.
 * @param hum  Referencia donde se almacenará la humedad.
 * @return true si la lectura es válida, false si falla.
 */
bool dht_read(float &temp, float &hum);
