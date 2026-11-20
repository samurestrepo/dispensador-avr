// =====================================================================
// reles.h — Un LED por producto (simula el relé de salida), por registros
// PC2 = agua · PC3 = gaseosa. PORTC es compartido con el LCD (PC0–PC1).
// =====================================================================
#ifndef RELES_H
#define RELES_H

#include <stdint.h>

void reles_iniciar(void);                // PC2–PC3 como salidas, apagadas
void rele_encender(uint8_t producto);    // PRODUCTO_AGUA o PRODUCTO_GASEOSA
void rele_apagar(uint8_t producto);

#endif
