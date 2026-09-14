#pragma once
#include <Arduino.h>

// =========================================================
// PROTOCOLO LORA BINARIO
// =========================================================

// Versión actual del protocolo
static constexpr uint8_t LORA_PROTOCOL_VERSION = 1;

// Tipos de mensaje
static constexpr uint8_t LORA_TYPE_DATA = 0x01;
static constexpr uint8_t LORA_TYPE_ACK  = 0x02;

// Tamaños fijos de los campos
static constexpr size_t DEVICE_ID_SIZE = 6;
static constexpr size_t SEQ_SIZE       = 4;

// Tamaño máximo de payload permitido por el SX127x / librería LoRa
static constexpr size_t LORA_MAX_PACKET_SIZE = 255;

// Posiciones de los campos dentro de la cabecera binaria
static constexpr size_t OFFSET_VERSION   = 0;
static constexpr size_t OFFSET_TYPE      = 1;
static constexpr size_t OFFSET_SEQ       = 2;
static constexpr size_t OFFSET_DEVICE_ID = 6;

// Tamaño total de la cabecera:
// version(1) + type(1) + seq(4) + deviceId(6)
static constexpr size_t LORA_HEADER_SIZE = 12;

/**
 * @brief Estructura que representa un mensaje de datos LoRa.
 */
struct LoRaMessage {
  uint32_t seq;
  String deviceId;
  String payload;

  uint64_t ivEpoch;
  uint32_t ivCounter;
};

/**
 * @brief Estructura que representa un ACK LoRa.
 */
struct LoRaAck {
  uint32_t seq;
  String deviceId;

  uint64_t ivEpoch;
  uint32_t ivCounter;
};

struct LoRaBinaryHeader
{
  uint8_t version;
  uint8_t type;
  uint32_t seq;
  String deviceId;
};


/**
* @brief Escribe un uint32_t en formato big-endian.
*/
void writeUint32BE(uint8_t *buffer, uint32_t value);

/**
* @brief Lee un uint32_t almacenado en formato big-endian.
*/
uint32_t readUint32BE(const uint8_t *buffer);

uint64_t readUint64BE(const uint8_t *buffer);

/**
* @brief Convierte un deviceId hexadecimal de 12 caracteres
*        en 6 bytes binarios.
*
* @param deviceIdHex ID hexadecimal, por ejemplo "14C281E350CC".
* @param outBytes Buffer de salida de DEVICE_ID_SIZE bytes.
* @return true si el ID es válido y pudo convertirse.
*/
bool writeDeviceIdBytes(const String &deviceIdHex, uint8_t *outBytes);

/**
* @brief Convierte un deviceId binario de 6 bytes
*        a su representación hexadecimal.
*
* @param bytes Buffer de DEVICE_ID_SIZE bytes.
* @return ID hexadecimal de 12 caracteres.
*/
String readDeviceIdHex(const uint8_t *bytes);

/**
* @brief Construye la cabecera binaria común de un paquete LoRa.
*
* Formato:
*  byte 0     : versión del protocolo
*  byte 1     : tipo de mensaje
*  bytes 2-5  : número de secuencia (big-endian)
*  bytes 6-11 : deviceId (6 bytes)
*
* @param type Tipo de mensaje (LORA_TYPE_DATA o LORA_TYPE_ACK).
* @param seq Número de secuencia.
* @param deviceIdHex ID hexadecimal del dispositivo (12 caracteres).
* @param outHeader Buffer donde se escribirá la cabecera.
* @param outSize Tamaño disponible en outHeader.
* @return true si la cabecera se construyó correctamente.
*/
bool buildBinaryHeader(uint8_t type, uint32_t seq, const String &deviceIdHex, uint8_t *outHeader, size_t outSize);

/**
* @brief Parsea una cabecera binaria LoRa.
*
* Formato:
*  byte 0     : versión del protocolo
*  byte 1     : tipo de mensaje
*  bytes 2-5  : número de secuencia (big-endian)
*  bytes 6-11 : deviceId (6 bytes)
*
* @param buffer Buffer recibido.
* @param len Longitud disponible en el buffer.
* @param header Estructura donde se guardarán los campos.
* @return true si la cabecera es válida.
*/
bool parseBinaryHeader(const uint8_t *buffer, size_t len, LoRaBinaryHeader &header);





















/**
 * @brief Clase de conveniencia que agrupa las funciones del protocolo.
 *
 * Esta clase actúa como fachada (wrapper) sobre las funciones de creación
 * y parseo que están implementadas en message_builder/message_parser.
 */
class LoRaProtocol {
public:

  /**
  * @brief Crea un mensaje DATA binario compacto.
  *
  * @param seq Número de secuencia.
  * @param deviceId Identificador hexadecimal del dispositivo.
  * @param payload Payload en texto plano que será cifrado.
  * @param outPacket Buffer de salida.
  * @param outCapacity Capacidad del buffer.
  * @param outLen Longitud real generada.
  * @return true si el mensaje se creó correctamente.
  */
  static bool createDataMessageBinary(uint32_t seq, const String &deviceId, const String &payload, uint8_t *outPacket, size_t outCapacity, size_t &outLen);

  /**
  * @brief Crea un ACK binario compacto.
  */
  static bool createAckMessageBinary(uint32_t seq, const String &deviceId, uint8_t *outPacket, size_t outCapacity, size_t &outLen);

  /**
  * @brief Parsea y descifra un mensaje DATA binario.
  */
  static bool parseMessageBinary(const uint8_t *packet, size_t packetLen, LoRaMessage &msg);

  /**
  * @brief Parsea y autentica un ACK binario.
  */
  static bool parseAckBinary(const uint8_t *packet, size_t packetLen, LoRaAck &ack);

};