# 03 — Decisiones de diseño

| # | Decisión | Por qué (para la sustentación) |
|---|---|---|
| D1 | LCD, teclado, monedas y LEDs **por registros**; RFID con `MFRC522.h` + `SPI.h` | Los periféricos simples se manejan por registros: es lo del curso y sirve de estudio para el parcial 2 (C a papel). El protocolo interno del RC522 tiene decenas de registros propios y no es tema del curso; la profe permitió librerías. |
| D2 | **Máquina de estados no bloqueante** (sin `delay()`); los tiempos se miden con `millis()` | El teclado y los switches se leen siempre; ninguna espera congela el sistema. |
| D3 | Moneda = **flanco 0→1** con antirrebote de 30 ms | Un switch se queda en su posición: si se contara por nivel, un switch encendido metería monedas sin parar. |
| D4 | Al **entrar** a pago con monedas se toma una "foto" de los switches; solo cuenta un cambio posterior | Si un switch ya estaba arriba, no debe contar como moneda. |
| D5 | Los cambios de los switches fuera del estado de pago **se ignoran** | Una moneda solo tiene sentido cuando hay algo que pagar. |
| D6 | Saldo en el **bloque 4** (sector 1) de una tarjeta MIFARE Classic 1K, con la clave A de fábrica (FF FF FF FF FF FF) **[SUP: tipo de tarjeta]** | El bloque 0 es de fábrica (UID) y el 3 es el trailer con las claves; el 4 es el primer bloque de datos libre fuera del sector 0. |
| D7 | Formato del bloque: bytes 0–3 = saldo (`uint32_t`); bytes 4–7 = su complemento (~saldo); el resto en 0 | Si los dos no coinciden, la tarjeta está en blanco o dañada → "tarjeta no válida". Detecta una escritura cortada. |
| D8 | Al pagar (o recargar) se **escribe, se relee y se verifica** el saldo antes de dispensar. **Si la escritura o la verificación fallan**, NO se reintenta a ciegas: se pasa a VERIFICAR_TARJETA, se pide acercar **la misma tarjeta** (mismo UID) y se lee el saldo: si es el **nuevo**, la operación sí quedó (se dispensa / se confirma la recarga); si es el **original**, no quedó (se puede reintentar sin riesgo); si no es válido, error. Ese estado no tiene tiempo límite: solo sale con la tarjeta o con `*` (`Op. no confirmada`) | Si se retira la tarjeta a mitad de la escritura, no se entrega producto sin cobrar. Y si la escritura sí quedó pero la relectura falló, un reintento a ciegas **cobraría dos veces** (o recargaría dos veces). Sin tiempo límite porque, si el cobro ya quedó, volver al menú haría perder el dinero al cliente. |
| D9 | Modo recarga: tecla **D** + **clave de administrador** de 4 dígitos (`1234`, constante) | En una máquina real el cliente no puede regalarse saldo. |
| D10 | Recarga: máximo 6 dígitos, **múltiplo de $100**; saldo máximo $999.900 | Todos los precios y monedas son múltiplos de 100; el tope cabe en 6 cifras en el LCD. |
| D11 | Máximo **9 unidades por producto** | Una cifra en el LCD. Total máximo: 9·500 + 9·700 = $10.800 (cabe en `uint16_t`). |
| D12 | Dispensado: por cada unidad, LED encendido **2 s** y apagado 0,5 s; primero el agua y luego la gaseosa | Se ve cada unidad; dos LEDs a la vez no simulan dos salidas separadas. |
| D13 | `*` en pago con monedas = cancelar y **devolver lo ingresado** (se muestra en el LCD) | El cliente nunca pierde dinero. |
| D14 | Tarjeta sin acercar en **20 s** → vuelve al menú anterior | Evita que la máquina quede bloqueada. |
| D15 | Nivel activo de los switches definido con una constante (`MONEDA_ACTIVA_EN_ALTO`) **[SUP]** | No sabemos si el switch del entrenador da 1 o 0 cuando está arriba; se confirma con `prueba_monedas`. |
| D16 | **Consulta de saldo:** tecla **C** desde INICIO → acercar la tarjeta → `Saldo $X` (3 s) → INICIO. **Solo lectura**: nunca escribe la tarjeta | El cliente sabe cuánto tiene antes de pedir; sirve para mostrar que un cobro o una recarga quedaron en la tarjeta. Al no escribir, no hay riesgo para el saldo. |
