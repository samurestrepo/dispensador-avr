# CLAUDE.md — dispensador-avr

Reglas permanentes para Claude Code en este repositorio. Léelas completas antes de cada tarea.

## Qué es
Proyecto final de Microprocesadores (Universidad de Caldas): dispensador de bebidas en un **Arduino Mega 2560 (ATmega2560, 16 MHz)** montado en un entrenador del laboratorio. El diseño completo y aprobado está en `diseño/` (empieza por `diseño/00_indice.md`). **El diseño manda:** si algo del diseño es imposible o contradictorio, detente y repórtalo; no lo cambies en silencio ni edites `diseño/` sin que te lo pidan.

El autor (Samuel) debe sustentar el proyecto **a papel e individualmente** y presentar un parcial de **C por registros a papel**. El código es también su material de estudio: claridad por encima de ingenio.

## Idioma
Comentarios, identificadores, mensajes de commit y textos del LCD en **español** (sin tildes ni ñ en el LCD: el HD44780 no las tiene).

## Restricciones del Arduino IDE (no negociables)
- El sketch vive en `dispensador/` y su archivo principal es `dispensador/dispensador.ino`.
- **Todos los `.h` y `.cpp` del sketch van planos en `dispensador/`** (sin subcarpetas: el IDE no las compila).
- Un solo sketch. Las pruebas de hardware NO son sketches aparte: se eligen con la constante `MODO` de `config.h` (ver abajo).
- Librerías externas: solo `MFRC522` (GithubCommunity, 1.4.12) y `SPI.h`. Nada más.

## Registros vs. librerías
- **Por registros:** LCD, teclado, monedas (switches) y LEDs/relés. Prohibido en esos módulos: `pinMode`, `digitalWrite`, `digitalRead`, `analogRead`, `delay`, `Serial`, `LiquidCrystal`, `Keypad`.
- **Con librería:** solo el RFID (`MFRC522.h` + `SPI.h`), encapsulado en `tarjeta.cpp`.
- Tiempos: `millis()` está permitido para la máquina de estados no bloqueante. Esperas cortas del protocolo del LCD con `_delay_us` / `_delay_ms` de `<util/delay.h>`, **solo dentro de `lcd.cpp`** (y el `_delay_us(10)` del barrido del teclado).
- No definir `ISR(TIMER0_OVF_vect)` (lo usa `millis()`).
- Máscaras siempre en notación **`0b...`** (convención de la profesora). Nada de `(1<<BIT)` ni `_BV()`.
- **PORTC es compartido** (LCD en PC0–PC1, LEDs en PC2–PC3): escribir solo con `|=` y `&= ~`. Nunca `PORTC = valor`.
- Cada escritura a un registro lleva un comentario: nombre del registro, qué bits cambian y para qué. **No inventes números de página del datasheet**; si no lo verificaste, no pongas página.

## Código de referencia probado
`referencia/esclavo_usart1.ino` funcionó en este mismo entrenador. **Reutiliza el LCD y el teclado con los mismos nombres y el mismo estilo** (`pulsoE`, `instruc`, `escrib`, `texto`, `lcd_iniciar`, `teclado_iniciar`, `teclado_escanear`, `teclado_nueva`): Samuel ya los conoce. Puedes agregar funciones nuevas (p. ej. `lcd_dinero`, `lcd_pantalla`), pero no renombres ni reescribas las que ya funcionan.

## Módulos
Ver `diseño/06_arquitectura_y_pruebas.md`. Resumen:
- `config.h`: pines, precios, tiempos, clave, valores de monedas, `MONEDA_ACTIVA_EN_ALTO`, `MODO`.
- `lcd`, `teclado`, `monedas`, `reles`: registros. `tarjeta`: librería MFRC522.
- `pedido`, `saldo`: **lógica pura**. Solo `<stdint.h>` / `<stdbool.h>`; **nada de `Arduino.h` ni registros**, para que compilen con g++ en el PC.
- `fsm`: máquina de estados; usa solo las interfaces de los demás módulos; no escribe registros.
- `pruebas_hw.h/.cpp`: una función por modo de prueba de hardware.
- Ningún módulo toca los registros de otro.

## Selector de modo
En `config.h`:
```c
#define MODO_NORMAL          0
#define MODO_PRUEBA_LCD      1
#define MODO_PRUEBA_TECLADO  2
#define MODO_PRUEBA_MONEDAS  3   // muestra también el estado crudo de PG0–PG2 (para confirmar el nivel activo)
#define MODO_PRUEBA_RELES    4
#define MODO_PRUEBA_RFID     5   // UID, tipo de tarjeta y saldo del bloque 4. SOLO LECTURA.
#ifndef MODO
#define MODO MODO_NORMAL
#endif
```
`dispensador.ino` despacha `setup()`/`loop()` según `MODO`.

## Verificación obligatoria antes de dar algo por terminado
1. Compilar **cada** modo sin errores ni advertencias nuevas:
   ```
   arduino-cli compile --fqbn arduino:avr:mega --warnings all --export-binaries --build-property "build.extra_flags=-DMODO=N" dispensador
   ```
   (N = 0..5). Si `build.extra_flags` no funciona en esta plataforma, dilo y propón alternativa; no la cambies en silencio.
2. Al final, compilar `MODO_NORMAL` y reportar el uso de **flash y RAM** que imprime arduino-cli (restricción de recursos del curso).
3. Pruebas de PC (cuando existan): compilar y correr con g++ las pruebas de `pruebas_pc/`. Si g++ no está en el PATH, dilo; no lo instales tú.
4. Nunca digas que algo "funciona en la placa": solo puedes afirmar que **compila** y que **las pruebas de PC pasan**. El hardware lo prueba Samuel.

## Git
- Un commit por fase terminada, con mensaje en español que diga qué se hizo.
- **No hagas push**: Samuel revisa y hace push él mismo.
- **Sin atribución a Claude en los commits:** nada de `Co-Authored-By: Claude`, ni "Generated with Claude Code", ni ninguna mención a Claude o Anthropic en mensajes de commit, código o README. El autor es Samuel (lo toma de `git config`).
- No subas `build/`, `.hex` ni ejecutables (ya están en `.gitignore`).

## Entorno
Windows + PowerShell. Herramientas instaladas: git 2.45, arduino-cli 1.6.0, core arduino:avr 1.8.8, MFRC522 1.4.12, g++ (WinLibs; verificar que esté en el PATH).
