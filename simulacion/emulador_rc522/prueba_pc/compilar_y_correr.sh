#!/bin/bash
# Prueba en el PC: librería MFRC522 1.4.12 + tarjeta.cpp/saldo.cpp del proyecto contra el emulador.
# Uso: ./compilar_y_correr.sh <ruta a src de MFRC522>
LIB=${1:-"$HOME/Documents/Arduino/libraries/MFRC522/src"}
D=../../../dispensador
g++ -std=gnu++11 -I stubs -I $D -I "$LIB" -fpermissive -w prueba_pc.cpp "$LIB/MFRC522.cpp" $D/tarjeta.cpp $D/saldo.cpp -x c ../rc522_logica.c -o prueba && ./prueba
