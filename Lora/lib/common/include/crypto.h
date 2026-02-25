// crypto.h
#ifndef CRYPTO_H
#define CRYPTO_H

#include <Arduino.h>

/**
 * @brief Tamaños fijos usados en AES-GCM:
 *  - AES_KEY_SIZE: 16 bytes (clave AES-128)
 *  - AES_IV_SIZE: 12 bytes (recomendado para GCM)
 *  - AES_TAG_SIZE: 16 bytes (tamaño del tag de autenticación)
 */
#define AES_KEY_SIZE 16
#define AES_IV_SIZE 12
#define AES_TAG_SIZE 16

/**
 * @brief Clave AES global usada para cifrar/descifrar todos los mensajes.
 *
 * Se define en crypto.cpp y se inicializa mediante derive_key_from_password().
 * Acceso externo permitido, pero debe protegerse en producción.
 */
extern uint8_t AES_KEY[AES_KEY_SIZE];

/**
 * @brief Deriva una clave criptográfica a partir de una contraseña usando PBKDF2-HMAC-SHA256.
 *
 * Internamente usa salt fijo ("LoRaPair-v1") y 5000 iteraciones.
 * Rellena out_key con la clave derivada y marca is_key_ready() como true si tiene éxito.
 *
 * @param password Contraseña de entrada (String de Arduino)
 * @param out_key Buffer de salida donde se escribirá la clave (debe tener tamaño key_len)
 * @param key_len Longitud deseada de la clave (normalmente AES_KEY_SIZE)
 * @return true si la derivación fue exitosa, false en caso de error
 */
bool derive_key_from_password(const String &password, uint8_t* out_key, size_t key_len);

/**
 * @brief Indica si la clave AES ha sido cargada/derivada y está lista para usar.
 * @return true si la clave está disponible, false si no
 */
bool is_key_ready();  

/**
 * @brief Marca manualmente si la clave está lista (usado internamente tras derive_key_from_password).
 * @param v true si la clave está lista, false si no
 */
void set_key_ready(bool v);   

/**
 * @brief Cifra datos usando AES-GCM.
 *
 * @param plaintext Datos a cifrar
 * @param plaintext_len Longitud de los datos
 * @param key Clave AES (16 bytes)
 * @param iv Vector de inicialización (12 bytes recomendado)
 * @param iv_len Longitud del IV
 * @param aad Datos adicionales autenticados (pueden ser nullptr si aad_len=0)
 * @param aad_len Longitud de AAD
 * @param ciphertext Buffer de salida para el ciphertext (misma longitud que plaintext)
 * @param tag Buffer de salida para el tag de autenticación (16 bytes)
 * @return true si cifrado y generación de tag fueron exitosos
 */
bool aes_gcm_encrypt(const uint8_t* plaintext, size_t plaintext_len,
                     const uint8_t* key,
                     const uint8_t* iv, size_t iv_len,
                     const uint8_t* aad, size_t aad_len,
                     uint8_t* ciphertext,
                     uint8_t* tag);

/**
 * @brief Descifra y autentica datos usando AES-GCM.
 *
 * @param ciphertext Datos cifrados
 * @param ciphertext_len Longitud de los datos cifrados
 * @param key Clave AES (16 bytes)
 * @param iv Vector de inicialización (12 bytes)
 * @param iv_len Longitud del IV
 * @param aad Datos adicionales autenticados (pueden ser nullptr si aad_len=0)
 * @param aad_len Longitud de AAD
 * @param tag Tag de autenticación recibido
 * @param plaintext Buffer de salida para el plaintext (misma longitud que ciphertext)
 * @return true si autenticación y descifrado fueron exitosos
 */
bool aes_gcm_decrypt(const uint8_t* ciphertext, size_t ciphertext_len,
                     const uint8_t* key,
                     const uint8_t* iv, size_t iv_len,
                     const uint8_t* aad, size_t aad_len,
                     const uint8_t* tag,
                     uint8_t* plaintext);

/**
 * @brief Genera una clave aleatoria criptográficamente fuerte.
 *
 * Usada para crear K_dev, la clave definitiva del dispositivo.
 *
 * @param out_key Buffer de salida.
 * @param key_len Longitud en bytes (normalmente AES_KEY_SIZE).
 */
void generate_random_key(uint8_t* out_key, size_t key_len);

/**
 * @brief Guarda la clave AES del dispositivo en NVS.
 *
 * Se usa para persistir K_dev. No expone la clave fuera; el patrón normal
 * será copiar a AES_KEY y luego llamar a esta función.
 *
 * @param key Buffer con la clave.
 * @param key_len Longitud de la clave (AES_KEY_SIZE).
 * @return true si se guardó correctamente.
 */
bool store_device_key_in_nvs(const uint8_t* key, size_t key_len);

/**
 * @brief Carga la clave AES del dispositivo desde NVS.
 *
 * Si existe, la copia a AES_KEY y marca is_key_ready() = true.
 *
 * @return true si se ha cargado una clave válida, false si no había o hubo error.
 */
bool load_device_key_from_nvs();

/**
 * @brief Limpia la clave AES global y marca como no lista.
 */
void clear_aes_key();

#endif 