#include "sensor_dht.h"
#include <DHT.h>

// Pin donde está conectado el DHT11 (DATA)
// Debe tener un pull-up externo de 10 kΩ
#define DHT_PIN 13

// Tipo de sensor: DHT11
#define DHT_TYPE DHT11

// Instancia global del sensor.
// Se declara static para limitar su ámbito SOLO a este fichero .cpp
// evitando que otros módulos accedan directamente al objeto.
static DHT dht(DHT_PIN, DHT_TYPE);

/**
 * @brief Inicializa el sensor DHT11.
 *
 * Debe llamarse una vez desde setup() antes de realizar lecturas.
 * Internamente configura tiempos y prepara el bus 1-wire.
 */
void dht_init()
{
    dht.begin(); // Arranca la librería Adafruit DHT
}

/**
 * @brief Lee temperatura y humedad desde el DHT11.
 *
 * @param temp Referencia donde se almacenará la temperatura (°C)
 * @param hum  Referencia donde se almacenará la humedad relativa (%)
 *
 * @return true  → Lectura válida
 * @return false → Error de lectura o sensor desconectado
 *
 * Esta función:
 *  - Realiza una lectura del sensor
 *  - Comprueba si los valores son válidos
 *  - Devuelve los datos por referencia
 */
bool dht_read(float &temp, float &hum)
{
    // Leer humedad (%)
    hum = dht.readHumidity();

    // Leer temperatura (°C)
    temp = dht.readTemperature();

    // Si la librería devuelve NaN, significa error de lectura
    if (isnan(hum) || isnan(temp))
    {
        return false;
    }

    // Lectura correcta
    return true;
}