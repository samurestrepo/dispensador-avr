# 02 — Lista de materiales

Actualizado 20-nov: la profe revisó el módulo RC522 y **ya trae los divisores de nivel** → no hacen falta protoboard ni resistencias.

| Material | Cant. | Estado | Para qué |
|---|---|---|---|
| Entrenador (Mega 2560, LCD 16x2, teclado 4x4, switches, LEDs) | 1 | ✅ Tenemos | Base |
| Módulo RFID-RC522 (con divisores incluidos) + tarjeta/llavero | 1 | ✅ Tenemos | Pago y recarga |
| Cables macho-macho | ~15 | ✅ Tenemos | Conexiones del entrenador |
| **Cables macho-hembra** | 7 | ⚠️ Verificar | Del Mega (hembra) a los pines del RC522 (macho). Si no hay, se puede usar la protoboard como puente con macho-macho |
| 3.er switch | 0–1 | ⚠️ Verificar | Si el entrenador no tiene 3 switches libres: 1 interruptor + 1 resistencia de 10 kΩ |

Los switches del entrenador ya traen resistencias. Lo único del entrenador sin resistencias es el teclado 4x4 (se usan las pull-ups internas en las columnas).
