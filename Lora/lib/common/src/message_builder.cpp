#include "message_builder.h"
#include "iv_generator.h"
#include "crypto.h"
#include "protocol.h"


// ----------------------------------------
// ---- Formato de transmision binaria ----
// ----------------------------------------
bool createDataMessageBinary(uint32_t seq, const String &deviceId, const String &payload, uint8_t *outPacket, size_t outCapacity, size_t &outLen)
{
  outLen = 0;

  // -----------------------------------------------------
  // Validaciones iniciales
  // -----------------------------------------------------
  if (outPacket == nullptr)
  {
    Serial.println("createDataMessageBinary: buffer nulo");
    return false;
  }

  size_t payloadLen = payload.length();

  // Tamaño total:
  // header + IV + ciphertext + TAG
  size_t requiredSize = LORA_HEADER_SIZE + AES_IV_SIZE + payloadLen + AES_TAG_SIZE;

  if (requiredSize > outCapacity || requiredSize > LORA_MAX_PACKET_SIZE)
  {
    Serial.printf("createDataMessageBinary: paquete demasiado grande (%u bytes)\n", (unsigned int)requiredSize);
    return false;
  }

  // -----------------------------------------------------
  // Construir cabecera
  // -----------------------------------------------------
  if (!buildBinaryHeader(LORA_TYPE_DATA, seq, deviceId, outPacket, outCapacity))
  {
    Serial.println("createDataMessageBinary: cabecera inválida");
    return false;
  }

  // -----------------------------------------------------
  // Posiciones dentro de la trama
  // -----------------------------------------------------
  size_t ivOffset = LORA_HEADER_SIZE;
  size_t ciphertextOffset = ivOffset + AES_IV_SIZE;
  size_t tagOffset = ciphertextOffset + payloadLen;
  uint8_t *iv = &outPacket[ivOffset];
  uint8_t *ciphertext = &outPacket[ciphertextOffset];
  uint8_t *tag = &outPacket[tagOffset];

  // -----------------------------------------------------
  // Generar IV seguro
  // -----------------------------------------------------
  if (!generateSecureIV(iv))
  {
    Serial.println("createDataMessageBinary: no se pudo generar IV seguro");
    return false;
  }

  // -----------------------------------------------------
  // Cifrado AES-GCM
  // -----------------------------------------------------
  const uint8_t *plaintext = reinterpret_cast<const uint8_t *>(payload.c_str());

  bool ok = aes_gcm_encrypt(plaintext, payloadLen, AES_KEY, iv, AES_IV_SIZE, outPacket, LORA_HEADER_SIZE, ciphertext, tag);

  if (!ok)
  {
    Serial.println("createDataMessageBinary: cifrado AES-GCM fallido");
    return false;
  }

  // -----------------------------------------------------
  // Resultado
  // -----------------------------------------------------
  outLen = requiredSize;

  Serial.printf("DATA BINARIO LoRa: %u bytes\n", (unsigned int)outLen);
  return true;
}



// -------------------------------
// --- Formato de ACK binario ----
// -------------------------------
bool createAckMessageBinary(uint32_t seq, const String &deviceId, uint8_t *outPacket, size_t outCapacity, size_t &outLen)
{
  outLen = 0;

  if (outPacket == nullptr)
  {
    Serial.println("createAckMessageBinary: buffer nulo");
    return false;
  }

  const size_t requiredSize = LORA_HEADER_SIZE + AES_IV_SIZE + AES_TAG_SIZE;

  if (requiredSize > outCapacity || requiredSize > LORA_MAX_PACKET_SIZE)
  {
    Serial.println("createAckMessageBinary: buffer insuficiente");
    return false;
  }

  // -----------------------------------------------------
  // Cabecera
  // -----------------------------------------------------
  if (!buildBinaryHeader(LORA_TYPE_ACK, seq, deviceId, outPacket, outCapacity))
  {
    Serial.println("createAckMessageBinary: cabecera inválida");
    return false;
  }

  // -----------------------------------------------------
  // Posiciones
  // -----------------------------------------------------
  const size_t ivOffset = LORA_HEADER_SIZE;
  const size_t tagOffset = ivOffset + AES_IV_SIZE;
  uint8_t *iv = &outPacket[ivOffset];
  uint8_t *tag = &outPacket[tagOffset];

  // -----------------------------------------------------
  // IV único
  // -----------------------------------------------------
  if (!generateSecureIV(iv))
  {
    Serial.println("createAckMessageBinary: no se pudo generar IV seguro");
    return false;
  }

  // -----------------------------------------------------
  // Autenticación AES-GCM
  // -----------------------------------------------------
  uint8_t dummyOut[1] = {0};

  bool ok = aes_gcm_encrypt(reinterpret_cast<const uint8_t *>(""), 0, AES_KEY, iv, AES_IV_SIZE, outPacket, LORA_HEADER_SIZE, dummyOut, tag);

  if (!ok)
  {
    Serial.println("createAckMessageBinary: autenticación AES-GCM fallida");
    return false;
  }

  outLen = requiredSize;

  Serial.printf("ACK BINARIO LoRa: %u bytes\n", (unsigned int)outLen);
  return true;
}