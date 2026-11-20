# 06 — Arquitectura y pruebas

## Módulos (sketch de Arduino IDE con varias pestañas)
```
dispensador/
├── dispensador.ino     setup(): init de todo · loop(): fsm_tick()
├── config.h            pines, precios, tiempos, clave, valores de las monedas, nivel activo
├── lcd.h / lcd.cpp     REGISTROS: lcd_init, lcd_cmd, lcd_dato, lcd_ir(fila,col), lcd_texto, lcd_dinero, lcd_pantalla(l1,l2)
├── teclado.h / .cpp    REGISTROS: teclado_init, teclado_leer() → char o 0 (no bloqueante, una vez por pulsación)
├── monedas.h / .cpp    REGISTROS: monedas_init, monedas_rearmar() (la foto de D4), monedas_leer() → 100/200/500 o 0
├── reles.h / .cpp      REGISTROS: reles_init, rele_encender(prod), rele_apagar(prod)
├── tarjeta.h / .cpp    LIBRERÍA MFRC522: tarjeta_init, tarjeta_presente(), tarjeta_leer_saldo(&s), tarjeta_escribir_saldo(s) → códigos de resultado
├── pedido.h / .cpp     LÓGICA PURA: cantidades, agregar/quitar, total, pago_insertar_moneda()
├── saldo.h / .cpp      LÓGICA PURA: codificar/decodificar el bloque de 16 bytes, validar
└── fsm.h / fsm.cpp     máquina de estados: solo usa las interfaces de los demás módulos
```

## Reglas para el código
- Ningún módulo toca los registros de otro. La FSM no escribe registros.
- `pedido` y `saldo` no dependen del hardware → se prueban en el PC.
- Sin `delay()` fuera de `lcd` (allí solo los µs/ms que exige el HD44780).
- Cada registro que se escribe lleva un comentario con nombre, bit y para qué (sirve para estudiar).
- Máscaras en `0b...` (convención de la profe).
- PORTC compartido: escribir solo con máscaras.

## Pruebas
**En el PC (g++), sin hardware:**
- `pedido`: agregar/quitar, máximo 9, nunca negativo, total; monedas: pago exacto, moneda que se pasa, completar el restante.
- `saldo`: codificar ↔ decodificar, bloque en blanco, bloque corrupto, tope.

**En el entrenador, un sketch por pieza (también es el orden de montaje):**
1. `prueba_lcd` — texto en las dos líneas.
2. `prueba_teclado` — muestra la tecla en el LCD.
3. `prueba_monedas` — muestra 100/200/500 en cada flanco; confirma el nivel activo (D15).
4. `prueba_reles` — alterna los dos LEDs.
5. `prueba_rfid` — muestra el UID y el saldo del bloque 4 (solo lectura); confirma el tipo de tarjeta (D6).
6. Programa completo — recorrer la tabla de estados caso por caso.
