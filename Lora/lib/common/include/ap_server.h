#pragma once

#include <stdint.h>
#include "pair_status.h"

/**
 * @typedef pair_callback_t
 * @brief Tipo de función callback que se invoca al completar el proceso de emparejamiento.
 * @param ok Indica si el emparejamiento fue exitoso (true) o fallido (false).
 */
typedef void (*pair_callback_t)(bool ok);

/**
 * @var pairStatus
 * @brief Estado global y volátil del proceso de emparejamiento.
 */
extern volatile PairStatus pairStatus;

/**
 * @enum PairState
 * @brief Estados posibles del proceso de emparejamiento en el punto de acceso.
 */
enum PairState {
  PAIR_IDLE,    /**< Estado inactivo, sin emparejamiento en curso */
  PAIR_PENDING, /**< Emparejamiento pendiente o en cola */
  PAIR_BUSY,    /**< Emparejamiento en proceso */
  PAIR_SUCCESS, /**< Emparejamiento completado con éxito */
  PAIR_FAILED   /**< Emparejamiento fallido */
};

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