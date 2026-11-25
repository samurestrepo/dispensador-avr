# Manual técnico — dispensador-avr

Documento de referencia para entender, verificar, auditar y modificar el proyecto **sin necesidad de otro contexto**. Describe el sistema tal como está implementado en el código (commit `dbe2781`, fase 4, 20-nov-2026).

> **Estado:** el código **compila** en sus 6 modos sin advertencias y **pasan las 322 verificaciones** de las pruebas en PC. **Nada se ha probado todavía en el hardware real.** Ver §12 (supuestos abiertos).

---

## Contenido
1. [Qué es y de dónde sale](#1-qué-es-y-de-dónde-sale)
2. [Mapa del repositorio](#2-mapa-del-repositorio)
3. [Hardware y conexiones](#3-hardware-y-conexiones)
4. [Compilar, cargar, simular y probar](#4-compilar-cargar-simular-y-probar)
5. [Arquitectura](#5-arquitectura)
6. [Módulos: responsabilidad, interfaz y registros](#6-módulos-responsabilidad-interfaz-y-registros)
7. [Máquina de estados](#7-máquina-de-estados)
8. [La tarjeta: formato del saldo y operaciones seguras](#8-la-tarjeta-formato-del-saldo-y-operaciones-seguras)
9. [Tiempos y recursos](#9-tiempos-y-recursos)
10. [Trazabilidad: requisito → decisión → código → prueba](#10-trazabilidad-requisito--decisión--código--prueba)
11. [Cómo modificar el sistema (recetas)](#11-cómo-modificar-el-sistema-recetas)
12. [Supuestos abiertos, limitaciones y riesgos](#12-supuestos-abiertos-limitaciones-y-riesgos)
13. [Guion de prueba en el entrenador](#13-guion-de-prueba-en-el-entrenador)
14. [Historial y convenciones](#14-historial-y-convenciones)

---

## 1. Qué es y de dónde sale

Dispensador de bebidas sobre un **Arduino Mega 2560 (ATmega2560, 16 MHz)** montado en el entrenador del laboratorio de Microprocesadores (Universidad de Caldas, 2026-II). Proyecto final en grupos de 2, 30 % de la nota: montaje funcionando (27-nov) y sustentación individual a papel (≈ 3-dic).

**Enunciado resumido** (`Proyecto 2026_II.docx.md`, fuera del repositorio): agua $500 y gaseosa $700; varios productos con un solo pago; mostrar el costo de cada producto y el total en un LCD; confirmar el pedido y poder cancelar productos; pago con monedas de $100/$200/$500 (si la última moneda se pasa, se devuelve y se pide el restante); pago con tarjeta RFID con modo de recarga; un relé (simulado con LED) por producto.

**Respuestas de la profesora** que completan el enunciado (20-nov): el saldo se guarda **en la tarjeta**; el modo de recarga lo definimos nosotros y el monto se digita en el teclado; el teclado 4x4 sirve para elegir, confirmar y cancelar, pero **las monedas van con switches** (uno por moneda); cancelar = **quitar una unidad**; si el saldo no alcanza, **se rechaza**; los demás detalles los decidimos nosotros si tienen sentido funcional y están sustentados. Se permiten todas las librerías.

**Regla del curso:** el curso exige programar **por registros**. Por eso todo el hardware simple se maneja por registros; solo el lector RFID usa librería (ver D1).

---

## 2. Mapa del repositorio

```
├── README.md                 presentación corta, requisitos y cómo compilar
├── MANUAL_TECNICO.md         este documento
├── CLAUDE.md                 reglas de programación del repositorio (aplican a cualquier persona o agente)
├── diseño/                   el diseño aprobado: la especificación
│   ├── 00_indice.md
│   ├── 01_requisitos.md      R1–R12
│   ├── 02_materiales.md
│   ├── 03_decisiones.md      D1–D16, con su justificación
│   ├── 04_pines_y_montaje.md mapa de pines, niveles del RC522, verificación antes de energizar
│   ├── 05_teclado_y_estados.md teclas y tabla de estados
│   └── 06_arquitectura_y_pruebas.md
├── dispensador/              EL sketch (lo único que abre el Arduino IDE)
│   ├── dispensador.ino       setup()/loop(); despacha según MODO
│   ├── config.h              TODAS las constantes: pines, precios, tiempos, clave, MODO
│   ├── lcd.h/.cpp            registros
│   ├── teclado.h/.cpp        registros
│   ├── monedas.h/.cpp        registros
│   ├── reles.h/.cpp          registros
│   ├── tarjeta.h/.cpp        librería MFRC522 (único módulo con librería)
│   ├── pedido.h/.cpp         lógica pura
│   ├── saldo.h/.cpp          lógica pura
│   ├── fsm.h/.cpp            máquina de estados (sin registros, sin Arduino.h)
│   └── pruebas_hw.h/.cpp     modos de prueba de hardware 1–5
├── pruebas_pc/               pruebas en PC con g++ (sin hardware)
│   ├── mini_prueba.h         mini framework (VERIFICAR, resumen)
│   ├── prueba_pedido.cpp     95 verificaciones
│   ├── prueba_saldo.cpp      67 verificaciones
│   ├── prueba_fsm.cpp        160 verificaciones, con hardware simulado
│   └── correr_pruebas.ps1    compila y corre todo
└── referencia/
    └── esclavo_usart1.ino    código anterior PROBADO en el entrenador; origen del LCD y del teclado
```

**Orden de autoridad** si dos documentos se contradicen: (1) el código, (2) este manual, (3) `diseño/`. Este manual registra en §7.4 cada punto en que la implementación precisó o completó el diseño.

---

## 3. Hardware y conexiones

### 3.1 Mapa de pines
| Uso | Pin Mega | Puerto/bit | Dirección | Notas |
|---|---|---|---|---|
| LCD D0–D7 | 22–29 | PA0–PA7 | Salida | Bus de 8 bits |
| LCD RS / E | 37 / 36 | PC0 / PC1 | Salida | RW a GND |
| Teclado filas A–D | 49, 48, 47, 46 | PL0–PL3 | Se activan de a una | |
| Teclado columnas 1–4 | 45, 44, 43, 42 | PL4–PL7 | Entrada + pull-up interna | El teclado del entrenador no trae resistencias |
| Moneda $100 | 41 | PG0 | Entrada | Resistencia del entrenador |
| Moneda $200 | 40 | PG1 | Entrada | Resistencia del entrenador |
| Moneda $500 | 39 | PG2 | Entrada | Resistencia del entrenador |
| LED/relé agua | 35 | PC2 | Salida | |
| LED/relé gaseosa | 34 | PC3 | Salida | |
| RC522 SDA (SS) | 53 | PB0 | Salida | SPI por hardware |
| RC522 SCK / MOSI / MISO | 52 / 51 / 50 | PB1 / PB2 / PB3 | — | Fijos del SPI; los maneja la librería |
| RC522 RST | 5 | PE3 | Salida | |
| RC522 IRQ | — | — | — | Sin conectar |
| RC522 3.3V / GND | 3.3V / GND | — | — | **NUNCA a 5V** |

⚠️ **PORTC es compartido** (LCD en PC0–PC1, LEDs en PC2–PC3): todo el código lo escribe con `|=` y `&= ~`. Escribir `PORTC = valor` rompería el LCD o los LEDs.

### 3.2 El RC522 y los niveles de voltaje
El chip MFRC522 trabaja a 3,3 V y su máximo absoluto en un pin de entrada es V_DD + 0,5 = 3,8 V (datasheet NXP rev. 3.4, Tabla 150, p. 71). Un 1 del Mega son 5 V, así que las señales Mega → RC522 necesitan divisor. **El módulo que usamos ya trae los divisores en la placa** (verificado por la profesora el 20-nov): las señales se conectan directo. La alimentación sigue siendo del pin **3.3V**. Detalle completo y cálculo de un divisor en `diseño/04`.

### 3.3 Antes de energizar
1. Revisar cable por cable contra la tabla.
2. RC522 al **3.3V** del Mega (revisar dos veces).
3. GND del Mega, del entrenador y del RC522 comunes. El 5V de fuentes distintas **nunca** se une.
4. Encender primero el entrenador y después el Mega (si no, el LCD no recibe la inicialización: RESET del Mega).

---

## 4. Compilar, cargar, simular y probar

### 4.1 Requisitos (versiones probadas)
arduino-cli 1.6.0 (o Arduino IDE 2) · core `arduino:avr` 1.8.8 · librería `MFRC522` 1.4.12 (GithubCommunity) · g++ (WinLibs GCC 16.2) solo para las pruebas de PC.

### 4.2 Compilar y generar el .hex
```
arduino-cli compile --fqbn arduino:avr:mega --warnings all --export-binaries dispensador
```
El .hex queda en `dispensador/build/arduino.avr.mega/`. Para Proteus se usa `dispensador.ino.hex` (no el `with_bootloader`). Desde el IDE: abrir `dispensador/dispensador.ino` → placa *Arduino Mega or Mega 2560* → **Programa → Exportar binario compilado**.

**Restricción del IDE:** todos los `.h`/`.cpp` del sketch van planos en `dispensador/`. El IDE no compila subcarpetas.

### 4.3 Modos de prueba de hardware
Un solo sketch; la constante `MODO` de `config.h` elige qué se compila:

| MODO | Qué hace | Qué confirma |
|---|---|---|
| 0 `MODO_NORMAL` | El dispensador completo | Todo |
| 1 `MODO_PRUEBA_LCD` | Texto en las dos líneas y un contador por segundo | Cableado y temporización del LCD |
| 2 `MODO_PRUEBA_TECLADO` | Muestra la última tecla | Filas/columnas del teclado |
| 3 `MODO_PRUEBA_MONEDAS` | Línea 1: nivel crudo de PG0–PG2 · línea 2: última moneda y total | **Nivel activo de los switches (D15)** y antirrebote |
| 4 `MODO_PRUEBA_RELES` | Alterna los dos LEDs cada segundo | LEDs |
| 5 `MODO_PRUEBA_RFID` | Tipo y UID de la tarjeta + saldo del bloque 4. **Solo lectura** | Cableado del RC522 y **tipo de tarjeta (D6)** |

Se cambia la línea `#define MODO` en `config.h` y se recompila, o sin editar:
```
arduino-cli compile --fqbn arduino:avr:mega --export-binaries --build-property "build.extra_flags=-DMODO=3" dispensador
```

### 4.4 Pruebas en PC
```
powershell -ExecutionPolicy Bypass -File pruebas_pc\correr_pruebas.ps1
```
Compila cada prueba con `g++ -std=c++17 -Wall -Wextra` junto con los módulos reales que prueba y la ejecuta. Código de salida: 0 todo pasa · 1 algo falla o no compila · 2 no hay g++.

| Prueba | Módulos reales | Qué cubre |
|---|---|---|
| `prueba_pedido` | pedido | agregar/quitar, máximo 9, nunca negativo, subtotales y total, pago exacto, moneda que se pasa (R7), completar el restante, monedas no válidas, pedido máximo $10.800 |
| `prueba_saldo` | saldo | formato D7 (little-endian), ida y vuelta, saldo $0 ≠ tarjeta en blanco, bloques corruptos (bit cambiado, complemento dañado, escritura cortada, basura en bytes 8–15, todo FF), tope, cobro, recarga |
| `prueba_fsm` | fsm + pedido + saldo, con lcd/teclado/monedas/relés/tarjeta **simulados** | los 13 estados, el texto exacto del LCD, tiempos de D12 y D14, D4, D5, D13, D16 y los tres casos de D8 en pago y en recarga |

**Límite de las pruebas de PC:** en el PC `int` es de 32 bits y en el AVR de 16 bits. Un desborde que solo ocurra en el AVR no se detectaría. Se revisó a mano que ningún cálculo pase de 32 767 (el mayor es `pagado + moneda` ≤ 11 300).

---

## 5. Arquitectura

```
                        dispensador.ino  (setup / loop, selección de MODO)
                                │
                ┌───────────────┴────────────────┐
             fsm.cpp                         pruebas_hw.cpp
   (máquina de estados: NO toca registros,   (modos 1–5)
    NO usa Arduino.h; recibe la hora)
                │
   ┌──────┬─────┼──────┬────────┬──────────┬─────────┐
 lcd   teclado monedas reles  tarjeta    pedido    saldo
 └──── REGISTROS ─────┘   LIBRERÍA     └─ LÓGICA PURA ─┘
                          MFRC522+SPI    (sin hardware)
```

**Reglas de la arquitectura** (`CLAUDE.md` las fija para cualquier cambio futuro):
- Cada registro lo escribe **un solo** módulo. `fsm` no escribe registros.
- `pedido` y `saldo` solo incluyen `<stdint.h>`, `<stdbool.h>` y `config.h`: compilan en el PC.
- `fsm` recibe la hora como parámetro (`fsm_tick(millis())`): no depende de `Arduino.h` y se prueba en el PC con módulos simulados.
- Sin `delay()`. Las únicas esperas activas son las del protocolo del LCD (`_delay_us`/`_delay_ms` dentro de `lcd.cpp`), el `_delay_us(10)` del barrido del teclado y los tiempos internos de la librería MFRC522 (§9.2). La ISR del teclado no espera.
- Máscaras en notación `0b...` (convención de la profesora). Los únicos desplazamientos (`>>`, `<<`) están en `saldo.cpp`, para partir un `uint32_t` en bytes; no son máscaras de registro.
- No se usa `ISR(TIMER0_OVF_vect)`: el núcleo de Arduino la usa para `millis()`. La única ISR propia es `ISR(TIMER2_COMPA_vect)` del teclado (D17).

---

## 6. Módulos: responsabilidad, interfaz y registros

### 6.1 `config.h`
Única fuente de constantes: selector `MODO`, máscaras de pines (`MONEDA_*_MASCARA`, `RELE_*_MASCARA`), pines del RC522 (`TARJETA_PIN_SS` 53, `TARJETA_PIN_RST` 5), productos y precios (`PRECIO_AGUA` 500, `PRECIO_GASEOSA` 700, `MAX_UNIDADES` 9), valores de las monedas, `MONEDA_ACTIVA_EN_ALTO`, tiempos (`ANTIRREBOTE_MONEDA_MS` 30, `ANTIRREBOTE_TECLADO_MS` 20, `TIEMPO_LED_ON_MS` 2000, `TIEMPO_LED_OFF_MS` 500, `TIEMPO_MENSAJE_MS` 2000, `TIEMPO_ALTERNAR_MS` 2000, `TIEMPO_TARJETA_MS` 20000, `TIEMPO_CONSULTA_MS` 3000), `CLAVE_ADMIN` "1234", `RECARGA_MAX_DIGITOS` 6, `SALDO_MAXIMO` 999 900, `BLOQUE_SALDO` 4.

### 6.2 `lcd` — LCD 16x2 (HD44780) por registros
| Función | Qué hace |
|---|---|
| `lcd_iniciar()` | DDRA y PC0–PC1 como salidas; espera 50 ms; instrucciones `0x38` (8 bits, 2 líneas), `0x0C` (display ON, sin cursor), `0x06` (incrementa), `0x01` (borra) |
| `instruc(c)` / `escrib(c)` | Pone `c` en PORTA, RS = 0 (instrucción) o 1 (dato), pulso en E; espera 2 ms / 50 µs |
| `pulsoE()` | E = 1, 1 µs, E = 0: el LCD lee el bus en el flanco de bajada |
| `texto(t)` | Escribe una cadena desde la posición actual |
| `lcd_ir(fila, col)` | Instrucción "Set DDRAM address" `0b1xxxxxxx`: línea 1 desde 0x00, línea 2 desde 0x40 |
| `lcd_dinero(v)` | Escribe `$` y el valor en decimal |
| `lcd_hex(b)` | Escribe un byte en hexadecimal (UID en el MODO 5) |
| `lcd_pantalla(l1, l2)` | Escribe las dos líneas y rellena con espacios hasta la columna 16. **No usa `0x01`** (borrar hace parpadear el LCD) |

Registros: `DDRA = 0b11111111`, `PORTA = dato`, `DDRC |= 0b00000011`, `PORTC |= / &= ~ 0b00000001` (RS), `PORTC |= / &= ~ 0b00000010` (E).
El LCD **no tiene tildes ni ñ**: todos los textos van sin ellas.

### 6.3 `teclado` — 4x4 por registros, leído por interrupción del Timer2 (D17)
**D17 (25-nov, pedido de la profesora): el teclado se atiende por interrupción.** El puerto L no tiene interrupciones por pin (ni INTn ni PCINT: datasheet, Figura 1-1, p. 2), y el cableado no se cambia. Por eso la interrupción es la del **Timer2 en CTC cada 1 ms**, y en cada interrupción se revisa **una fila**.

| Función | Qué hace |
|---|---|
| `teclado_iniciar()` | `DDRL = 0b00000000`, `PORTL = 0b11110000` (pull-ups en las columnas PL4–PL7), `DDRL = 0b00000001` (fila A activa). Timer2: `TCCR2A = 0b00000010` (CTC), `TCCR2B = 0b00000100` (/64), `OCR2A = 249`, `TIFR2 = 0b00000010` (borra OCF2A), `TIMSK2 = 0b00000010` (OCIE2A) |
| `ISR(TIMER2_COMPA_vect)` | Cada 1 ms: lee `PINL & 0b11110000` de la fila que activó la interrupción anterior (ya asentada: no hace falta esperar) y activa la siguiente (`DDRL = fila_activa[fila]`). Cada 4 ms completa una pasada; la tecla se acepta cuando **5 pasadas seguidas** (20 ms) dan lo mismo. Al aceptar una tecla nueva la deja en `tecla_lista` (`volatile`). Corta y sin llamadas |
| `teclado_leer()` | La que usa el programa: devuelve `tecla_lista` y la borra, con las interrupciones apagadas un instante (`SREG` guardado → `cli()` → restaurar `SREG`) para no perder una tecla |
| `teclado_escanear()`, `teclado_nueva()` | De la referencia, probadas en el entrenador; se conservan pero el programa ya no las usa |

**Cálculo del Timer2:** t = N · (OCR2A + 1) / f_clk = 64 · 250 / 16 MHz = **1 ms**. En el Timer2, CS22:20 = `100` es /64 (en el Timer0, `100` sería /256: las tablas son distintas). Pasada completa = 4 filas × 1 ms = 4 ms; antirrebote = 5 pasadas × 4 ms = 20 ms (`ANTIRREBOTE_TECLADO_MS / 4`).

**Por qué el Timer2:** el Timer0 lo usa `millis()` (no se toca `TIMER0_OVF`); el Timer1 y los de 16 bits quedan libres. El núcleo de Arduino configura el Timer2 para PWM en los pines 9/10, que no se usan; `teclado_iniciar()` sobrescribe TCCR2A/B por completo.

**Ventaja frente al sondeo:** el teclado se revisa aunque el programa esté ocupado (por ejemplo, los ~25 ms de `PICC_IsNewCardPresent()` sin tarjeta).

Distribución: `1 2 3 A / 4 5 6 B / 7 8 9 C / * 0 # D`.

### 6.4 `monedas` — tres switches por registros
| Función | Qué hace |
|---|---|
| `monedas_iniciar()` | `DDRG &= ~0b00000111` (PG0–PG2 entradas), `PORTG &= ~0b00000111` (sin pull-up: el entrenador trae resistencias); primera foto |
| `monedas_rearmar()` | **D4**: toma la "foto" del estado actual de los switches; solo cuenta un cambio posterior |
| `monedas_leer()` | Devuelve 100, 200 o 500 cuando un switch pasa de 0 a 1 **y se mantiene 30 ms** (D3); si no, 0. Una moneda por llamada (si dos suben a la vez, la otra sale en la siguiente) |
| `monedas_crudo()` | `PING & 0b00000111` sin invertir (MODO 3) |

Nivel activo: si `MONEDA_ACTIVA_EN_ALTO` es 1, "switch arriba" = 5 V en el pin; si es 0, se invierte con `~valor & 0b00000111`.

### 6.5 `reles` — un LED por producto
`reles_iniciar()`: `DDRC |= 0b00001100`, `PORTC &= ~0b00001100`. `rele_encender(p)` / `rele_apagar(p)`: `PORTC |= / &= ~` `0b00000100` (agua, PC2) o `0b00001000` (gaseosa, PC3).

### 6.6 `tarjeta` — RC522 con la librería MFRC522
| Función | Llamadas a la librería | Qué hace |
|---|---|---|
| `tarjeta_iniciar()` | `SPI.begin()` → `PCD_Init()` | La librería no llama `SPI.begin()`. Llena la clave A con FF×6 |
| `tarjeta_presente()` | `PICC_IsNewCardPresent()` → `PICC_ReadCardSerial()` | REQA + anticolisión y selección; deja UID y SAK en `lector.uid`. No ve tarjetas en HALT |
| `tarjeta_despertar()` | `PICC_WakeupA()` → `PICC_ReadCardSerial()` | WUPA: también despierta tarjetas en HALT. Solo para D8 |
| `tarjeta_leer_saldo(&s)` | `PICC_GetType` → `PCD_Authenticate(KEY_A, 4)` → `MIFARE_Read(4, buf18)` → `saldo_decodificar` | Solo MIFARE 1K (D6). El buffer debe ser de 18 bytes (16 + 2 de CRC) |
| `tarjeta_escribir_saldo(s)` | `PICC_GetType` → `PCD_Authenticate` → `MIFARE_Write(4, bloque, 16)` → `MIFARE_Read` → comparar | D8: escribe, relee y compara los 16 bytes |
| `tarjeta_terminar()` | `PICC_HaltA()` → `PCD_StopCrypto1()` | Duerme la tarjeta y apaga el cifrado. **Se llama al final de toda operación, salga bien o mal**, y por eso no está dentro de leer/escribir: si la lectura durmiera la tarjeta, el pago no podría escribir después |

Resultados (`ResultadoTarjeta`): `OK`, `EN_BLANCO`, `NO_VALIDA`, `NO_SOPORTADA`, `ERROR_AUTENTICACION`, `ERROR_LECTURA`, `ERROR_ESCRITURA`, `ERROR_VERIFICACION`, `SALDO_FUERA_DE_RANGO`.

### 6.7 `pedido` — lógica pura
Estado interno: `cantidad[2]` y `pagado`.
`pedido_reiniciar`, `pedido_agregar(p)` (false si ya hay 9), `pedido_quitar(p)` (false si está en 0), `pedido_cantidad`, `pedido_subtotal`, `pedido_total` (máx. 10 800, `uint16_t`), `pago_reiniciar`, `pago_insertar_moneda(m)` → `PAGO_ACEPTADA` / `PAGO_DEVUELTA` (se pasaba o no es 100/200/500) / `PAGO_COMPLETO`, `pago_pagado`, `pago_falta`.

### 6.8 `saldo` — lógica pura
`saldo_codificar(s, bloque)`, `saldo_decodificar(bloque, &s)` → `BLOQUE_VALIDO` / `BLOQUE_EN_BLANCO` / `BLOQUE_NO_VALIDO`, `saldo_cobrar(s, total, &nuevo)` (false si no alcanza), `saldo_recargar(s, monto, &nuevo)` → `RECARGA_OK` / `RECARGA_MONTO_NO_VALIDO` (0 o no múltiplo de 100) / `RECARGA_SUPERA_TOPE`. La comparación del tope es `monto > tope − saldo`, que no se desborda.

### 6.9 `fsm` — máquina de estados
`fsm_iniciar(ahora)`, `fsm_tick(ahora)`, `fsm_estado()`, `fsm_mostrando_mensaje()`. Organización interna: `entrar(estado)` (acciones al entrar + dibujar), `dibujar()` (pantalla del estado), `tick_xxx()` (lo que se revisa en cada vuelta), `mensaje(...)` (texto temporal y luego otro estado). Ver §7.

---

## 7. Máquina de estados

### 7.1 Teclas
`1` +agua · `2` +gaseosa · `4` −agua · `5` −gaseosa · `#` confirmar · `*` volver/cancelar · `A` monedas · `B` tarjeta · `C` consultar saldo · `D` recarga · `0–9` dígitos.

### 7.2 Reglas transversales
- **No bloqueante:** todos los tiempos se miden como `ahora − inicio ≥ duración` (funciona cuando `millis()` da la vuelta, cada ~49 días).
- **Redibujo:** el LCD solo se reescribe cuando algo cambia.
- **Mensajes temporales:** mientras se muestran, las teclas se descartan y `monedas_leer()` no se llama (una moneda que entra durante el mensaje se cuenta al terminar; no se pierde). Si el mensaje vuelve al mismo estado, solo se repinta: **no se repiten las acciones de entrada** (por ejemplo, no se vuelve a tomar la foto de los switches).
- **Monedas:** solo se leen en PAGO_MONEDAS (D5); la foto se toma al entrar (D4).
- **Tarjeta:** `tarjeta_presente()` solo en PAGO_TARJETA, RECARGA_TARJETA y CONSULTA_SALDO; `tarjeta_despertar()` solo en VERIFICAR_TARJETA; `tarjeta_terminar()` al cerrar cada operación.
- **INICIO siempre** borra el pedido y apaga los LEDs al entrar.

### 7.3 Estados (implementación)
| Estado | Al entrar | LCD | Entradas → siguiente |
|---|---|---|---|
| INICIO | Borra pedido, apaga LEDs | `Dispensador` / `1:Agua 2:Gaseosa` | `1`/`2` → +1 y SELECCION · `C` → CONSULTA_SALDO · `D` → RECARGA_CLAVE |
| SELECCION | — | `Ag:2 Gs:1` / `Total $1700` | `1`/`2` +1 (máx. 9) · `4`/`5` −1 · `#` (total > 0) → CONFIRMAR · `*` → INICIO |
| CONFIRMAR | Pantalla 0 | Alterna cada 2 s: `Ag 2x500=$1000` / `Gs 1x700=$ 700` ↔ `Total $1700` / `#:Si *:Atras` | `#` → METODO_PAGO · `*` → SELECCION · `4`/`5` −1 (si queda vacío → INICIO) |
| METODO_PAGO | — | `A:Monedas` / `B:Tarjeta *:Atr` | `A` → PAGO_MONEDAS · `B` → PAGO_TARJETA · `*` → CONFIRMAR |
| PAGO_MONEDAS | `pago_reiniciar()`, `monedas_rearmar()` | `Falta $1200` / `Pagado $500` | Moneda aceptada → redibuja · se pasa → `Devuelta $500` / `Ingrese $200` (2 s) · completa → DISPENSAR · `*` con dinero → `Devolviendo` / `$300` (2 s) → INICIO · `*` sin dinero → INICIO |
| PAGO_TARJETA | Arranca 20 s | `Acerque tarjeta` / `Total $1700` | Ver §8.2 · `*` o 20 s → METODO_PAGO |
| DISPENSAR | Agua primero (si hay); LED encendido | `Sirviendo agua` o `Sirv. gaseosa` / `Unidad 1 de 2` | Cada unidad: LED 2 s encendido, 0,5 s apagado; al final → GRACIAS. **Las teclas no hacen nada** |
| GRACIAS | — | `Gracias!` / `Disfrute` | 2 s → INICIO |
| RECARGA_CLAVE | 0 dígitos | `Clave admin:` / `****` | Dígitos (máx. 4, el 5.º se ignora) · `#` con 4 dígitos correctos → RECARGA_MONTO, si no → `Clave errada` (2 s) → INICIO · `*` → INICIO |
| RECARGA_MONTO | Monto 0 | `Monto recarga:` / `$5000` | Dígitos (máx. 6; un 0 a la izquierda se ignora) · `*` borra uno (vacío → INICIO) · `#` válido → RECARGA_TARJETA, si no → `Monto no valido` / `Multiplo de 100` (2 s) y conserva lo digitado |
| RECARGA_TARJETA | Arranca 20 s (conserva el monto) | `Acerque tarjeta` / `Recarga $5000` | Ver §8.3 · `*` o 20 s → INICIO |
| CONSULTA_SALDO | Arranca 20 s | `Acerque tarjeta` / `Consulta saldo` | Válida o en blanco → `Saldo $X` (3 s) → INICIO · no válida → `Tarjeta` / `no valida` → INICIO · no soportada → `No soportada` / `Use MIFARE 1K` → INICIO · error de lectura → `Error lectura` / `Intente de nuevo` y sigue esperando · `*` o 20 s → INICIO. **Nunca escribe** |
| VERIFICAR_TARJETA | (datos guardados antes de entrar) | `Acerque de nuevo` / `la misma tarjeta` | Ver §8.4. **Sin tiempo límite** · `*` → `Operacion no` / `confirmada` → INICIO |

### 7.4 Decisiones tomadas al implementar (precisan o completan `diseño/05`)
1. Textos que no caben en 16 columnas: `Sirv. gaseosa`; `Tarjeta` / `no valida`; `Operacion no` / `confirmada`; `Devolviendo` / `$X`.
2. INICIO borra el pedido y apaga los LEDs **siempre** al entrar (si no, después de `Devolviendo` o de una recarga se sumaría sobre un pedido viejo).
3. SELECCION con todo en 0 se queda en SELECCION; solo CONFIRMAR vuelve a INICIO cuando el pedido queda vacío.
4. CONFIRMAR muestra también un producto en 0 (`Gs 0x700=$   0`).
5. `*` en PAGO_MONEDAS sin dinero va directo a INICIO (sin `Devolviendo $0`).
6. Tarjeta en blanco al pagar = saldo $0 → `Saldo insuf.` / `Saldo $0`.
7. Error de lectura o de autenticación: `Error lectura` / `Intente de nuevo` y se sigue esperando (no se escribió nada). No soportada: `No soportada` / `Use MIFARE 1K` y sale como no válida.
8. `Saldo $X` después de pagar o recargar dura 2 s; la consulta, 3 s (D16).
9. Clave: `#` con menos de 4 dígitos = errada; el 5.º dígito se ignora; `*` sale a INICIO (no borra un dígito).
10. Monto: un 0 a la izquierda se ignora; un monto inválido se conserva para corregirlo; sin dígitos se ve `$`.
11. Recarga que supera el tope: **se rechaza** (`Supera el tope` / `Saldo $X` → INICIO); no se recorta.
12. Recarga de tarjeta nueva: un solo mensaje `Tarjeta nueva` / `Saldo $X`. Una tarjeta no válida en recarga va a INICIO: **no se reformatea** (ver §12).
13. VERIFICAR_TARJETA: un saldo válido que no es ni el original ni el nuevo → `Operacion no` / `confirmada` → INICIO; una tarjeta en blanco cuando el original no lo era se trata como no válida; con otra tarjeta apoyada, `Otra tarjeta` se repite cada 2 s.
14. Después de `No se cobro` / `No se recargo` hay que **retirar la tarjeta y acercarla otra vez** (quedó en HALT): el reintento es deliberado; los 20 s vuelven a empezar.
15. En DISPENSAR no se puede cancelar.
16. `fsm_tick` recibe la hora como parámetro (`diseño/06` decía `fsm_tick()`), para poder probar la FSM en el PC.

---

## 8. La tarjeta: formato del saldo y operaciones seguras

### 8.1 Dónde y cómo se guarda (D6, D7)
- Tarjeta **MIFARE Classic 1K**, **bloque 4** (sector 1), autenticado con la **clave A de fábrica** `FF FF FF FF FF FF`. El bloque 0 es de fábrica (UID) y el 3 es el trailer del sector 0 con las claves; el 4 es el primer bloque de datos libre fuera del sector 0.
- Formato de los 16 bytes:

| Bytes | Contenido |
|---|---|
| 0–3 | saldo, `uint32_t` little-endian (ej.: 1700 = `A4 06 00 00`) |
| 4–7 | `~saldo` (complemento a uno), little-endian |
| 8–15 | `00` |

- Interpretación: 16 bytes en 0 → **en blanco** (tarjeta nueva). Saldo y complemento que no coinciden, bytes 8–15 distintos de 0, o saldo > $999.900 → **no válido**. Un saldo $0 válido **no** es "en blanco": su complemento es `FF FF FF FF`.

### 8.2 Pago con tarjeta
```
tarjeta presente → leer saldo
  no soportada / no válida            → mensaje → METODO_PAGO
  error de lectura                    → "Error lectura" y sigue esperando
  saldo < total                       → "Saldo insuf." / "Saldo $X" → METODO_PAGO   (R8: rechazo, no se escribe)
  nuevo = saldo − total → escribir, releer, comparar
     OK                               → "Saldo $nuevo" → DISPENSAR
     falla escritura o verificación   → guardar (UID, original, nuevo, origen) → HALT → VERIFICAR_TARJETA
tarjeta_terminar()
```

### 8.3 Recarga
Igual que el pago, con `nuevo = saldo + monto` (tarjeta en blanco: `nuevo = monto`) y el tope de $999.900. Ante una falla de escritura o verificación también va a VERIFICAR_TARJETA; si la tarjeta estaba en blanco, el "original" es "en blanco".

### 8.4 Verificación tras una escritura dudosa (D8)
**Problema que resuelve:** si la escritura sí quedó en la tarjeta pero la relectura falló (por ejemplo, porque retiraron la tarjeta justo en ese momento), reintentar a ciegas **cobraría dos veces** (o recargaría dos veces).
```
esperar la tarjeta con WUPA (la encuentra aunque esté en HALT); comparar el UID
  otro UID            → "Otra tarjeta" (2 s) y sigue esperando
  saldo == nuevo      → la operación SÍ quedó → pago: DISPENSAR · recarga: INICIO
  saldo == original   → NO quedó → "No se cobro" / "No se recargo" → reintentar en el estado de origen
  válido pero otro    → "Operacion no confirmada" → INICIO
  no válido           → "Tarjeta no valida" → INICIO
  error de lectura    → "Error lectura" / "Acerque de nuevo" y sigue esperando
  *                   → "Operacion no confirmada" → INICIO
```
No tiene tiempo límite: si el cobro ya quedó, volver solo al menú haría perder el dinero al cliente.

---

## 9. Tiempos y recursos

### 9.1 Memoria (MODO 0, compilado con arduino-cli 1.6.0)
Flash **12 942 B de 253 952 (5 %)** · RAM **981 B de 8192 (11 %)** (antes de D17; con D17, en una compilación equivalente con avr-gcc: **+ 1 B de RAM**, ~+120 B de flash — volver a medir con arduino-cli). La mayor parte de la RAM son los textos del LCD: en el AVR los literales de cadena se copian a RAM al arrancar. Si alguna vez faltara RAM, se pueden pasar a flash con `PROGMEM` (hoy no es necesario).

### 9.2 Bloqueos
| Qué | Cuánto | Dónde | Por qué se acepta |
|---|---|---|---|
| `PICC_IsNewCardPresent()` sin tarjeta | ~25 ms por llamada (temporizador del RC522 programado por `PCD_Init`) | Solo en los estados que esperan tarjeta | El teclado no se afecta: lo lee la interrupción del Timer2 (D17); la tecla espera en `tecla_lista` hasta la siguiente vuelta. Las monedas no se leen en esos estados (D5) |
| Intercambio con tarjeta presente | Pocos ms (SPI a 4 MHz); una vez por operación | Lectura/escritura | — |
| Peor caso si el RC522 no contesta | 36 ms (comunicación) / 89 ms (CRC) | Librería | — |
| `PCD_Init()` | ≥ 50 ms | Solo en `setup()` | — |
| Esperas del LCD | 2 ms por instrucción, 50 µs por carácter | `lcd.cpp` | Exigidas por el HD44780 |

Los tiempos de la librería se tomaron de su código fuente (MFRC522 1.4.12); **no se han medido** en la placa.

### 9.3 Tiempos del sistema (todos en `config.h`)
Antirrebote de monedas 30 ms · interrupción del teclado cada 1 ms, antirrebote 20 ms (5 pasadas de 4 ms) · LED 2 s encendido / 0,5 s apagado por unidad · mensajes 2 s · alternancia de CONFIRMAR 2 s · espera de tarjeta 20 s · consulta de saldo 3 s.

---

## 10. Trazabilidad: requisito → decisión → código → prueba

| Requisito | Decisiones | Código | Prueba |
|---|---|---|---|
| R1 Toda la información por LCD | — | `lcd.cpp`, `fsm.cpp: dibujar()` | `prueba_fsm` (texto exacto) · MODO 1 |
| R2 Precios agua $500, gaseosa $700 | — | `config.h` | `prueba_pedido` |
| R3 Varios productos, un pago | D11 | `pedido.cpp` | `prueba_pedido`, `prueba_fsm` |
| R4 Costo por producto y total | — | `pedido_subtotal/total`, CONFIRMAR | `prueba_pedido`, `prueba_fsm` |
| R5 Confirmar; cancelar = −1 unidad | — | `pedido_quitar`, SELECCION, CONFIRMAR | `prueba_pedido`, `prueba_fsm` |
| R6 Monedas por switch, flanco 0→1 | D3, D4, D5, D15 | `monedas.cpp`, PAGO_MONEDAS | `prueba_fsm` (D4, D5) · MODO 3 |
| R7 Moneda que se pasa se devuelve | D13 | `pago_insertar_moneda`, PAGO_MONEDAS | `prueba_pedido`, `prueba_fsm` |
| R8 Tarjeta; saldo en la tarjeta; rechazo | D6, D7, D8, D14 | `tarjeta.cpp`, `saldo.cpp`, PAGO_TARJETA, VERIFICAR_TARJETA | `prueba_saldo`, `prueba_fsm` · MODO 5 |
| R9 Modo recarga | D9, D10 | RECARGA_* , `saldo_recargar` | `prueba_saldo`, `prueba_fsm` |
| R10 Un relé (LED) por producto | D12 | `reles.cpp`, DISPENSAR | `prueba_fsm` · MODO 4 |
| R11 Teclado para elegir/confirmar/cancelar (por interrupción) | D17 | `teclado.cpp` (ISR del Timer2), `fsm.cpp` | `prueba_fsm` · MODO 2 |
| R12 Detalles propios, sustentados | D1–D17 | todo | — |
| (extra) Consulta de saldo | D16 | CONSULTA_SALDO | `prueba_fsm` |

---

## 11. Cómo modificar el sistema (recetas)

Después de **cualquier** cambio: compilar los modos 0 a 5 (§4.2–4.3) y correr las pruebas de PC (§4.4). Si se cambia un comportamiento, actualizar la prueba correspondiente y este manual.

**Cambiar un precio, un tiempo, la clave o el máximo de unidades** → solo `config.h`. Si cambia un precio, revisar en `prueba_pedido.cpp`/`prueba_fsm.cpp` los totales esperados. Si el máximo de unidades pasa de 9, la cantidad deja de caber en una cifra del LCD (pantallas de SELECCION y CONFIRMAR) y el total máximo cambia: verificar que siga cabiendo en `uint16_t` (≤ 65 535) y que `pagado + moneda` no pase de 32 767 (`int` de 16 bits en el AVR).

**Invertir el nivel de los switches** → `MONEDA_ACTIVA_EN_ALTO` en `config.h` (1 = arriba da 5 V; 0 = arriba da 0 V). Confirmarlo con el MODO 3.

**Mover una moneda o un LED a otro pin** → `config.h` (máscara) y el módulo (`monedas.cpp` usa PORTG/DDRG/PING; `reles.cpp` usa PORTC/DDRC). Si cambia de puerto, cambiar también el nombre de los registros. Revisar que el pin no esté usado (§3.1) y, si el puerto es compartido, escribir solo con `|=` y `&= ~`.

**Cambiar el valor de una moneda** → `MONEDA_1/2/3_VALOR` en `config.h`. `pago_insertar_moneda` rechaza cualquier valor que no sea uno de los tres.

**Agregar una moneda** (4.º switch) → `config.h` (máscara y valor), `monedas.cpp` (arreglos `mascara[]` y `valor[]` de 3 a 4, `MONEDAS_MASCARA`), `pago_insertar_moneda` (aceptar el valor nuevo), MODO 3 y pruebas.

**Agregar un producto** (el cambio más grande):
1. `config.h`: `PRODUCTO_X 2`, `PRECIO_X`, máscara del LED nuevo.
2. `pedido.cpp`: `cantidad[2]` → `[3]`, `precio()`, `producto_valido()`, `pedido_reiniciar()`, `pedido_total()`.
3. `reles.cpp`: pin y casos en `rele_encender`/`rele_apagar`.
4. `fsm.cpp`: teclas para +/− (hay que elegir teclas libres: hoy `3` y `6` lo están), pantallas de SELECCION y CONFIRMAR (16 columnas: con tres productos CONFIRMAR necesita otra pantalla en la alternancia) y el orden de DISPENSAR.
5. Revisar el total máximo frente a `uint16_t` e `int` de 16 bits.
6. Pruebas de `pedido` y `fsm`, y este manual.

**Agregar un estado** → valor en el `enum Estado` (`fsm.h`); caso en `dibujar()`; acciones de entrada en `entrar()` si las tiene; función `tick_xxx()`; caso en el `switch` de `fsm_tick()`; transiciones desde los estados que llegan a él; casos en `prueba_fsm.cpp`; fila en §7.3 y en `diseño/05`.

**Cambiar un texto del LCD** → `fsm.cpp` (`dibujar()` o el mensaje correspondiente). Máximo 16 caracteres por línea, sin tildes ni ñ. Actualizar el texto esperado en `prueba_fsm.cpp`.

**Cambiar el bloque o la clave de la tarjeta** → `BLOQUE_SALDO` en `config.h` (nunca un bloque trailer: 3, 7, 11…, ni el 0) y `clave_a` en `tarjeta.cpp`. Las tarjetas con saldo en el bloque anterior dejarán de leerse.

**Errores típicos a evitar:** `PORTC = valor` (rompe LCD o LEDs) · `delay()` en la FSM (congela teclado y monedas) · leer monedas fuera de PAGO_MONEDAS · escribir la tarjeta sin `tarjeta_terminar()` al final · olvidar que `int` es de 16 bits en el AVR · textos de más de 16 caracteres.

---

## 12. Supuestos abiertos, limitaciones y riesgos

### 12.1 Supuestos que se confirman en el hardware
| Supuesto | Cómo se confirma | Si falla |
|---|---|---|
| Nivel activo de los switches (D15) | MODO 3: con los switches abajo, la línea 1 debe mostrar `000` | Cambiar `MONEDA_ACTIVA_EN_ALTO` a 0 |
| La tarjeta es MIFARE Classic 1K (D6) | MODO 5: debe decir `1K` | Conseguir una 1K (las del kit del RC522 suelen serlo) |
| Escribir con la clave A (bits de acceso de fábrica) | Primera recarga en MODO 0 | Si la lectura funciona y la escritura da error, son los bits de acceso del sector 1 |
| El pin 3.3V del Mega alimenta bien el RC522 | Lecturas estables | Si la lectura es intermitente, sospechar la alimentación (la antena puede pedir hasta 100 mA) |
| Tiempos de bloqueo de la librería | Medir con `millis()` en el MODO 5 | — |

### 12.2 Limitaciones conocidas (aceptadas para el alcance académico)
- **Seguridad del saldo:** el saldo está en claro y protegido solo con la clave de fábrica. Cualquiera con un lector y esa clave podría reescribirlo. El complemento detecta errores, no fraude. Una versión real cambiaría las claves del sector y firmaría el saldo.
- **Clave de administrador en el código** (`CLAVE_ADMIN` "1234").
- **Una tarjeta con el bloque dañado no se puede recuperar** desde el dispensador: la recarga la rechaza como no válida en lugar de reformatearla, para no permitir que una tarjeta cualquiera se "formatee" con saldo.
- **El RFID no se puede simular en Proteus** (no hay modelo del RC522, hasta donde se sabe); se prueba en el montaje con el MODO 5.
- **No se puede cancelar durante el dispensado.**

### 12.3 Riesgos del montaje
Alimentar el RC522 con 5 V (lo daña) · `PORTC` mal cableado (LCD y LEDs comparten puerto) · encender el Mega antes que el entrenador (el LCD no inicializa) · GND no común.

---

## 13. Guion de prueba en el entrenador

**Preparación:** MODO 0, todos los switches abajo, una tarjeta MIFARE 1K nueva. Antes, confirmar los MODOS 1 a 5 en orden (§4.3).

| # | Acción | LCD esperado |
|---|---|---|
| 1 | Encender | `Dispensador` / `1:Agua 2:Gaseosa` |
| 2 | Subir y bajar los switches | No pasa nada (D5) |
| 3 | `C` | `Acerque tarjeta` / `Consulta saldo` |
| 4 | Acercar la tarjeta nueva | `Saldo $0` (3 s) → INICIO. Retirarla |
| 5 | `C` y esperar 20 s · luego `C` y `*` | Vuelve a INICIO las dos veces |
| 6 | `D` | `Clave admin:` / (vacío) |
| 7 | `1111` `#` | `****` → `Clave errada` (2 s) → INICIO |
| 8 | `D` `1234` `#` | `Monto recarga:` / `$` |
| 9 | `150` `#` | `Monto no valido` / `Multiplo de 100` → vuelve a `$150` |
| 10 | `*` `*` `*`, luego `5000` | `$15` `$1` `$` → `$5000` |
| 11 | `#` | `Acerque tarjeta` / `Recarga $5000` |
| 12 | Acercar la tarjeta | `Tarjeta nueva` / `Saldo $5000` (2 s) → INICIO. Retirarla |
| 13 | `C` + tarjeta | `Saldo $5000` |
| 14 | `1` `1` `2` | `Ag:2 Gs:1` / `Total $1700` |
| 15 | `4`, luego `1` | `Ag:1 Gs:1` / `Total $1200` → `Ag:2 Gs:1` / `Total $1700` |
| 16 | `#` | Alterna cada 2 s: `Ag 2x500=$1000` / `Gs 1x700=$ 700` ↔ `Total $1700` / `#:Si *:Atras` |
| 17 | `*`, luego `#` | Vuelve a SELECCION y luego a CONFIRMAR |
| 18 | `#` · `*` · `#` | `A:Monedas` / `B:Tarjeta *:Atr` → CONFIRMAR → METODO_PAGO |
| 19 | Subir el switch de $500 **antes** de pulsar `A`, luego `A` | `Falta $1700` / `Pagado $0`: no cuenta (D4). Bajarlo |
| 20 | $500 tres veces (subir y bajar) | `Falta $1200`/`Pagado $500` → `$700`/`$1000` → `$200`/`$1500` |
| 21 | $500 otra vez | `Devuelta $500` / `Ingrese $200` (2 s) → `Falta $200` / `Pagado $1500` |
| 22 | $200 | `Sirviendo agua` / `Unidad 1 de 2`, LED agua 2 s y apagado 0,5 s |
| 23 | Esperar | `Unidad 2 de 2` → `Sirv. gaseosa` / `Unidad 1 de 1` (LED gaseosa) → `Gracias!` / `Disfrute` (2 s) → INICIO |
| 24 | `1` `#` `#` `A`, $200, $100, luego `*` | `Pagado $300` → `Devolviendo` / `$300` (2 s) → INICIO (D13) |
| 25 | `2` nueve veces, `#` `#` `B` | `Ag:0 Gs:9` / `Total $6300` → `Acerque tarjeta` / `Total $6300` |
| 26 | Acercar la tarjeta | `Saldo insuf.` / `Saldo $5000` (2 s) → METODO_PAGO. Retirarla |
| 27 | `*`, `5` cinco veces, `#` | CONFIRMAR con `Gs 4x700=$2800` → METODO_PAGO |
| 28 | `B` y esperar 20 s · luego `B` y `*` | Vuelve a METODO_PAGO las dos veces (D14) |
| 29 | `B` + tarjeta | `Saldo $2200` (2 s) → 4 gaseosas → GRACIAS → INICIO |
| 30 | `C` + tarjeta | `Saldo $2200`: se cobró una sola vez |
| 31 | `D` `1234` `#` `999900` `#` + tarjeta | `Supera el tope` / `Saldo $2200` → INICIO |
| 32 | `1` `#` `#` `B` y pasar la tarjeta muy rápido; repetir hasta ver `Acerque de nuevo` / `la misma tarjeta` | Puede salir bien, `Error lectura`, o entrar a VERIFICAR_TARJETA |
| 33 | En VERIFICAR_TARJETA: otra tarjeta (si hay) | `Otra tarjeta` (2 s) y sigue esperando |
| 34 | La misma tarjeta | `Saldo $X` → DISPENSAR (quedó), o `No se cobro` → `Acerque tarjeta` (retirarla y acercarla otra vez) |
| 35 | Repetir 32 y pulsar `*` | `Operacion no` / `confirmada` → INICIO. Luego `C` para comprobar que el cobro, si hubo, fue uno solo |

Los pasos 32–35 dependen de la mano y pueden no salir; los tres casos de VERIFICAR_TARJETA están cubiertos en `prueba_fsm`.

---

## 14. Historial y convenciones

### 14.1 Fases
| Commit | Fase |
|---|---|
| `f9f6965` | Diseño, README, .gitignore |
| `3e241bf` | Fase 1: periféricos por registros y modos de prueba |
| `e0ead90` | CLAUDE.md, código de referencia, nombres reales en diseño/06 |
| `c7488df` | Fase 2: lógica de pedido y saldo con pruebas en PC |
| `3e4b503` | Diseño: D16 consulta de saldo |
| `9bce885` | Fase 3: tarjeta RFID y modo de prueba |
| `cfc507a` | Diseño: D8 verificación tras escritura dudosa |
| `dbe2781` | Fase 4: máquina de estados completa |
| (pendiente) | D17: teclado por interrupción del Timer2 (pedido de la profesora, 25-nov) |

### 14.2 Convenciones
Español en código, comentarios y commits · textos del LCD sin tildes ni ñ · máscaras `0b...` · cada escritura de registro comentada (registro, bits, para qué) · un commit por cambio coherente.

### 14.3 Glosario
**FSM:** máquina de estados finitos · **Flanco 0→1:** cambio de nivel bajo a alto · **Antirrebote:** ignorar las oscilaciones mecánicas de un contacto durante unos ms · **DDRAM:** memoria de caracteres del LCD · **PICC:** la tarjeta · **PCD:** el lector (RC522) · **UID:** identificador único de la tarjeta · **SAK:** byte con el que la tarjeta indica su tipo · **REQA / WUPA:** comandos para detectar tarjetas (WUPA también despierta las que están en HALT) · **HALT:** estado en que la tarjeta deja de responder a REQA · **Little-endian:** el byte menos significativo primero.
