#define LONG_STACK_XM         50
#define LONG_STACK_PRES       50

typedef struct Tcb Tcb 
  {
  void *sp;
  };

typedef void Corrutina(void *parametro);

Tcb *tcb_actual;
TCb   tcb_ppal,
      tcb_xm,
      tcb_pres;
char  stack_xm[LONG_STACK_XM], 
      stack_pres[LONG_STACK_PRES];

void Ejecute_cr (Tcb *tcb) 
  {
  static void *sp;
  
  // Guardar contexto (en el stack)
  // sp <- SP (en asembler)
  tcb_actual->sp = sp;
  tcb_actual = tcb;
  sp = tcb_actual->sp;
  // SP <- sp (en asembler)
  // Restaurar contexto (del stack)
  }

/* Segun el procesador */
#define LONG_SR            1
#define INIT_SR            0
#define LONG_RESTO_CTX     5
void Inicie_cr (Tcb *tcb, 
                void *sp, 
                Corrutina *rutina, 
                void *parametro)
  {
	(char*)sp -= (sizeof(void*)-1); //tomando sp en el primero libre
	*((void**)sp) = parametro;
	--((corrutina**)sp);
	*((corrutina**)sp) = rutina;
	(char*)sp -= (sizeof(void*) + LONG_SR);
	*((TIPO_SR**)sp) = INIT_SR;
	(char *)sp -= (LONG_RESTO_CTX +1);
	tcb->sp = sp;
  }

/* Modulo de presentacion */
void Presentacion (void *p)
  {
	unsigned char i; 
	unsigned char *bp;
	
	while(SI)
	  {
		while(!hay_buffer_datos)
			Ejecute_cr(&tcb_ppal);
			
		for(i=128; bp = buffer_pres; i>0; ++bp, i--)
		  {
			while(!Tm_Hubo_periodico(...))
				Ejecute_cr(&tcb_ppal);
			Tm_Baje_periodico( ... );
			Decodifique(*bp);
		  }
		hay_buffer_datos = NO;
	  }
  }

/* Xmodem como deberia ser al utilizar corrutinas */
void Xmodem (void *parametro)
   {
	unsigned char  hay_dato,
                  repeticiones,
                  num_paquete,
                  fin_archivo,
                  aborte;
	
	while(SI)
      { /* recibe archivo */
		repeticiones = 0;
		num_paquete = 1;
		fin_archivo = NO;
		do 
		  { // Recibe paquete
		  aborte = NO; 
         do
            { /* Espera inicio de paquete o EOT */
            Tm_Inicie_timeout( ... );
            Tx_dato (NAK);
            while(!(hay_dato = Rx_dato(&dato)) && !Tm_Hubo_timeout( .. ))
               Ejecute_cr(&tcb_ppal);
            
            if(hay_dato)
              {
               switch(dato)
                 {
                  case SOH:
                     ...
                  case EOT:
                    fin_archivo = SI;
                     ...
                  default
                     hay_dato = NO;
                 }
               }
               else
               { /* Hubo timeout */
               repeticiones++;
               if(repeticiones >= xxx)
                  {
                  aborte = SI;
                  break;
                  }
               }
           } while (!hay_dato && !fin_archivo && !aborte);
         if (aborte)
           /* A inicio de recibir paquete */
            continue;
            
         // Espera numero de paquete
         Tm_Inicie_timeout( ...);
         while(!(hay_dato = Rx_dato(&dato)) && !Tm_Hubo_timeout( .. ))
            Ejecute_cr(&tcb_ppal);
         
         // Etc...
         
         } while (!fin_archivo)
      }
   }

/* ----------------------------------------------------------------------------- */
/* Forma de uso */
void main()
   {
  
  
	Inicie_cr(&tcb_xm, stack_xm + LONG_STACK_XM - 1, Xmodem, NULL);
	Inicie_cr(&tcb_pres, stack_pres + LONG_STACK_PRES - 1, Presentacion, NULL);
	tcb_actual = &tcb_ppal;
	
	
	
	while(SI)
	  {
		Ejecute_cr(&tcb_xm);
		if( ... ){
			Ejecute_cr(&tcb_pres);
		}
		if( ... ){
			Mux();
		}
  }

  
  
  
  
  
  