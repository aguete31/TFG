#include "message_parser.h"
#include "crypto.h"
#include <stdlib.h>


// ----------------------------------------
// ------ Formato de recepcion binaria ----
// ----------------------------------------
bool parseDataMessageBinary(const uint8_t *packet, size_t packetLen, LoRaMessage &msg)
{
  // -----------------------------------------------------
  // Validaciones básicas
  // -----------------------------------------------------
  if (packet == nullptr)
  {
    Serial.println("parseDataMessageBinary: paquete nulo");
    return false;
  }

  // Tamaño mínimo:
  // HEADER + IV + TAG
  const size_t minimumSize = LORA_HEADER_SIZE + AES_IV_SIZE + AES_TAG_SIZE;

  if (packetLen < minimumSize || packetLen > LORA_MAX_PACKET_SIZE)
  {
    Serial.printf("parseDataMessageBinary: longitud inválida (%u bytes)\n", (unsigned int)packetLen);
    return false;
  }

  // -----------------------------------------------------
  // Parsear cabecera
  // -----------------------------------------------------
  LoRaBinaryHeader header;

  if (!parseBinaryHeader(packet, packetLen, header))
  {
    Serial.println("parseDataMessageBinary: cabecera inválida");
    return false;
  }

  // Esta función solamente acepta DATA
  if (header.type != LORA_TYPE_DATA)
  {
    Serial.println("parseDataMessageBinary: tipo no es DATA");
    return false;
  }

  // -----------------------------------------------------
  // Calcular posiciones
  // -----------------------------------------------------
  const size_t ivOffset = LORA_HEADER_SIZE;
  const size_t ciphertextOffset = ivOffset + AES_IV_SIZE;
  const size_t tagOffset = packetLen - AES_TAG_SIZE;
  const size_t ciphertextLen = tagOffset - ciphertextOffset;
  const uint8_t *iv = &packet[ivOffset];
  const uint8_t *ciphertext = &packet[ciphertextOffset];
  const uint8_t *tag = &packet[tagOffset];

  // -----------------------------------------------------
  // Reservar plaintext
  // -----------------------------------------------------
  uint8_t *plaintext = (uint8_t *)malloc(ciphertextLen + 1);

  if (!plaintext)
  {
    Serial.println("parseDataMessageBinary: malloc failed");
    return false;
  }

  // -----------------------------------------------------
  // AES-GCM
  // -----------------------------------------------------
  bool ok = aes_gcm_decrypt(ciphertextLen > 0 ? ciphertext : reinterpret_cast<const uint8_t *>(""), ciphertextLen, AES_KEY, iv, AES_IV_SIZE, packet, LORA_HEADER_SIZE, tag, plaintext);

  if (!ok)
  {
    Serial.printf("parseDataMessageBinary: autenticación fallida seq=%lu\n", (unsigned long)header.seq);

    free(plaintext);
    return false;
  }

  // El payload que ciframos es texto JSON,
  // por lo que añadimos terminador.
  plaintext[ciphertextLen] = '\0';

  // -----------------------------------------------------
  // Rellenar estructura existente
  // -----------------------------------------------------
  msg.seq = header.seq;
  msg.deviceId = header.deviceId;
  msg.payload = String(reinterpret_cast<char *>(plaintext));

  free(plaintext);

  Serial.printf("DATA BINARIO OK: seq=%lu deviceId=%s payload=%u bytes\n", (unsigned long)msg.seq, msg.deviceId.c_str(), (unsigned int)ciphertextLen);
  return true;
}



// ----------------------------------------
// -------- Formato de ACK binaria --------
// ----------------------------------------
bool parseAckMessageBinary(const uint8_t *packet, size_t packetLen, LoRaAck &ack)
{
  // -----------------------------------------------------
  // Validaciones básicas
  // -----------------------------------------------------
  if (packet == nullptr)
  {
    Serial.println("parseAckMessageBinary: paquete nulo");
    return false;
  }

  const size_t expectedSize = LORA_HEADER_SIZE + AES_IV_SIZE + AES_TAG_SIZE;

  if (packetLen != expectedSize)
  {
    Serial.printf("parseAckMessageBinary: longitud inválida (%u != %u bytes)\n", (unsigned int)packetLen, (unsigned int)expectedSize);
    return false;
  }

  // -----------------------------------------------------
  // Parsear cabecera
  // -----------------------------------------------------
  LoRaBinaryHeader header;

  if (!parseBinaryHeader(packet, packetLen, header))
  {
    Serial.println("parseAckMessageBinary: cabecera inválida");
    return false;
  }

  if (header.type != LORA_TYPE_ACK)
  {
    Serial.println("parseAckMessageBinary: tipo no es ACK");
    return false;
  }

  // -----------------------------------------------------
  // Localizar IV y TAG
  // -----------------------------------------------------
  const size_t ivOffset = LORA_HEADER_SIZE;
  const size_t tagOffset = ivOffset + AES_IV_SIZE;
  const uint8_t *iv = &packet[ivOffset];
  const uint8_t *tag = &packet[tagOffset];

  // -----------------------------------------------------
  // Verificar autenticación AES-GCM
  // -----------------------------------------------------
  uint8_t dummyOut[1] = {0};

  bool ok = aes_gcm_decrypt(reinterpret_cast<const uint8_t *>(""), 0, AES_KEY, iv, AES_IV_SIZE, packet, LORA_HEADER_SIZE, tag, dummyOut);

  if (!ok)
  {
    Serial.printf("parseAckMessageBinary: autenticación fallida seq=%lu\n", (unsigned long)header.seq);
    return false;
  }

  // -----------------------------------------------------
  // Rellenar estructura existente
  // -----------------------------------------------------
  ack.seq = header.seq;
  ack.deviceId = header.deviceId;

  Serial.printf("ACK BINARIO OK: seq=%lu deviceId=%s\n", (unsigned long)ack.seq, ack.deviceId.c_str());
  return true;
}