## COMANDOS PARA SUBIR Y EJECUTAR CODIGO EN PLACAS
pio run -e heltec_tx -t upload
pio device monitor -e heltec_tx

pio run -e ttgo_rx -t upload
pio device monitor -e ttgo_rx


pio run -e ttgo_rx --target uploadfs