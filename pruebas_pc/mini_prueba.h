// =====================================================================
// mini_prueba.h — Mini framework de pruebas propio (sin librerías externas)
//
// VERIFICAR(condicion): si la condición es falsa imprime archivo, línea y
// el texto de la condición; en todo caso la cuenta como pasada o fallida.
// resumen(nombre): imprime "N pasaron, M fallaron" y devuelve el código de
// salida del programa (0 = todo bien, 1 = algo falló).
// Cada archivo de prueba es un programa aparte, así que los contadores
// "static" de este archivo no se mezclan entre pruebas. Las funciones son
// "static inline" para que g++ no avise si una prueba no usa alguna.
// =====================================================================
#ifndef MINI_PRUEBA_H
#define MINI_PRUEBA_H

#include <stdio.h>

static int pruebas_pasaron = 0;
static int pruebas_fallaron = 0;

// El "do { ... } while (0)" hace que la macro se comporte como una sola
// instrucción (por ejemplo, dentro de un if sin llaves).
// "#condicion" convierte el código de la condición en texto para imprimirlo.
#define VERIFICAR(condicion)                                              \
  do {                                                                    \
    if (condicion) {                                                      \
      pruebas_pasaron++;                                                  \
    } else {                                                              \
      pruebas_fallaron++;                                                 \
      printf("  FALLO %s:%d: %s\n", __FILE__, __LINE__, #condicion);      \
    }                                                                     \
  } while (0)

// Título de un grupo de verificaciones (solo para leer la salida).
static inline void caso(const char *nombre) {
  printf("- %s\n", nombre);
}

static inline int resumen(const char *nombre) {
  printf("%s: %d pasaron, %d fallaron\n", nombre, pruebas_pasaron, pruebas_fallaron);
  if (pruebas_fallaron > 0) {
    return 1;
  }
  return 0;
}

#endif
