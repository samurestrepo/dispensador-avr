# Simulación en Proteus 8.13

El Mega ejecuta **el mismo programa del montaje**: el código de `dispensador/` no cambia. El RC522 no tiene modelo en Proteus, así que lo reemplaza un **ATmega328P que se comporta como el RC522** en el bus SPI (carpeta `emulador_rc522/`, solo para simular).

## Archivos
| Archivo | Qué es |
|---|---|
| `hex_mega/modo0.hex` … `modo5.hex` | Sketch compilado con `-DMODO=N` (0 = programa completo, 1 LCD, 2 teclado, 3 monedas, 4 LEDs, 5 RFID) |
| `emulador_rc522/emulador_rc522.hex` | Firmware del emulador (ATmega328P a **64 MHz**) |
| `emulador_rc522/rc522_logica.c/.h` | Registros del RC522 + tarjeta MIFARE 1K emulados |
| `emulador_rc522/main.c` | SPI esclavo, interruptores y registro por la USART |
| `emulador_rc522/prueba_pc/` | Prueba en el PC: librería MFRC522 real + `tarjeta.cpp` real contra el emulador (23 verificaciones) |

`*.hex` está en `.gitignore`: en el repositorio solo van las fuentes. Para regenerar:
```
arduino-cli compile --fqbn arduino:avr:mega --export-binaries --build-property "build.extra_flags=-DMODO=N" dispensador
cd simulacion/emulador_rc522 && ./compilar.sh      (necesita avr-gcc)
```

## Conexiones del esquema
| Bloque | Mega | Otro lado |
|---|---|---|
| LCD D0–D7 | 22–29 | LM016L D0–D7 · VSS y RW a GND |
| LCD RS / E | 37 / 36 | RS / E |
| Teclado filas | 47, 48, 49, 46 | fila 1 (7 8 9), 2 (4 5 6), 3 (1 2 3), 4 (ON 0 =) |
| Teclado columnas | 45, 44, 43, 42 | columnas 1–4 |
| Monedas $100/$200/$500 | 41 / 40 / 39 | LOGICSTATE |
| LED agua (verde) / gaseosa (azul) | 35 / 34 | LED → 220 Ω → GND |
| SPI SS / SCK / MOSI / MISO | 53 / 52 / 51 / 50 | ATmega328P PB2 / PB5 / PB3 / PB4 |
| RST del RC522 | 5 | sin conectar (la librería sigue igual) |

**Teclado:** el KEYPAD-SMALLCALC de Proteus tiene otras etiquetas. Las filas se cablearon para que los **números coincidan**. La columna de letras queda así: **÷ = C · × = B · − = A · + = D · ON/C = \* · = = #**. En el entrenador, el teclado va según el mapa de la bitácora (FIL A = pin 49).

## Emulador del RC522
Propiedades del ATmega328P: Program File = `emulador_rc522.hex`, **CKSEL = Ext. Clock, CKDIV8 = Unprogrammed, Clock Frequency = 64MHz**.

| Interruptor (PC0–PC3) | 0 | 1 |
|---|---|---|
| TARJETA | sin tarjeta | tarjeta en el campo |
| TARJETA_B | tarjeta A (UID DE AD BE EF, $5.000) | tarjeta B (UID 12 34 56 78, en blanco) |
| FALLA | normal | la próxima escritura no recibe ACK (D8) |
| FALLA_GUARDA | (con FALLA) los datos NO quedan | (con FALLA) los datos SÍ quedan |

El VIRTUAL TERMINAL (9600 8N1) muestra lo que pasa en la "tarjeta": lecturas, escrituras con el saldo, HALT. Los saldos viven en la RAM del emulador: al reiniciar la simulación, A vuelve a $5.000 y B queda en blanco.

**Por qué 64 MHz:** la librería usa SPI a 4 MHz, y entre dos bytes el Mega deja menos de 1 µs para cargar la respuesta. A 16 MHz no alcanza (Proteus avisa "SPDR Write collision"). Proteus no respeta 160 MHz (la USART sale corrida); con 64 MHz no hubo colisiones. El precio es que la simulación corre unas 10 veces más lenta que el tiempo real.

## Qué se probó (25-nov)
- MODO 1: LCD ✔ · MODO 2: las 16 teclas ✔
- Programa completo: consulta de saldo (C) → "Saldo $5000" ✔ · compra de gaseosa con tarjeta → leyó $5.000, escribió $4.300, verificó, HALT, LED azul ✔ · compra de agua con moneda de $500 → LED verde ✔
- Solo en el PC (no en Proteus): tarjeta B en blanco, las dos fallas de escritura, otra tarjeta en VERIFICAR.

**Lo que la simulación NO prueba:** la tarjeta real (tipo 1K, clave A), la alimentación a 3,3 V, el nivel activo de los switches del entrenador ni las pull-ups reales. Eso se confirma en el montaje.
