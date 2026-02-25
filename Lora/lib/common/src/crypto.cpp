// lib/common/src/crypto.cpp
#include "crypto.h"

#include <mbedtls/gcm.h>
#include <mbedtls/aes.h>
#include <mbedtls/pkcs5.h>
#include <mbedtls/sha256.h>
#include <mbedtls/md.h>
#include <mbedtls/hkdf.h>

#include <Preferences.h>
#include <esp_system.h>

#include <string.h>
#include <stdlib.h>

// Fallback si no está definido en crypto.h
#ifndef DEFAULT_PBKDF2_ITERS
#define DEFAULT_PBKDF2_ITERS 5000
#endif

// Definición de la clave AES-128 (tamaño definido en crypto.h)
uint8_t AES_KEY[AES_KEY_SIZE];
static volatile bool g_key_ready = false;

static const char *NVS_CRYPTO_NAMESPACE = "lora_crypto";
static const char *NVS_KEY_DEVKEY = "dev_key";

/**
 * @brief Devuelve si la clave AES global está lista para usar.
 */
bool is_key_ready()
{
  return g_key_ready;
}

/**
 * @brief Establece el estado de disponibilidad de la clave AES.
 */
void set_key_ready(bool v)
{
  g_key_ready = v;
}

/**
 * @brief Limpia un buffer de forma segura (evitando optimización del compilador).
 * Útil para borrar contraseñas temporales o claves sensibles.
 */
static void secure_zero(void *v, size_t n)
{
  volatile uint8_t *p = (volatile uint8_t *)v;
  while (n--)
    *p++ = 0;
}

void generate_random_key(uint8_t *out_key, size_t key_len)
{
  if (!out_key || key_len == 0)
    return;

  // Rellenar con esp_random() en bloques de 4 bytes
  for (size_t i = 0; i < key_len; i += 4)
  {
    uint32_t r = esp_random();
    size_t chunk = (key_len - i) < 4 ? (key_len - i) : 4;
    memcpy(out_key + i, &r, chunk);
  }
}

/**
 * @brief Deriva una clave AES-128 a partir de una contraseña usando PBKDF2-HMAC-SHA256.
 *
 * Pasos:
 *  1. Copia la contraseña a un buffer temporal.
 *  2. Inicializa contexto HMAC-SHA256.
 *  3. Ejecuta PBKDF2 con salt fijo y 5000 iteraciones.
 *  4. Limpia y libera buffer temporal.
 *  5. Marca clave como lista si todo va bien.
 *
 * @param password Contraseña de entrada
 * @param out_key Buffer de salida donde se escribirá la clave (16 bytes)
 * @param key_len Longitud de la clave (debe ser 16)
 * @return true si la derivación fue exitosa
 */
bool derive_key_from_password(const String &password, uint8_t *out_key, size_t key_len)
{
  const char *salt = "LoRaPair-v1"; // Salt fijo / versión del esquema
  const size_t salt_len = strlen(salt);
  const unsigned int iterations = 5000;

  if (!out_key || key_len == 0)
    return false;

  // Copiar contraseña a buffer nativo para poder borrarla después
  size_t pw_len = password.length();
  unsigned char *pw_buf = nullptr;
  if (pw_len > 0)
  {
    pw_buf = (unsigned char *)malloc(pw_len);
    if (!pw_buf)
      return false;
    memcpy(pw_buf, password.c_str(), pw_len);
  }

  // Preparar contexto md (HMAC-SHA256)
  mbedtls_md_context_t md_ctx;
  mbedtls_md_init(&md_ctx);

  const mbedtls_md_info_t *md_info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
  if (!md_info)
  {
    if (pw_buf)
    {
      secure_zero(pw_buf, pw_len);
      free(pw_buf);
    }
    mbedtls_md_free(&md_ctx);
    set_key_ready(false);
    return false;
  }

  if (mbedtls_md_setup(&md_ctx, md_info, 1) != 0)
  {
    if (pw_buf)
    {
      secure_zero(pw_buf, pw_len);
      free(pw_buf);
    }
    mbedtls_md_free(&md_ctx);
    set_key_ready(false);
    return false;
  }

  // Ejecutar PBKDF2-HMAC-SHA256 (nota: pasamos &md_ctx)
  int ret = mbedtls_pkcs5_pbkdf2_hmac(
      &md_ctx,
      pw_buf ? pw_buf : (const unsigned char *)"", (size_t)pw_len,
      (const unsigned char *)salt, salt_len,
      iterations,
      (uint32_t)key_len,
      out_key);

  // Borrar y liberar buffer de contraseña
  if (pw_buf)
  {
    secure_zero(pw_buf, pw_len);
    free(pw_buf);
  }

  // Liberar contexto md
  mbedtls_md_free(&md_ctx);

  if (ret != 0)
  {
    // En caso de fallo, limpiar clave de salida por seguridad y actualizar flag
    secure_zero(out_key, key_len);
    set_key_ready(false);
    return false;
  }

  // Derivación OK: marcar clave lista
  set_key_ready(true);
  return true;
}

/**
 * @brief Cifra datos usando AES-GCM con mbed TLS.
 *
 * @param plaintext Datos a cifrar
 * @param plaintext_len Longitud de los datos
 * @param key Clave AES (16 bytes)
 * @param iv Vector de inicialización (12 bytes)
 * @param iv_len Longitud del IV
 * @param aad Datos adicionales autenticados (pueden ser nullptr si aad_len=0)
 * @param aad_len Longitud de AAD
 * @param ciphertext Buffer de salida para el ciphertext
 * @param tag Buffer de salida para el tag de autenticación (16 bytes)
 * @return true si cifrado y generación de tag fueron exitosos
 */
bool aes_gcm_encrypt(const uint8_t *plaintext, size_t plaintext_len,
                     const uint8_t *key,
                     const uint8_t *iv, size_t iv_len,
                     const uint8_t *aad, size_t aad_len,
                     uint8_t *ciphertext,
                     uint8_t *tag)
{
  if (!plaintext || (plaintext_len && !ciphertext) || !key || !iv || !tag)
    return false;

  mbedtls_gcm_context gcm;
  mbedtls_gcm_init(&gcm);

  int ret = mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, key, (int)(AES_KEY_SIZE * 8));
  if (ret != 0)
  {
    mbedtls_gcm_free(&gcm);
    return false;
  }

  ret = mbedtls_gcm_crypt_and_tag(&gcm, MBEDTLS_GCM_ENCRYPT, plaintext_len, iv, iv_len, aad, aad_len, plaintext, ciphertext, AES_TAG_SIZE, tag);

  mbedtls_gcm_free(&gcm);
  return (ret == 0);
}

/**
 * @brief Descifra y autentica datos usando AES-GCM con mbed TLS.
 *
 * @param ciphertext Datos cifrados
 * @param ciphertext_len Longitud de los datos cifrados
 * @param key Clave AES (16 bytes)
 * @param iv Vector de inicialización (12 bytes)
 * @param iv_len Longitud del IV
 * @param aad Datos adicionales autenticados (pueden ser nullptr si aad_len=0)
 * @param aad_len Longitud de AAD
 * @param tag Tag de autenticación recibido
 * @param plaintext Buffer de salida para el plaintext
 * @return true si autenticación y descifrado fueron exitosos
 */
bool aes_gcm_decrypt(const uint8_t *ciphertext, size_t ciphertext_len,
                     const uint8_t *key,
                     const uint8_t *iv, size_t iv_len,
                     const uint8_t *aad, size_t aad_len,
                     const uint8_t *tag,
                     uint8_t *plaintext)
{
  if (!ciphertext || (ciphertext_len && !plaintext) || !key || !iv || !tag)
    return false;

  mbedtls_gcm_context gcm;
  mbedtls_gcm_init(&gcm);

  int ret = mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, key, (int)(AES_KEY_SIZE * 8));
  if (ret != 0)
  {
    mbedtls_gcm_free(&gcm);
    return false;
  }

  ret = mbedtls_gcm_auth_decrypt(&gcm, ciphertext_len, iv, iv_len, aad, aad_len, tag, AES_TAG_SIZE, ciphertext, plaintext);

  mbedtls_gcm_free(&gcm);
  return (ret == 0);
}

/**
 * @brief Limpia la clave AES global y marca como no lista.
 * Útil para borrar la clave al reiniciar o cerrar sesión.
 */
void clear_aes_key()
{
  secure_zero(AES_KEY, AES_KEY_SIZE);
  set_key_ready(false);
}

bool store_device_key_in_nvs(const uint8_t *key, size_t key_len)
{
  if (!key || key_len != AES_KEY_SIZE)
    return false;

  Preferences prefs;
  prefs.begin(NVS_CRYPTO_NAMESPACE, false); // escritura
  size_t w = prefs.putBytes(NVS_KEY_DEVKEY, key, key_len);
  prefs.end();

  return (w == key_len);
}

bool load_device_key_from_nvs()
{
  Preferences prefs;
  prefs.begin(NVS_CRYPTO_NAMESPACE, true); // solo lectura

  uint8_t tmp[AES_KEY_SIZE];
  size_t r = prefs.getBytes(NVS_KEY_DEVKEY, tmp, AES_KEY_SIZE);
  prefs.end();

  if (r != AES_KEY_SIZE)
  {
    clear_aes_key();
    return false;
  }

  // Copiar a AES_KEY y limpiar temporal
  memcpy(AES_KEY, tmp, AES_KEY_SIZE);
  secure_zero(tmp, AES_KEY_SIZE);
  set_key_ready(true);
  return true;
}
