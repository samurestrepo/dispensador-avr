# 04 — Pines y montaje

## Mapa de pines
| Uso | Pin Mega | Puerto | Dirección | Notas |
|---|---|---|---|---|
| LCD D0–D7 | 22–29 | PA0–PA7 | Salida | Igual que en las prácticas |
| LCD RS / EN | 37 / 36 | PC0 / PC1 | Salida | RW a GND |
| Teclado FIL A–D | 49, 48, 47, 46 | PL0–PL3 | Salida | Igual que en las prácticas |
| Teclado COL 1–4 | 45, 44, 43, 42 | PL4–PL7 | Entrada + pull-up interna | El teclado no trae resistencias |
| Moneda $100 | 41 | PG0 | Entrada | Resistencia del entrenador |
| Moneda $200 | 40 | PG1 | Entrada | Resistencia del entrenador |
| Moneda $500 | 39 | PG2 | Entrada | Resistencia del entrenador |
| LED/relé agua | 35 | PC2 | Salida | |
| LED/relé gaseosa | 34 | PC3 | Salida | |
| RC522 SDA (SS) | 53 | PB0 | Salida | Directo (el módulo trae divisor) |
| RC522 SCK | 52 | PB1 | Salida | Directo (el módulo trae divisor) |
| RC522 MOSI | 51 | PB2 | Salida | Directo (el módulo trae divisor) |
| RC522 MISO | 50 | PB3 | Entrada | Directo |
| RC522 RST | 5 | PE3 | Salida | Directo (el módulo trae divisor) |
| RC522 IRQ | — | — | — | Sin conectar |
| RC522 3.3V | 3.3V | — | — | **NUNCA a 5V** |
| RC522 GND | GND | — | — | Común con el entrenador |

**⚠️ PORTC lo comparten el LCD (PC0–PC1) y los LEDs (PC2–PC3).** Todo el código escribe con máscaras (`|=`, `&= ~`); nunca `PORTC = valor`.

## Niveles de voltaje del RC522 (datasheet MFRC522, NXP rev. 3.4)
**Actualizado 20-nov:** la profe revisó nuestro módulo y **ya trae los divisores en la placa**. Las señales se conectan directo; NO se agregan divisores externos. Lo de abajo queda como justificación para la sustentación (por qué hacen falta los divisores, aunque estén en el módulo).

| Dato | Valor | Página |
|---|---|---|
| Alimentación del chip | 2,5–3,6 V (típ. 3,3 V) | Tabla 151, p. 71 |
| Máximo absoluto en un pin de entrada | V_DD + 0,5 = **3,8 V** | Tabla 150, p. 71 |
| V_IH (mínimo para leer un 1) | 0,7·3,3 = **2,31 V** | Tabla 153, p. 72 |
| V_OH (lo que entrega en un 1) | ≥ 3,3 − 0,4 = 2,9 V con 4 mA | Tabla 153, p. 73 |
| Consumo de la antena (TVDD) | 60 mA típ. / 100 mA máx. | Tabla 153, p. 74 |

Un 1 del Mega son 5 V > 3,8 V → las líneas Mega → RC522 (SDA, SCK, MOSI, RST) llevan divisor. MISO (RC522 → Mega) va directo: el Mega lee un 1 desde 0,6·Vcc = 3,0 V *(dato deducido del datasheet del ATmega2560; la copia del proyecto se corta antes de esa tabla)*; poco margen, pero funciona en la práctica.

Alimentación desde el pin 3.3V del Mega: es lo usual y funciona, pero ese pin se especifica en ≈ 50 mA *(dato de la página de Arduino, no está en nuestros documentos)* y la antena puede pedir hasta 100 mA. Si la lectura es intermitente, es la primera sospecha.

## Cómo funciona un divisor (para la sustentación; en nuestro caso ya está en el módulo)
```
Pin Mega ──[R1]──┬── Pin RC522
                 │
               [R2]
                 │
                GND
```
V = 5 · R2 / (R1 + R2). Ejemplo con 1 kΩ y 2,2 kΩ: 5 · 2,2 / 3,2 = **3,44 V** → < 3,8 V (no daña) y > 2,31 V (se lee como 1).

## Verificación antes de energizar el RC522
1. Con todo desenergizado, conectar el RC522: **3.3V del Mega al pin 3.3V del módulo**. Revisar dos veces que no vaya a 5V (los divisores protegen las señales; no asumir que el módulo trae regulador para la alimentación).
2. GND del módulo, del Mega y del entrenador comunes.
3. SDA, SCK, MOSI, MISO, RST directos según la tabla; IRQ libre.
4. Revisar cable por cable contra la tabla antes de energizar (error repetido: cable mal puesto en el LCD, 18-nov).
5. Encender primero el entrenador y luego el Mega.
