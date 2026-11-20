# Diseño — Proyecto final: dispensador de bebidas
Versión 1 · 20-nov-2026 · Base para el prompt de Claude Code.

Fuentes: `Proyecto 2026_II.docx.md`, respuestas de la profe (`claude/preguntas_proyecto_final.md`), datasheet MFRC522 (NXP rev. 3.4).

**Convención:** **[SUP]** = supuesto o decisión nuestra; hay que poder sustentarla.

| Archivo | Contenido |
|---|---|
| `01_requisitos.md` | Requisitos del enunciado y respuestas de la profe |
| `02_materiales.md` | Lista de materiales: qué hay y qué falta |
| `03_decisiones.md` | Decisiones de diseño y su justificación |
| `04_pines_y_montaje.md` | Mapa de pines, divisores del RC522, verificación antes de energizar |
| `05_teclado_y_estados.md` | Teclas y máquina de estados (pantallas, transiciones, reglas de pago) |
| `06_arquitectura_y_pruebas.md` | Módulos del código, reglas de programación, plan de pruebas |

**Línea de base:** todo lo simple va **por registros** (LCD, teclado, monedas, LEDs), porque sirve de estudio para el parcial 2. Solo el RFID va con librería (`MFRC522.h` + `SPI.h`).
