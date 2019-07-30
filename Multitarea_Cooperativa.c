/*	Las rutinas de interrupción deben incrementar ++nivel_so al iniciar y decremen-
	tarlo al terminar.
*/

/* ========= Constantes =========== */
#define TASK_STATUS_WAIT		0
#define TASK_STATUS_READY		1
#define TASK_STATUS_RUN		   2


/* ========= Tipos =========== */
typedef struct Tcb Tcb;
Struct Tcb
   {
   void *sp;
   char estado;
   unsigned char eventos,
                 m_eventos;
   unsigned char prioridad;
   };


/* ========= Variables =========== */
Tcb *tcb_actual;
void *sp_so;
unsigned char nivel_so;


/* ========= Nucleo =========== */
/*	Si GUARDA_CONTEXTO_SO está definido se compila código para guardar el contexto
	del sistema operativo; si no está definido el código que se compila solo asigna el valor 
	inicial del stack pointer y salta al punto de entrada del sistema operativo. */
void Ejecute_so (void)
   {
   static void *sp;

   // Guarda el contexto de la tarea
   // sp = SP;
   tcb_actual->sp = sp;

   ++nivel_so;
   sp = sp_so;
   // SP = sp
#	if defined(GUARDA_CONTEXTO_SO)
   // Restaura el contexto del SO
#		else
	goto SO:
#		endif	
   }


void Dispatcher (Tcb *tcb)
   {
   static void *sp;

   tcb->estado = TASK_STATUS_RUN;

#	if defined(GUARDA_CONTEXTO_SO)
   // Guarda contexto del SO
   // sp = SP;
   sp_so = sp;
#    endif
	
   --nivel_so;
   tcb_actual = tcb;
   sp = tcb_actual->sp;
   // SP = sp;
   // Restaura contexto de la tarea
   }


/* ========= Servicios =========== */
unsigned char Espere_evento (unsigned char m_eventos)
   {
   unsigned char eventos;

   tcb_actual->m_eventos = m_eventos;
	Di();
   if (tcb_actual->eventos & tcb_actual->m_eventos)
      tcb_actual->estado = TASK_STATUS_READY;
      else
      tcb_actual->estado = TASK_STATUS_WAIT;
	Ei();
   
   Ejecute_so();

   eventos = tcb_actual->eventos & tcb_actual->m_eventos;
	Di();
   tcb_actual->eventos &= ~eventos;
	Ei();
   
   return eventos;
   }

void Genere_evento (Tcb *tcb,
                    unsigned char eventos)
   {
	Di();
   tcb->eventos |= eventos;
	Ei();
   if ( (tcb->estado == TASK_STATUS_WAIT) && 
        (tcb->eventos & tcb->m_eventos) )
      tcb->estado = TASK_STATUS_READY;

   if (!nivel_so)
      {
      tcb_actual->estado = TASK_STATUS_READY;
      Ejecute_so();
      };
   }


/* ======================================================================== */
/* Ejemplo de uso - Rutina de presentacion de las practicas (sin titilación)*/
unsigned char buffer_pres[128];

Tcb 	tcb_xm,
		tcb_pres;

#define XM_EV_RX					0x80
#define XM_EV_TX					0x40
#define XM_EV_TIMEOUT			0x20
#define XM_EV_BUFFER_LIBRE		0x10

#define PRES_EV_BUFF_LISTO		0x80
#define PRES_EV_PERIODO			0x40
void Presentacion (void *p)
	{
	unsigned char i;
	unsigned char *bp;
	
	while(SI)
		{
		Espere_evento(PRES_EV_BUFF_LISTO);
		for (bp = buffer_pres, i=128;
			  i;
			  ++bp, --i)
			{
			Espere_evento(PRES_EV_PERIODO);
			Codifique(*bp);
			};
		Genere_evento(&tcb_xm, XM_EV_BUFFER_LIBRE);
		};
	}
