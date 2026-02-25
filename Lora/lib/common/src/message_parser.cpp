#include "message_parser.h"
#include "hex_utils.h"
#include "aad_utils.h"
#include "crypto.h"
#include "crc.h"
#include <stdlib.h>
#include <ArduinoJson.h>

/**
 * @brief Verifica el CRC del JsonDocument contra el CRC recibido (en hex).
 *
 * Detalles:
 *  - El parámetro `doc` es modificado temporalmente (se elimina "crc") y luego restaurado.
 *  - Se asume que crcRecvStr está en formato hexadecimal (p. ej. "1A2B").
 */
bool verifyCRC(JsonDocument &doc, const String &crcRecvStr)
{
  // remove crc
  doc.remove("crc");

  String dataToCheck;
  serializeJson(doc, dataToCheck);

  uint16_t crcCalc = calcCRC16(dataToCheck);
  uint16_t crcRecv = (uint16_t)strtol(crcRecvStr.c_str(), NULL, 16);

  // restore
  doc["crc"] = crcRecvStr;

  return (crcCalc == crcRecv);
}

/**
 * @brief Parsea un mensaje de datos JSON, verifica CRC y descifra payload.
 *
 * Notas de implementacion:
 *  - Se usa DynamicJsonDocument de 768 bytes; ajustar si hay problemas de memoria.
 *  - Se reservan buffers con malloc para ciphertext y plaintext si hay datos cifrados.
 *  - Si cipherLen==0, se crea plaintext de 1 byte con '\0'.
 *  - Después de convertir hex->bytes, se construye AAD y se llama a aes_gcm_decrypt.
 *  - En caso de fallo (JSON, CRC, malloc, hexToBytes, decrypt) se libera memoria y retorna false.
 *
 * @param jsonStr JSON entrante
 * @param msg     Estructura LoRaMessage donde se guardan seq, retry, type, crc, iv, tag, payload (plaintext)
 * @return true si parse y decrypt son correctos; false en caso contrario
 */
bool parseDataMessage_JSON(const String &jsonStr, LoRaMessage &msg)
{
  DynamicJsonDocument doc(768);
  DeserializationError error = deserializeJson(doc, jsonStr);
  if (error)
  {
    Serial.printf("parseDataMessage_JSON: JSON parse error: %s\n", error.c_str());
    return false;
  }

  msg.seq = doc["seq"] | 0;
  msg.retry = doc["retry"] | 0;
  msg.type = doc["type"] | "";
  msg.deviceId = doc["deviceId"] | "";
  msg.crc = doc["crc"] | "";
  msg.iv = doc["iv"] | "";
  msg.tag = doc["tag"] | "";
  String cipherHex = doc["payload"] | "";

  if (!verifyCRC(doc, msg.crc))
  {
    Serial.printf("parseDataMessage_JSON: CRC failed seq=%lu\n", (unsigned long)msg.seq);
    return false;
  }

  size_t cipherLen = cipherHex.length() / 2;
  uint8_t iv[AES_IV_SIZE];
  uint8_t tag[AES_TAG_SIZE];
  uint8_t *ciphertext = nullptr;
  uint8_t *plaintext = nullptr;

  if (!hexToBytes(msg.iv, iv, AES_IV_SIZE))
    return false;
  if (!hexToBytes(msg.tag, tag, AES_TAG_SIZE))
    return false;

  if (cipherLen > 0)
  {
    ciphertext = (uint8_t *)malloc(cipherLen);
    plaintext = (uint8_t *)malloc(cipherLen + 1);
    if (!ciphertext || !plaintext)
    {
      if (ciphertext)
        free(ciphertext);
      if (plaintext)
        free(plaintext);
      Serial.println("parseDataMessage_JSON: malloc failed");
      return false;
    }
    if (!hexToBytes(cipherHex, ciphertext, cipherLen))
    {
      free(ciphertext);
      free(plaintext);
      return false;
    }
  }
  else
  {
    plaintext = (uint8_t *)malloc(1);
    if (!plaintext)
      return false;
    plaintext[0] = '\0';
  }

  uint8_t aad[12];
  size_t aad_len = 0;
  build_aad(msg.seq, TYPE_CODE_DATA, msg.retry, msg.deviceId, aad, &aad_len);

  bool ok = aes_gcm_decrypt(ciphertext ? ciphertext : (const uint8_t *)"", cipherLen, AES_KEY,
                            iv, AES_IV_SIZE, aad, aad_len, tag, plaintext);
  if (!ok)
  {
    Serial.printf("parseDataMessage_JSON: decrypt/auth failed seq=%lu\n", (unsigned long)msg.seq);
    if (ciphertext)
      free(ciphertext);
    if (plaintext)
      free(plaintext);
    return false;
  }

  if (cipherLen > 0)
    plaintext[cipherLen] = '\0';
  msg.payload = String((char *)plaintext);

  if (ciphertext)
    free(ciphertext);
  if (plaintext)
    free(plaintext);
  return true;
}

/**
 * @brief Parsea y valida un ACK: verifica CRC, convierte iv/tag y valida autenticidad con AES-GCM.
 *
 * Notas:
 *  - La verificación de autenticidad se hace llamando a aes_gcm_decrypt con payload vacío.
 *  - Si la verificación falla, retorna false.
 */
bool parseAckMessage_JSON(const String &jsonStr, LoRaAck &ack)
{
  DynamicJsonDocument doc(256);
  DeserializationError error = deserializeJson(doc, jsonStr);
  if (error)
  {
    Serial.printf("parseAckMessage_JSON: JSON parse error: %s\n", error.c_str());
    return false;
  }

  ack.seq = doc["seq"] | 0;
  ack.type = doc["type"] | "";
  ack.deviceId = doc["deviceId"] | "";
  ack.crc = doc["crc"] | "";

  if (!verifyCRC(doc, ack.crc))
  {
    Serial.printf("parseAckMessage_JSON: CRC failed ack=%lu\n", (unsigned long)ack.seq);
    return false;
  }

  String ivHex = doc["iv"] | "";
  String tagHex = doc["tag"] | "";

  uint8_t iv[AES_IV_SIZE];
  uint8_t tag[AES_TAG_SIZE];

  if (!hexToBytes(ivHex, iv, AES_IV_SIZE))
    return false;
  if (!hexToBytes(tagHex, tag, AES_TAG_SIZE))
    return false;

  uint8_t aad[12];
  size_t aad_len = 0;
  build_aad(ack.seq, TYPE_CODE_ACK, 0, ack.deviceId, aad, &aad_len);

  uint8_t dummy_out[1];
  bool ok = aes_gcm_decrypt((const uint8_t *)"", 0, AES_KEY, iv, AES_IV_SIZE,
                            aad, aad_len, tag, dummy_out);
  if (!ok)
  {
    Serial.printf("parseAckMessage_JSON: auth failed ack=%lu\n", (unsigned long)ack.seq);
    return false;
  }

  return true;
}