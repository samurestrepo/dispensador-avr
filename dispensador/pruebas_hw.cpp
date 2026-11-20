// =====================================================================
// pruebas_hw.cpp — Pruebas de hardware (orden de montaje: LCD, teclado, monedas, LEDs, RFID)
// Sin delay(): los tiempos se miden con millis(), igual que en el programa final.
// =====================================================================
#include <Arduino.h>        // solo por millis()
#include "config.h"
#include "lcd.h"
#include "teclado.h"
#include "monedas.h"
#include "reles.h"
#include "tarjeta.h"
#include "pruebas_hw.h"

// ======================= MODO 1: LCD =======================
static unsigned long lcd_t_anterior = 0;
static uint32_t lcd_segundos = 0;

void prueba_lcd_iniciar(void) {
  lcd_iniciar();
  lcd_pantalla("Prueba LCD", "0123456789ABCDEF");
  lcd_t_anterior = millis();
}

void prueba_lcd_ciclo(void) {
  if (millis() - lcd_t_anterior >= 1000) {
    lcd_t_anterior = lcd_t_anterior + 1000;
    lcd_segundos++;
    lcd_ir(0, 12);
    lcd_dinero(lcd_segundos % 1000);   // "$0" a "$999": cabe en las columnas 12–15
    texto("   ");                      // borra cifras viejas (el LCD corta en la columna 16)
  }
}

// ======================= MODO 2: teclado =======================
void prueba_teclado_iniciar(void) {
  lcd_iniciar();
  teclado_iniciar();
  lcd_pantalla("Prueba teclado", "Tecla: -");
}

void prueba_teclado_ciclo(void) {
  char tecla = teclado_leer();
  if (tecla != 0) {
    lcd_ir(1, 7);
    escrib(tecla);
  }
}

// ======================= MODO 3: monedas =======================
// Línea 1: "Crudo PG012:" y los tres niveles eléctricos (0 o 1) de PG0, PG1 y PG2.
//   Con los switches abajo debe verse 000 si MONEDA_ACTIVA_EN_ALTO es 1, o 111 si es 0 (D15).
// Línea 2: última moneda contada y total acumulado; debe subir UNA vez por cada 0->1.
static unsigned long monedas_t_anterior = 0;
static uint32_t monedas_total = 0;

void prueba_monedas_iniciar(void) {
  lcd_iniciar();
  monedas_iniciar();
  lcd_pantalla("Crudo PG012:", "Ult $0 T:$0");
  monedas_t_anterior = millis();
}

void prueba_monedas_ciclo(void) {
  // Nivel crudo, cada 100 ms (no hace falta refrescar el LCD en cada vuelta).
  if (millis() - monedas_t_anterior >= 100) {
    monedas_t_anterior = millis();
    const uint8_t mascara_pg[3] = {MONEDA_100_MASCARA, MONEDA_200_MASCARA, MONEDA_500_MASCARA};
    uint8_t crudo = monedas_crudo();
    lcd_ir(0, 12);
    for (uint8_t i = 0; i < 3; i++) {
      if ((crudo & mascara_pg[i]) != 0) {                 // PG0, luego PG1, luego PG2
        escrib('1');
      } else {
        escrib('0');
      }
    }
  }

  uint16_t moneda = monedas_leer();
  if (moneda != 0) {
    monedas_total = monedas_total + moneda;
    lcd_ir(1, 0);
    texto("Ult ");
    lcd_dinero(moneda);
    texto(" T:");
    lcd_dinero(monedas_total);
    texto("  ");
  }
}

// ======================= MODO 4: LEDs / relés =======================
static unsigned long reles_t_anterior = 0;
static uint8_t reles_turno_agua = 1;    // 1 = está encendida el agua; 0 = la gaseosa

void prueba_reles_iniciar(void) {
  lcd_iniciar();
  reles_iniciar();
  rele_encender(PRODUCTO_AGUA);
  lcd_pantalla("Prueba reles", "Agua ON");
  reles_t_anterior = millis();
}

void prueba_reles_ciclo(void) {
  if (millis() - reles_t_anterior >= 1000) {
    reles_t_anterior = millis();
    if (reles_turno_agua == 1) {
      rele_apagar(PRODUCTO_AGUA);
      rele_encender(PRODUCTO_GASEOSA);
      lcd_pantalla("Prueba reles", "Gaseosa ON");
      reles_turno_agua = 0;
    } else {
      rele_apagar(PRODUCTO_GASEOSA);
      rele_encender(PRODUCTO_AGUA);
      lcd_pantalla("Prueba reles", "Agua ON");
      reles_turno_agua = 1;
    }
  }
}

// ======================= MODO 5: RFID (SOLO LECTURA) =======================
// Línea 1: tipo y UID en hexadecimal, p. ej. "1K A1B2C3D4" (D6: debe decir 1K).
//          Un UID de 7 bytes no cabe entero: el LCD muestra hasta la columna 16.
// Línea 2: resultado de leer el bloque de saldo.
// Este modo NUNCA llama a tarjeta_escribir_saldo().
// La misma tarjeta no se vuelve a leer hasta retirarla y acercarla (queda en HALT).
void prueba_rfid_iniciar(void) {
  lcd_iniciar();
  tarjeta_iniciar();
  lcd_pantalla("Prueba RFID", "Acerque tarjeta");
}

void prueba_rfid_ciclo(void) {
  if (!tarjeta_presente()) {
    return;
  }

  // Línea 1: tipo y UID.
  uint8_t uid[10];
  uint8_t tam = tarjeta_uid(uid);
  lcd_pantalla("", "");                 // limpia las dos líneas sin parpadeo
  lcd_ir(0, 0);
  texto(tarjeta_tipo_texto());
  escrib(' ');
  for (uint8_t i = 0; i < tam; i++) {
    lcd_hex(uid[i]);
  }

  // Línea 2: saldo o motivo.
  uint32_t saldo = 0;
  ResultadoTarjeta r = tarjeta_leer_saldo(&saldo);
  lcd_ir(1, 0);
  if (r == TARJETA_OK) {
    texto("Saldo ");
    lcd_dinero(saldo);
  } else if (r == TARJETA_EN_BLANCO) {
    texto("En blanco");
  } else if (r == TARJETA_NO_VALIDA) {
    texto("No valida");
  } else if (r == TARJETA_NO_SOPORTADA) {
    texto("No soportada");
  } else if (r == TARJETA_ERROR_AUTENTICACION) {
    texto("Error clave A");
  } else {
    texto("Error lectura");
  }

  tarjeta_terminar();
}
