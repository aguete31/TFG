#include "gateway_time.h"
#include <time.h>

static bool     s_time_synced   = false;
static uint64_t s_boot_millis   = 0;
static uint64_t s_boot_epoch    = 0;   // epoch real cuando se sincronizó

void gateway_time_init()
{
    // Zona horaria España (CET / CEST)
    setenv("TZ", "CET-1CEST,M3.5.0/2,M10.5.0/3", 1);
    tzset();

    // Servidores NTP
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");

    Serial.print("[TIME] Sincronizando hora NTP");
    time_t now = 0;
    int retries = 0;

    while (retries < 30) {       // ~15s máximo
        delay(500);
        time(&now);
        Serial.print(".");
        if (now >= 1700000000) { // ~2023-11-14, "hora razonable"
            s_time_synced = true;
            break;
        }
        retries++;
    }
    Serial.println();

    s_boot_millis = millis();

    if (s_time_synced) {
        s_boot_epoch = (uint64_t) now;
        Serial.printf("[TIME] Hora sincronizada: %s", ctime(&now));
    } else {
        s_boot_epoch = 0;
        Serial.println("[TIME] NO se pudo sincronizar la hora, usando millis() como fallback");
    }
}

bool gateway_time_is_synced()
{
    return s_time_synced;
}

uint64_t gateway_time_now()
{
    uint64_t elapsed_ms = millis() - s_boot_millis;

    if (s_time_synced) {
        // epoch real + tiempo transcurrido
        return s_boot_epoch + elapsed_ms / 1000ULL;
    } else {
        // fallback: segundos desde arranque (no real, pero monótono)
        return elapsed_ms / 1000ULL;
    }
}
