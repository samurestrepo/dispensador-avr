// =====================================================================
// rc522_logica.h — Lógica del emulador del RC522 (solo para la simulación)
// Se compila igual para el ATmega328P del esquema (main.c) y para el PC
// (prueba_pc.cpp), donde se prueba contra la librería MFRC522 real.
// =====================================================================
#ifndef RC522_LOGICA_H
#define RC522_LOGICA_H
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void    emu_iniciar(void);              // llamar una vez al arrancar

// Interfaz SPI (la llama la ISR del SPI y la del pin SS)
void    emu_ss_bajo(void);              // empieza una trama
uint8_t emu_byte(uint8_t recibido);     // devuelve el byte que se carga en SPDR para el siguiente
void    emu_ss_alto(void);              // termina la trama

// Trabajo lento (comandos), fuera de las interrupciones
void    emu_procesar(void);
uint8_t emu_hay_trabajo(void);         // 1 si hay un comando esperando a emu_procesar

// Entradas del esquema (interruptores LOGICSTATE)
void emu_entradas(uint8_t presente, uint8_t tarjeta_b, uint8_t falla, uint8_t falla_guarda);

// Para el registro de eventos y las pruebas
uint8_t  emu_bloque4(uint8_t tarjeta, uint8_t i);
uint8_t  emu_evento(void);              // último evento (0 = ninguno); se borra al leerlo
enum { EV_NADA, EV_LECTURA, EV_ESCRITURA, EV_ESCRITURA_FALLIDA, EV_HALT, EV_AUTH_MAL };

#ifdef __cplusplus
}
#endif
#endif
