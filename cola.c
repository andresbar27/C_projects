/* ============= Modulo de colas (buffer circular) ================ */

#include "cola.h"
#include "varios.h"


void Cola_Defina (volatile Cola *cp,
                  volatile char *bp,
                  Cola_Num max)
   {
   cp->bp = bp;
   cp->max = max;
   cp->num = cp->in = cp->out = 0;
   }


char Cola_Agregue (volatile Cola *cp,
                   char dato)
   {
   if (cp->num < cp->max)
      {
      cp->bp[cp->in] = dato;
      ++(cp->num);
      ++(cp->in);
      if (cp->in >= cp->max)
         cp->in = 0;
      return SI;
      }
      else
      return NO;
   }


char Cola_Retire (volatile Cola *cp,
                  char *dp)
   {
   if (cp->num)
      {
      *dp = cp->bp[cp->out];
      --(cp->num);
      ++(cp->out);
      if (cp->out >= cp->max)
         cp->out = 0;
      return SI;
      }
      else
      return NO;
   }

Cola_Num Cola_Num_datos (volatile const Cola *cp)
   {
   return cp->num;
   }
