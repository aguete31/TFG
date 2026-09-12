## COMANDOS PARA SUBIR Y EJECUTAR CÓDIGO EN PLACAS

### Subir firmware a la Heltec transmisora

```bash
pio run -e heltec_tx -t upload
```

Compila el proyecto usando el entorno `heltec_tx` definido en `platformio.ini` y, si la compilación es correcta, sube el firmware a la placa Heltec conectada.

---

### Abrir el monitor serie de la Heltec

```bash
pio device monitor -e heltec_tx
```

Abre el monitor del puerto serie usando la configuración del entorno `heltec_tx`. Permite ver mensajes enviados por la placa mediante `Serial`, como información de depuración, estado del dispositivo, errores o datos transmitidos.

---

### Subir firmware a la TTGO receptora

```bash
pio run -e ttgo_rx -t upload
```

Compila el código correspondiente al entorno `ttgo_rx` y lo sube a la placa TTGO receptora conectada al Mac.

---

### Abrir el monitor serie de la TTGO

```bash
pio device monitor -e ttgo_rx
```

Abre el monitor serie correspondiente al entorno `ttgo_rx`. Se utiliza para comprobar en tiempo real qué está haciendo la placa, qué datos recibe y si aparecen errores durante la ejecución.

---

### Subir el sistema de archivos a la TTGO

```bash
pio run -e ttgo_rx --target uploadfs
```

Genera y sube a la memoria de la placa el sistema de archivos definido para el proyecto, por ejemplo `LittleFS` o `SPIFFS`.

Este comando se utiliza cuando la placa necesita archivos adicionales que no forman parte directamente del firmware, como:

* páginas HTML;
* archivos CSS o JavaScript;
* imágenes;
* archivos JSON;
* configuraciones;
* otros recursos almacenados en la carpeta `data/`.

Es importante distinguirlo de `upload`: `upload` carga el **firmware**, mientras que `uploadfs` carga los **archivos del sistema de archivos**.

---

### Subir el sistema de archivos a la Heltec

```bash
pio run -e heltec_tx --target uploadfs
```

Genera y sube a la memoria de la placa el sistema de archivos definido para el proyecto, por ejemplo `LittleFS` o `SPIFFS`.

Este comando se utiliza cuando la placa necesita archivos adicionales que no forman parte directamente del firmware, como:

* páginas HTML;
* archivos CSS o JavaScript;
* imágenes;
* archivos JSON;
* configuraciones;
* otros recursos almacenados en la carpeta `data/`.

Es importante distinguirlo de `upload`: `upload` carga el **firmware**, mientras que `uploadfs` carga los **archivos del sistema de archivos**.

TTGO / Gateway
pio device monitor --port /dev/cu.usbserial-0228164B --baud 115200

Heltec / TX
pio device monitor --port /dev/cu.usbserial-0001 --baud 115200