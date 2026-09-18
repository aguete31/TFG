#include "power_manager.h"

#include <Arduino.h>
#include <Wire.h>
#include <XPowersLib.h>

static XPowersAXP192 g_power(Wire, 21, 22);

bool powerManagerInit()
{
    Serial.println("PMU: inicializando AXP192...");

    if (!g_power.init())
    {
        Serial.println("PMU: AXP192 no detectado");
        return false;
    }

    g_power.setLDO2Voltage(3300);
    g_power.enableLDO2();

    Serial.printf("PMU: LDO2 enabled=%s voltage=%u mV\n", g_power.isEnableLDO2() ? "yes" : "no",g_power.getLDO2Voltage());
    return true;
}