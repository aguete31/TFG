#pragma once
#include <Arduino.h>
#include "protocol.h"


/**
 * @brief Parsea, autentica y descifra un mensaje DATA binario.
 *
 * Formato:
 *  [HEADER 12 B]
 *  [IV AES_IV_SIZE B]
 *  [CIPHERTEXT N B]
 *  [TAG AES_TAG_SIZE B]
 *
 * La cabecera se utiliza como AAD de AES-GCM.
 *
 * @param packet Buffer binario recibido.
 * @param packetLen Longitud total recibida.
 * @param msg Estructura donde se devolverán los datos.
 * @return true si el mensaje es válido y pudo descifrarse.
 */
bool parseDataMessageBinary(const uint8_t *packet, size_t packetLen, LoRaMessage &msg);


/**
 * @brief Parsea y autentica un ACK binario.
 *
 * Formato:
 *  [HEADER 12 B]
 *  [IV AES_IV_SIZE B]
 *  [TAG AES_TAG_SIZE B]
 *
 * La cabecera se utiliza como AAD de AES-GCM.
 *
 * @param packet Buffer binario recibido.
 * @param packetLen Longitud total recibida.
 * @param ack Estructura donde se devolverán los datos del ACK.
 * @return true si el ACK es válido y está autenticado.
 */
bool parseAckMessageBinary(const uint8_t *packet, size_t packetLen, LoRaAck &ack);