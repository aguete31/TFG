#pragma once
#include <Arduino.h>
#include <SPI.h>
#include <LoRa.h>

/**
 * @brief Frecuencia por defecto para LoRa (868 MHz en Europa).
 */
#ifndef LORA_FREQ_HZ
#define LORA_FREQ_HZ 868E6
#endif

/**
 * @brief Spreading Factor por defecto (7 a 12).
 * Valores más altos aumentan alcance pero reducen velocidad.
 */
#ifndef LORA_SF
  #define LORA_SF 7        
#endif

/**
 * @brief Ancho de banda por defecto (125 kHz, 250 kHz o 500 kHz).
 */
#ifndef LORA_BW
  #define LORA_BW 125E3      
#endif

/**
 * @brief Coding Rate por defecto (valores 5 a 8, equivalen a 4/5 a 4/8).
 * Mayor valor mejora robustez a costa de velocidad.
 */
#ifndef LORA_CR
  #define LORA_CR 5         
#endif

/**
 * @brief Potencia de transmisión por defecto en dBm.
 * En Europa, máximo legal es 14 dBm (ETS).
 */
#ifndef LORA_TXPWR
  #define LORA_TXPWR 14    
#endif

#if defined(BOARD_HELTEC_V1)
  #define LORA_SS   18
  #define LORA_RST  14
  #define LORA_DIO0 26
  #define LORA_SCK  5
  #define LORA_MISO 19
  #define LORA_MOSI 27
#elif defined(BOARD_TBEAM_V1_1)
  #define LORA_SS   18
  #define LORA_RST  23
  #define LORA_DIO0 26
  #define LORA_SCK  5
  #define LORA_MISO 19
  #define LORA_MOSI 27
#else
  #error "Define BOARD_HELTEC_V1 o BOARD_TBEAM_V1_1"
#endif

/**
 * @brief Inicializa LoRa con los parámetros definidos.
 *
 * Configura SPI, pines, frecuencia, SF, BW, CR, TX power y habilita CRC.
 * Si falla la inicialización, se queda en bucle infinito.
 */
inline void lora_begin_basic() {
    SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_SS);
    LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);

    if (!LoRa.begin(LORA_FREQ_HZ))
    {
      Serial.println("LoRa init failed");
      while (true)
      {
        delay(1000);
      }
    }

    LoRa.setSpreadingFactor(LORA_SF);
    LoRa.setSignalBandwidth(LORA_BW);
    LoRa.setCodingRate4(LORA_CR);
    LoRa.setTxPower(LORA_TXPWR);
    LoRa.setSyncWord(LORA_SYNC_WORD);
    LoRa.enableCrc();
    
    Serial.printf("LoRa OK (SF=%d BW=%.0f CR=4/%d TX=%d dBm)\n", LORA_SF, (double)LORA_BW, LORA_CR, LORA_TXPWR);
}