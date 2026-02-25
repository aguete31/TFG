#include "setup.h"

namespace common {
  
/**
 * @brief Inicializa el puerto serie y espera a que esté listo (opcional).
 *
 * @param baudrate Velocidad en baudios (ej. 115200).
 * @param timeoutMs Tiempo máximo de espera en ms (0 para no esperar).
 */
void initSerial(uint32_t baudrate, unsigned long timeoutMs) {
  Serial.begin(baudrate);
  Serial.println("Puerto OK");

#if defined(ARDUINO_ARCH_ESP32)
  Serial.setDebugOutput(true);
#endif

  const unsigned long start = millis();
  while (!Serial && (millis() - start) < timeoutMs) {
    delay(10);
  }
}

}  