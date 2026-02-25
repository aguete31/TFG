#include "message_builder.h"
#include "hex_utils.h"
#include "aad_utils.h"
#include "iv_generator.h"
#include "crypto.h"
#include "crc.h"
#include <stdlib.h>
#include <ArduinoJson.h>

/**
 * @brief Implementación de createDataMessage_JSON.
 *
 * Comentarios importantes de implementacion:
 *  - payloadLen y payloadBytes se obtienen del String payload.
 *  - iv es un arreglo local de tamaño AES_IV_SIZE y se rellena con generateSecureIV().
 *  - Si payloadLen>0 se reserva (malloc) un buffer ciphertext de la misma longitud.
 *    En AES-GCM el ciphertext tiene la misma longitud que el plaintext.
 *  - tag tiene tamaño AES_TAG_SIZE.
 *  - Se construye el AAD con build_aad(seq, TYPE_CODE_DATA, retry, ...).
 *  - Llamada a aes_gcm_encrypt(...). Si falla, se libera memoria y devuelve "{}".
 *  - Convierte iv/tag/ciphertext a hex con bytesToHex para incrustarlos en JSON.
 *  - Construye DynamicJsonDocument, serializa sin crc, calcula CRC16, añade crc (en hex),
 *    vuelve a serializar y devuelve el String.
 */
String createDataMessage_JSON(uint32_t seq, uint8_t retry, const String &deviceId, const String &payload)
{
  size_t payloadLen = payload.length();
  const uint8_t *payloadBytes = (const uint8_t *)payload.c_str();

  uint8_t iv[AES_IV_SIZE];
  generateSecureIV(iv);

  uint8_t *ciphertext = nullptr;
  if (payloadLen > 0)
  {
    ciphertext = (uint8_t *)malloc(payloadLen);
    if (!ciphertext)
    {
      Serial.println("createDataMessage_JSON: malloc failed");
      return "{}";
    }
  }
  uint8_t tag[AES_TAG_SIZE];

  uint8_t aad[12];
  size_t aad_len = 0;
  build_aad(seq, TYPE_CODE_DATA, retry, deviceId, aad, &aad_len);

  bool ok = aes_gcm_encrypt(payloadBytes, payloadLen, AES_KEY, iv, AES_IV_SIZE,
                            aad, aad_len, ciphertext ? ciphertext : (uint8_t *)"", tag);
  if (!ok)
  {
    Serial.println("createDataMessage_JSON: encrypt failed");
    if (ciphertext)
      free(ciphertext);
    return "{}";
  }

  String ivHex = bytesToHex(iv, AES_IV_SIZE);
  String tagHex = bytesToHex(tag, AES_TAG_SIZE);
  String cipherHex = (payloadLen > 0) ? bytesToHex(ciphertext, payloadLen) : String("");

  DynamicJsonDocument doc(768);
  doc["seq"] = seq;
  doc["retry"] = retry;
  doc["type"] = "data";
  doc["deviceId"] = deviceId;
  doc["iv"] = ivHex;
  doc["tag"] = tagHex;
  doc["payload"] = cipherHex;

  // Serializar y CRC (sin campo crc)
  String jsonStr;
  serializeJson(doc, jsonStr);
  uint16_t crc = calcCRC16(jsonStr);
  char crcBuf[8];
  sprintf(crcBuf, "%04X", crc);
  doc["crc"] = String(crcBuf);

  jsonStr = "";
  serializeJson(doc, jsonStr);

  if (ciphertext)
    free(ciphertext);
  return jsonStr;
}

/**
 * @brief Implementación de createAckMessage_JSON.
 *
 * Comentarios importantes:
 *  - Genera IV con generateSecureIV().
 *  - Construye AAD con build_aad(seq, TYPE_CODE_ACK, 0, ...).
 *  - Llama a aes_gcm_encrypt con payload vacío para obtener sólo el tag de autenticación.
 *  - Convierte iv y tag a hex, arma JSON, calcula CRC16 y lo añade en hex.
 *  - Devuelve "{}" en caso de fallo de autenticación/encriptación.
 */
String createAckMessage_JSON(uint32_t seq, const String &deviceId)
{
  uint8_t iv[AES_IV_SIZE];
  generateSecureIV(iv);

  uint8_t tag[AES_TAG_SIZE];

  uint8_t aad[6];
  size_t aad_len = 0;
  build_aad(seq, TYPE_CODE_ACK, 0, deviceId, aad, &aad_len);

  uint8_t dummy_out[1] = {0};
  bool ok = aes_gcm_encrypt((const uint8_t *)"", 0, AES_KEY, iv, AES_IV_SIZE,
                            aad, aad_len, dummy_out, tag);
  if (!ok)
  {
    Serial.println("createAckMessage_JSON: auth failed");
    return "{}";
  }

  String ivHex = bytesToHex(iv, AES_IV_SIZE);
  String tagHex = bytesToHex(tag, AES_TAG_SIZE);

  DynamicJsonDocument doc(256);
  doc["type"] = "ack";
  doc["deviceId"] = deviceId;
  doc["seq"] = seq;
  doc["iv"] = ivHex;
  doc["tag"] = tagHex;

  String jsonStr;
  serializeJson(doc, jsonStr);
  uint16_t crc = calcCRC16(jsonStr);
  char crcBuf[8];
  sprintf(crcBuf, "%04X", crc);
  doc["crc"] = String(crcBuf);

  jsonStr = "";
  serializeJson(doc, jsonStr);
  return jsonStr;
}