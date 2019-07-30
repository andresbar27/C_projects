/* ============= Modulo de colas ================ */

#if !defined(COLA_H)

#define COLA_H

#include <stddef.h>


/* ------------------- Tipos de datos -------------------- */
// Numero de datos
typedef unsigned char Cola_Num;

/* Estructura de cola */
typedef struct Cola Cola;
struct Cola
   {
   volatile char *bp;	// Apuntador al buffer de datos
   Cola_Num max,	// Longitud de la cola
            num,	// Numero de bytes en la cola
            in,	// Indice de entrada
            out;	// Indice de salida
   };


/* ----------------------- Rutinas ------------------------ */
void Cola_Defina (volatile Cola *cp, volatile char *bp, Cola_Num max);

char Cola_Agregue (volatile Cola *cp, char dato);
char Cola_Retire (volatile Cola *cp, char *dp);
Cola_Num Cola_Num_datos (volatile const Cola *cp);
#define Cola_Disponible(COLA) \
        ((COLA)->max - Cola_Num_datos((COLA)))
#define Cola_Hay_espacio(COLA) \
        Cola_Disponible((COLA))
#define Cola_Llena(COLA) \
        (Cola_Num_datos((COLA)) >= (COLA)->max)

#  endif