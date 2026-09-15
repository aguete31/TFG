#pragma once

#include <stdint.h>
#include "pair_status.h"
#include "pairing_service.h"

/**
 * @brief Inicia el punto de acceso WiFi y el servidor HTTP para la configuración y emparejamiento.
 * 
 * Configura el SSID, la contraseña y el número máximo de clientes permitidos.
 * 
 * @param ssid Nombre de la red WiFi a crear.
 * @param ap_pass Contraseña para acceder al punto de acceso.
 * @param max_clients Número máximo de clientes que pueden conectarse simultáneamente.
 */
void ap_start(const char* ssid, const char* ap_pass, uint8_t max_clients);

/**
 * @brief Detiene el punto de acceso WiFi y el servidor HTTP asociados.
 */
void ap_stop();

/**
 * @brief Registra una función callback que será llamada al finalizar el proceso de emparejamiento.
 * 
 * @param cb Puntero a la función callback que recibe un booleano indicando éxito o fallo.
 */
void ap_set_pair_callback(pair_callback_t cb);

/**
 * @brief Obtiene el estado actual del proceso de emparejamiento.
 * 
 * @return Estado actual del emparejamiento (valor de tipo PairState).
 */
PairState ap_get_state();