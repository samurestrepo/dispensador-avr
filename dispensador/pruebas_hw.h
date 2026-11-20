// =====================================================================
// pruebas_hw.h — Pruebas de hardware, una por modo (se elige con MODO en config.h)
// Cada prueba tiene dos funciones: _iniciar() va en setup() y _ciclo() en loop().
// Estas pruebas no escriben registros: solo usan las funciones de cada módulo.
// =====================================================================
#ifndef PRUEBAS_HW_H
#define PRUEBAS_HW_H

// MODO 1: texto en las dos líneas y un contador de segundos (prueba lcd_ir y lcd_dinero).
void prueba_lcd_iniciar(void);
void prueba_lcd_ciclo(void);

// MODO 2: muestra la última tecla presionada.
void prueba_teclado_iniciar(void);
void prueba_teclado_ciclo(void);

// MODO 3: línea 1 = nivel crudo de PG0, PG1, PG2 · línea 2 = última moneda y total.
void prueba_monedas_iniciar(void);
void prueba_monedas_ciclo(void);

// MODO 4: alterna los dos LEDs cada segundo.
void prueba_reles_iniciar(void);
void prueba_reles_ciclo(void);

// MODO 5: pendiente (fase 3). Por ahora solo un mensaje.
void prueba_rfid_iniciar(void);
void prueba_rfid_ciclo(void);

#endif
