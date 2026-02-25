# Gestor de placas

`gestor_placas.py` proporciona una interfaz gráfica ligera para compilar, subir y monitorizar los firmwares PlatformIO definidos en este proyecto.

## Requisitos
- Python 3.10+
- Dependencias: `tkinter` (incluido en CPython para la mayoría de distribuciones), `pyserial`, `platformio`

Instala PlatformIO y PySerial en el entorno de trabajo:

```bash
pip install platformio pyserial
```

## Uso
1. Ejecuta el script desde la raíz del proyecto: `python gestor_placas.py`.
2. Selecciona la placa (entorno) que quieras manejar. El gestor lista los subdirectorios de `boards/` que tengan un `src_filter` asociado en `platformio.ini`.
3. Elige el firmware (`main.cpp` o ejemplo disponible).
4. Selecciona puerto serie y velocidad.
5. Usa los botones:
   - `Compiler`: ejecuta `pio run` para el entorno seleccionado.
   - `Uploader`: parchea temporalmente `platformio.ini` con `upload_port` y `upload_speed`, lanza `pio run -t upload` y restaura el fichero.
   - `Connect`: abre un monitor serie en el mismo puerto para ver la salida.

El área inferior actúa como consola y muestra el resultado de los comandos.

## Notas
- Los entornos deben coincidir con los definidos en `platformio.ini` (`heltec_wifi_lora_32`, `ttgo_lora32_v1`, etc.).
- El script crea copias de seguridad temporales (`platformio.ini.bak`) que se eliminan automáticamente al terminar cada operación.
- Para integrar nuevas placas añade su carpeta en `boards/<placa>` y declara el nuevo `[env:<nombre>]` en `platformio.ini`; el gestor la detectará al iniciarse.
