# Simulación en Proteus 8.13

El Mega ejecuta **el mismo programa del montaje**: el código de `dispensador/` no cambia. El RC522 no tiene modelo en Proteus, así que lo reemplaza un **ATmega328P que se comporta como el RC522** en el bus SPI (carpeta `emulador_rc522/`, solo para simular).

## Cómo correr la simulación (desde el Arduino IDE)
1. **Librería MFRC522 1.4.12** (sin ella no compila): Arduino IDE → *Herramientas → Administrar bibliotecas* → buscar `MFRC522` (de GithubCommunity) → instalar la versión **1.4.12**.
2. Abrir `dispensador/dispensador.ino` → placa **Arduino Mega or Mega 2560** → **Sketch → Export Compiled Binary**. El .hex queda en `dispensador/build/arduino.avr.mega/dispensador.ino.hex`.
3. Abrir `simulacion/SimulacionProyectoDispensadorAVR.pdsprj` en Proteus 8.13 y darle *Play*. No hay que tocar rutas: el *Program File* de los dos micros es **relativo al repositorio**:
   - Mega: `..\dispensador\build\arduino.avr.mega\dispensador.ino.hex`
   - ATmega328P: `emulador_rc522\emulador_rc522.hex` (**este .hex sí está en el repo**; no hace falta avr-gcc).
4. Para un modo de prueba (1 LCD, 2 teclado, 3 monedas, 4 LEDs, 5 RFID): cambiar `MODO` en `dispensador/config.h`, volver a exportar el binario y reiniciar la simulación. **Dejarlo en `MODO_NORMAL` al terminar.**

**Ojo con el teclado:** la simulación va ~10 veces más lenta que el tiempo real y el antirrebote pide 20 ms *simulados*: mantener el clic en cada tecla **medio segundo**. Un clic muy corto puede no registrarse.

## Archivos
| Archivo | Qué es |
|---|---|
| `emulador_rc522/emulador_rc522.hex` | Firmware del emulador (ATmega328P a **64 MHz**), ya compilado y en el repo |
| `emulador_rc522/rc522_logica.c/.h` | Registros del RC522 + tarjeta MIFARE 1K emulados |
| `emulador_rc522/main.c` | SPI esclavo, interruptores y registro por la USART |
| `emulador_rc522/prueba_pc/` | Prueba en el PC: librería MFRC522 real + `tarjeta.cpp` real contra el emulador (23 verificaciones) |

Los demás `*.hex` están en `.gitignore`. Para regenerar el emulador (solo si se cambia `main.c` o `rc522_logica.c`): `cd simulacion/emulador_rc522 && ./compilar.sh` (necesita `avr-gcc`; el que trae el Arduino IDE sirve). Por línea de comandos, el sketch se compila con `arduino-cli compile --fqbn arduino:avr:mega --export-binaries dispensador`.

## Conexiones del esquema
| Bloque | Mega | Otro lado |
|---|---|---|
| LCD D0–D7 | 22–29 | LM016L D0–D7 · VSS y RW a GND |
| LCD RS / E | 37 / 36 | RS / E |
| Teclado filas | 49, 48, 47, 46 (FIL A–D) | fila 1 (7 8 9 ÷), 2 (4 5 6 ×), 3 (1 2 3 −), 4 (ON/C 0 = +) |
| Teclado columnas | 45, 44, 43, 42 | columnas 1–4 |
| Monedas $100/$200/$500 | 41 / 40 / 39 | LOGICSTATE |
| LED agua (verde) / gaseosa (azul) | 35 / 34 | LED → 220 Ω → GND |
| SPI SS / SCK / MOSI / MISO | 53 / 52 / 51 / 50 | ATmega328P PB2 / PB5 / PB3 / PB4 |
| RST del RC522 | 5 | sin conectar (la librería sigue igual) |

**Teclado:** el KEYPAD-SMALLCALC de Proteus es de calculadora (el 7 arriba). Las filas se cablearon **igual que en el entrenador** (FIL A = pin 49 = fila de arriba), para que el esquema sirva de referencia del montaje. Por eso **los números NO coinciden**: la fila de arriba de Proteus es la fila `1 2 3 A` del programa.

| Proteus | 7 | 8 | 9 | ÷ | 4 | 5 | 6 | × | 1 | 2 | 3 | − | ON/C | 0 | = | + |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| **Programa** | 1 | 2 | 3 | A | 4 | 5 | 6 | B | 7 | 8 | 9 | C | \* | 0 | # | D |

Uso rápido en Proteus:
| Acción | Tecla del programa | Se presiona en Proteus |
|---|---|---|
| Agregar agua / gaseosa | 1 / 2 | **7 / 8** |
| Quitar agua / gaseosa | 4 / 5 | 4 / 5 |
| Confirmar | # | **=** |
| Cancelar / volver | \* | **ON/C** |
| Pagar con monedas / tarjeta | A / B | **÷ / ×** |
| Consultar saldo | C | **−** |
| Recarga | D | **+** |
| Clave de recarga `1234` | 1 2 3 4 | **7 8 9 4**, luego **=** |
| Monto (ej. $1.000) | 1 0 0 0 | **7 0 0 0**, luego **=** |

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
