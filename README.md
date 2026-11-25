# dispensador-avr

Dispensador de bebidas sobre **Arduino Mega 2560 (ATmega2560)**: pago con monedas (switches) y con tarjeta RFID **RC522**, LCD 16x2 y teclado 4x4 manejados **por registros en C**, y una máquina de estados no bloqueante.

Proyecto final de **Microprocesadores (60G8F)**, Ingeniería de Sistemas y Computación, Universidad de Caldas, 2026-II.

## Funcionamiento
- Productos: **agua $500** y **gaseosa $700**. Se pueden pedir varias unidades con un solo pago.
- El LCD muestra el costo de cada producto y el total. El pedido se confirma, y se puede quitar una unidad a la vez.
- **Monedas** de $100, $200 y $500, una por switch. Si la última moneda se pasa del total, se devuelve y se pide el valor restante.
- **Tarjeta RFID:** el saldo se guarda en la tarjeta. Si no alcanza, el pago se rechaza. Hay un modo de recarga protegido con clave.
- Un LED por producto simula el relé de salida.

## Estructura
```
MANUAL_TECNICO.md  documentación completa del sistema tal como está implementado
CLAUDE.md        reglas de programación del repositorio
diseño/          requisitos, decisiones, pines, máquina de estados, arquitectura y pruebas
dispensador/     el sketch (lo único que se abre en el Arduino IDE)
pruebas_pc/      pruebas en PC con g++: pedido, saldo y máquina de estados (sin hardware)
referencia/      código anterior probado en el entrenador (origen del LCD y del teclado)
simulacion/      simulación en Proteus 8.13 (ver simulacion/README.md)
```
**Documentación completa (arquitectura, máquina de estados, trazabilidad, cómo modificar, guion de prueba): [`MANUAL_TECNICO.md`](MANUAL_TECNICO.md).** El diseño original está en [`diseño/00_indice.md`](diseño/00_indice.md).

## Requisitos
| Herramienta | Versión probada |
|---|---|
| Arduino IDE 2 o `arduino-cli` | arduino-cli 1.6.0 |
| Core `arduino:avr` | 1.8.8 |
| Librería `MFRC522` (GithubCommunity) | 1.4.12 |
| g++ (solo para `pruebas_pc/`) | WinLibs GCC |

**Sin la librería MFRC522 no compila.** En el Arduino IDE: *Herramientas → Administrar bibliotecas* → `MFRC522` → versión **1.4.12**. Por línea de comandos:
```
arduino-cli core install arduino:avr
arduino-cli lib install MFRC522@1.4.12
```

## Compilar y generar el .hex
**Arduino IDE:** abrir `dispensador/dispensador.ino` → placa *Arduino Mega or Mega 2560* → **Programa → Exportar binario compilado**.

**Línea de comandos:**
```
arduino-cli compile --fqbn arduino:avr:mega --export-binaries dispensador
```
El .hex queda en `dispensador/build/arduino.avr.mega/`. Para Proteus se usa `dispensador.ino.hex` (no el `with_bootloader`).

## Simulación
Pasos completos (librería, exportar el .hex, abrir el `.pdsprj`, equivalencia de teclas): [`simulacion/README.md`](simulacion/README.md).

## Modos de prueba
En `dispensador/config.h`, la constante `MODO` elige qué se compila: el programa completo o una prueba de cada periférico (LCD, teclado, monedas, LEDs, RFID). Se recompila después de cambiarla.

## Conexiones
La tabla completa está en [`diseño/04_pines_y_montaje.md`](diseño/04_pines_y_montaje.md).
⚠️ El RC522 se alimenta del pin **3.3V** del Mega, **nunca de 5V**.

## Autores
Samuel Restrepo Ramírez y compañero de grupo.
