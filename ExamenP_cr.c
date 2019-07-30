/* Solucion del examen parcial 2009-I  - Variante utilizando corrutinas 

	Solo se manejan dos contextos, el principal y el del modulo de comunicaciones.

	Tambien utiliza interrupciones, para lo cual se asume que se utiliza #pragma 
	interrupt para indicarle al compilador cual es la rutina correspondiente a un 
	fuente en particular.
	
	Como referencia tambien se asume que la interrupcion de mayor prioridad es la
	del timer, seguida de la de rx y finalmente la de tx. 
*/

#include "Cola.h"
#include "Corrutin.h"
#include "Tiempo.h"
#include "Varios.h"
#include <ctype.h>
#include <stddef.h>
#include <stdio.h>


/* ---------------- Constantes --------------------- */
// Modulo de tiempos
#define N_PER_RELOJ			0
#  define PER_RELOJ				625	// 1 segundo
#define N_PER_CORRIMIENTO	1
#  define PER_CORRIMIENTO		125	// Un caracter por segundo
#define N_PER_MUX				2
#  define PER_MUX					1		/* Timer  a la frecuencia de multiplexacion
												     (25 columnas, 25 veces por segundo) */
#define NUM_PERIODICOS		(PER_MUX + 1)
#define N_TO_TRAMA			0
#  define TO_TRAMA				625	// 1 segundo
#  define TO_TRAMA_RAPIDO		25		// 20 milisegundos (para descarta trama errada)
#define NUM_TIMEOUTS			(N_TO_TRAMA + 1)
#define TIMER_STAT         (*timer_stat_p)
#  define TIMER_TC2           0x04

// Modulo de display
#define D_MAX_COLUMNAS		1000	// Columnas en el buffer de display
#define LONG_TXT_RELOJ		17		// Caracteres usados para mostar fecha y hora

// Modulo de comunicaciones
#define XON               0x11
#define XOFF              0x13
#define CM_T_BUENO		  "TSI"
#define CM_T_MALO			  "TNO"
#define UART_STAT         (*uart_stat_p)
//    Lectura (registro de estado)
#  define UART_RXD           0x01
#  define UART_TXE           0x04
//    Escritura (registro de control)
#  define UART_RXIE          0x40
#  define UART_TXIE          0x80
#define UART_DATO         (*uart_dato_p)

// Corrutinas
#define CR_LONG_STACK_CM	50


/* ---------------- Tipos de datos ----------------- */
// Display
typedef struct D_Control D_Control;
#define D_FLAG_CON_RELOJ	0x80
#define D_NUM_PUERTOS		4
#define D_NUM_COLUMNAS		25
struct D_Control
	{
	unsigned char banderas;
	unsigned char *bp;	// Apuntador al buffer del display
	size_t num_c,			// Numero de columnas en el buffer (5 por caracter)
	       pos;				// 1ra columna a mostrar en el display
	char col_25;				// Columna a activar en cada matriz de 25
	volatile unsigned char *dp_datos[D_NUM_PUERTOS];
	volatile unsigned char *dp_decoder;
	};

// Reloj
typedef struct Reloj Reloj;
struct Reloj
	{
	size_t anio;
	char mes,
		  dia,
		  hora,
		  minuto,
		  segundo;
	};
void Reloj_Dia_juliano(Reloj *rp);

// Comunicacion serial
typedef struct Uart_Control Uart_Control;
#define LONG_COLA_RX			32
#  define P25_COLA_RX			8
#  define P75_COLA_RX			24
#define LONG_COLA_TX			8
struct Uart_Control
	{
	volatile Cola cola_rx,
	              cola_tx;
	char rx_on,
	     tx_on;
	Tm_Num num_to;
	Tm_Contador duracion_to;
	};
typedef struct Cm_Control Cm_Control	;
struct Cm_Control
	{
	volatile Uart_Control *up;
	};

/* ------------------- Variables -------------------- */
volatile Tm_Control t_control;
volatile Tm_Periodico t_periodico[NUM_PERIODICOS];
volatile Tm_Timeout t_timeout[NUM_TIMEOUTS];
unsigned char buffer_1[D_MAX_COLUMNAS];
unsigned char buffer_2[D_MAX_COLUMNAS];
char usa_buffer_1;
D_Control c_display;
Reloj reloj;
volatile Uart_Control u_control;
volatile char buffer_rx[LONG_COLA_RX];
volatile char buffer_tx[LONG_COLA_TX];
Cm_Control cm_control;
Tcb tcb_cm,
    tcb_ppal;
char stack_cm[CR_LONG_STACK_CM];


/* ------------------- E/S ---------------------- */
volatile unsigned char *timer_stat_p;
volatile unsigned char *uart_stat_p;
volatile char *uart_dato_p;


/* --------------- Rutinas de soporte ------------- */
// Codificacion de un caracter en un buffer de display
void Codifique_caracter (unsigned char *bp,
                         size_t pos,
								 char caracter)
	{
	// Esta tabla deberia ir en ROM
	static unsigned char t_caracteres[sizeof(char)][5];
	char i;
	
	for (i = 0; i < 5; ++i)
		bp[pos + i] = t_caracteres[caracter][i];
	}


/* ---------------- Modulo de display ------------- */
// Esta rutina solo debe llamarse cuando la bandera de tiempo lo indique
void Display_procese (D_Control *dcp)
	{
	size_t columna,
	       offset;
	char i;
	
	Tm_Baje_periodico (&t_control, N_PER_MUX);
	
	if (dcp->bp == NULL)
		// No hay datos para mostar
		return;
	
	*(dcp->dp_decoder) = D_NUM_COLUMNAS + 1;	// Apaga todos
	for (i = 0, offset = 0; i < D_NUM_PUERTOS; ++i)
		{
		columna = (dcp->pos + offset) % dcp->num_c; // Columna a mostrar
		*(dcp->dp_datos[i]) = dcp->bp[columna];	// Muestra la columna
		offset += D_NUM_COLUMNAS;	// Siguiente
		};
	*(dcp->dp_decoder) = dcp->col_25;	// Prende las que se muestran
	// Determina la siguiente columna de las matrices a mostrar
	++(dcp->col_25);
	if (dcp->col_25 >= D_NUM_COLUMNAS)
		dcp->col_25 = 0;
	}

	
/* ----------------------- Modulo de corrimiento -------------------- */
// Esta rutina solo debe llamarse cuando la bandera de tiempo lo indique
void Display_desplace (D_Control *dcp)
	{
	Tm_Baje_periodico (&t_control, N_PER_CORRIMIENTO);
	
	if (dcp->bp == NULL)
		// No hay datos para mostar
		return;
	
	++(dcp->pos);
	if (dcp->pos >= dcp->num_c)
		dcp->pos = 0;
	}


/* ---------------------- Modulo de reloj ---------------------------- */
// Rutina que codifica fecha y hora en un buffer de display
void Reloj_codifique (Reloj *rp,
                      unsigned char *bdp,
							 size_t pos)
	{
	char buffer[LONG_TXT_RELOJ];
	char *bp;

	sprintf(buffer, "%4.4d/%2.2d/%2.2d %2.2d:%2.2d ", rp->anio, rp->mes,
	        rp->dia, rp->hora, rp->minuto);
	for (bp = buffer; *bp; ++bp, pos += 5)
		Codifique_caracter(bdp, pos, *bp);
	}

// Esta rutina solo debe llamarse cuando la bandera de tiempo lo indique
void Reloj_procese (Reloj *rp,
                    D_Control *dcp)
	{
	Tm_Baje_periodico (&t_control, N_PER_RELOJ);

	++(rp->segundo);
	if (rp->segundo >= 60)
		{
		rp->segundo = 0;
		++(rp->minuto);
		if (rp->minuto >= 60)
			{
			rp->minuto = 0;
			++(rp->hora);
			if (rp->hora >= 24)
				{
				rp->hora = 0;
				// Actualiza el día
				Reloj_Dia_juliano(rp);				
				};
			if ( (dcp->bp != NULL) && (dcp->banderas & D_FLAG_CON_RELOJ) )
				// Actualiza la hora en el buffer del display
				Reloj_codifique(rp, dcp->bp, 0);
			};
		};
	}

	
/* -------- Modulo de control de flujo en el serial -------- */
char Tx_dato (volatile Uart_Control *ucp,
              char dato)
	{
	char correcto;
	// Copia el valor actual del bit de habilitacion de interrupciones
	unsigned char txie = UART_STAT & UART_TXIE;

	UART_STAT &= ~UART_TXIE;	// Evita interrupciones
	correcto = Cola_Agregue((&(ucp->cola_tx)), dato);
	if (correcto)
		UART_STAT |= UART_TXIE;	// Habilita interrupciones
		else
		// Restaura el estado del bit de habilitacion de interrupciones
		UART_STAT |= txie;

	return correcto;
	}

char Rx_dato (volatile Uart_Control *ucp,
              char *dp)
	{
	char correcto;
	// Copia el valor actual del bit de habilitacion de interrupciones
	unsigned char rxie = UART_STAT & UART_RXIE;

	UART_STAT &= ~UART_RXIE;	// Evita interrupciones
	correcto = Cola_Retire((&(ucp->cola_rx)), dp);
	if (correcto)
		UART_STAT |= UART_RXIE;	// Habilita interrupciones
		else
		// Restaura el estado del bit de habilitacion de interrupciones
		UART_STAT |= rxie;

	return correcto;
	}

void Uart_Control_flujo_rx (volatile Uart_Control *ucp)
	{
	Cola_Num num_datos = Cola_Num_datos(&(ucp->cola_rx));

	if (num_datos <= P25_COLA_RX)
		{
		if ( !(ucp->rx_on) )
			ucp->rx_on = Tx_dato(ucp, XON);
		}
		else
		if (num_datos >= P75_COLA_RX)
			if (ucp->rx_on)
				ucp->rx_on = !Tx_dato(ucp, XOFF);
	}

void Rx (volatile Uart_Control *ucp)
   {
	char dato;

	if ( !Cola_Disponible(&(ucp->cola_rx)) )
		{ // No hay donde recibir. Se asegura de frenar el flujo, por si acaso
		ucp->rx_on = ucp->rx_on && !Tx_dato(ucp, XOFF);
		// Deshabilita interrupciones de rx de la uart para quitar la condicion de interrupcion 
		UART_STAT &= ~UART_RXIE;
		return;
		};

	dato = UART_DATO;
	switch (dato)
		{
		case XOFF:
			ucp->tx_on = NO;
			break;
		case XON:
			ucp->tx_on = SI;
			break;
		default:
			// Es un dato: se guarda y se reinicia el timeout (si no es 0)
			Tm_Inicie_timeout(&t_control, ucp->num_to, ucp->duracion_to);
			Cola_Agregue(&(ucp->cola_rx), dato);
		};

	Uart_Control_flujo_rx(ucp);
	}

#pragma interrupt UART_RX Int_rx
void Int_rx (void)
   {
	Rx(&u_control);
	}

void Tx (volatile Uart_Control *ucp)
   {
	if ( !(UART_STAT & UART_TXE) )
		return;

	if ( !Cola_Num_datos(&(ucp->cola_tx)) )
		{ // No hay datos para enviar
		// Deshabilita interrupciones de tx de la uart para quitar la condicion de interrupcion
		UART_STAT &= ~UART_TXIE;
		return;
		};

	if (ucp->tx_on)
		{ /* La interrupcion de recepción puede entrar aquí y modificar la cola de tx
		        (por envio de XON o XOFF) , de modo que debe darse manejo de zona critica
		        a la cola. Se copia el estado actual del bit, luego se deshabilitan y al terminar
			 con  la cola se regresa al estado en que estaba; esto se hace asi para no
			 habilitarla si estaba deshabilitada, de modo que no haya una invocación
			 innecesaria de la rutina de interrupcion de rx (en la practica la interrupcion
			 podria entrar despues de la copia y antes de deshabilitarla, pero la interrupcion
			 adicional no generaria inconsistencias, solo una pequeña perdida de tiempo). */
		// Copia el valor actual del bit de habilitacion de interrupciones de rx
		unsigned char rxie = UART_STAT & UART_RXIE;
      char dato;

		UART_STAT &= ~UART_RXIE;	// Evita interrupciones de rx
		Cola_Retire(&(ucp->cola_tx), &dato);
      UART_DATO = dato;
		// Restaura el estado del bit de habilitacion de interrupciones de rx
		UART_STAT |= rxie;
		};
	}

#pragma interrupt UART_TX Int_tx
void Int_tx (void)
   {
	Tx(&u_control);
	}


/* ------------------ Modulo de comunicaciones ----------------------- */
// Rutina que transmite una respuesta
void Cm_Respuesta (Cm_Control *cp,
                   char *respuesta)
	{
	if ( !respuesta )
		// NULL
		return;
		
	// Mientras hayan caracteres por enviar 
	while (*respuesta)
		{
		while ( !Tx_dato(cp->up, *respuesta) )
			// No hay espacio en la cola de transmision
			Cr_Ejecute(&tcb_ppal);
		// Coloco otro caracter en la cola de transmision
		++respuesta;
		};
	}

void Cm_Procese (void *parametro)
	{
	Cm_Control *cp = (Cm_Control *) parametro;
	// Respuesta al paquete
	char *respuesta;
	// Estado a tomar despues de enviar la respuesta
	char *bdp;
	size_t long_bd;
	char con_reloj;
	unsigned char num_t,		// Numero temporal
	              num_d,		// Numero de datos
					  chksum,
					  cont;		// Contador
	char hubo_dato = NO,
	     dato,
		  hay_error,
		  descarte;

	// Condiciones iniciales
	
	
	while (SI)
		{
		descarte = hay_error = NO;
		respuesta = NULL;

		// Espera a que llegue un dato
		while ( !Rx_dato(cp->up, &dato) )
			// Aun no hay dato
			Cr_Ejecute(&tcb_ppal);
		Uart_Control_flujo_rx(cp->up);

		switch (dato)
			{
			case 'T':
				// Es una trama de texto: inicia tiemout
				Tm_Inicie_timeout(&t_control, cp->up->num_to, TO_TRAMA);
				cp->up->duracion_to = TO_TRAMA;
				bdp = usa_buffer_1 ? buffer_2 : buffer_1;
				long_bd = 0;
				descarte = hay_error = NO;
				// Espera el campo de reloj
				while ( !(hubo_dato = Rx_dato(cp->up, &dato)) &&
						  !Tm_Hubo_timeout(&t_control, cp->up->num_to) )
					// Aun no hay dato ni timeout
					Cr_Ejecute(&tcb_ppal);
				if (hubo_dato)
					{ // Se recibio el campo de reloj
					Uart_Control_flujo_rx(cp->up);
					switch (dato)
						{
						case '0':
							con_reloj = NO;
							break;
						case '1':
							// Reserva espacio al inicio del buffer e indica que debe actualizarse
							long_bd += 5 * LONG_TXT_RELOJ;
							con_reloj = SI;
							break;
						default:
							// ERROR: no es '0' o '1'
							descarte = hay_error = SI;
							respuesta = CM_T_MALO;
						}
					if (hay_error)
						// Es todo para este caso
						break;
					chksum = dato;
					}
					else
					{ // Hubo timeout
					hay_error = SI;
					respuesta = CM_T_MALO;
					// Es todo para este caso
					break;
					};

				// Longitud
				num_t = cont = 0; // Longitud en 0 y numero de caracteres leidos en 0
				do
					{
					while ( !(hubo_dato = Rx_dato(cp->up, &dato)) &&
							  !Tm_Hubo_timeout(&t_control, cp->up->num_to) )
						// Aun no hay dato ni timeout
					Cr_Ejecute(&tcb_ppal);
					if (hubo_dato)
						{ // Se recibio un caracter de la longitud
						Uart_Control_flujo_rx(cp->up);
						if ( !isdigit(dato) )
							{ // ERROR: no es un digito decimal
							descarte = hay_error = SI;
							respuesta = CM_T_MALO;
							}
							else
							{
							num_t = 10 * num_t + (dato - '0');
							chksum = dato;
							++cont;
							};
						}
						else
						{ // Hubo tiemout
						hay_error = SI;
						respuesta = CM_T_MALO;
						};
					} while ( !hay_error && (cont < 3) );
				if (hay_error)
					// Es todo para este caso
					break;
				// Ya tiene la longitud
				num_d = num_t;

				// Datos
				cont = 0; // Numero de caracteres leidos en 0
				do
					{
					while ( !(hubo_dato = Rx_dato(cp->up, &dato)) &&
							  !Tm_Hubo_timeout(&t_control, cp->up->num_to) )
						// Aun no hay dato ni timeout
						Cr_Ejecute(&tcb_ppal);
					if (hubo_dato)
						{ // Se recibio un caracter de la longitud
						Uart_Control_flujo_rx(cp->up);
						if ( long_bd < (D_MAX_COLUMNAS - 5) )
							{ // Hay espacio en el buffer de display
							Codifique_caracter(bdp, long_bd, dato);
							long_bd += 5;
							};
							// else 	Si no hay espacio no guarda la información pero acepta el mensaje
						chksum += dato;
						++cont;
						}
						else
						{ // Hubo tiemout
						hay_error = SI;
						respuesta = CM_T_MALO;
						};
					} while ( !hay_error && (cont < num_d) );
				if (hay_error)
					// Es todo para este caso
					break;
				// Ya tiene todos los datos

				// Checksum
				num_t = cont = 0; // Valor y numero de caracteres leidos en 0
				do
					{
					while ( !(hubo_dato = Rx_dato(cp->up, &dato)) &&
							  !Tm_Hubo_timeout(&t_control, cp->up->num_to) )
						// Aun no hay dato ni timeout
						Cr_Ejecute(&tcb_ppal);
					if (hubo_dato)
						{ // Se recibio un caracter de la longitud
						Uart_Control_flujo_rx(cp->up);
						if ( !isdigit(dato) )
							{ // ERROR: no es un digito decimal
							descarte = hay_error = SI;
							respuesta = CM_T_MALO;
							}
							else
							{
							num_t = 10 * num_t + (dato - '0');
							++cont;
							};
						}
						else
						{ // Hubo tiemout
						hay_error = SI;
						respuesta = CM_T_MALO;
						};
					} while ( !hay_error && (cont < 3) );
				if (hay_error)
					// Es todo para este caso
					break;
				// Ya tiene el checksum
				if (num_t == chksum)
					{ // Trama correcta. Cancela el tiemout y envia la respuesta.
					Tm_Inicie_timeout(&t_control, cp->up->num_to, 0);
					cp->up->duracion_to = 0;
					Cm_Respuesta(cp, CM_T_BUENO);
					}
					else
					{ // Trama errada
					hay_error = SI;
					respuesta = CM_T_MALO;
					// Es todo para este caso
					break;
					};

				// Espera para poder cambiar el buffer del display
				while ( !(c_display.pos) )
					// Aun no ha complatado el refresco del display
					Cr_Ejecute(&tcb_ppal);
				c_display.bp = bdp;
				if (con_reloj)
					{ // Guarda la hora al inicio del buffer e indica que debe actualizarse
					Reloj_codifique(&reloj, bdp, 0);
					c_display.banderas |= D_FLAG_CON_RELOJ;
					}
					else
					c_display.banderas &= ~D_FLAG_CON_RELOJ;
				c_display.num_c = long_bd;
				usa_buffer_1 = !usa_buffer_1;
				break;
			// El codigo para los otros tipos de paquete no está implementado aún
			
		//	default:
			};
		if (descarte)
			{ /* Descarta todos los caracteres restantes del paquete (controla con un
			         timeout más pequeño */
			Tm_Inicie_timeout(&t_control, cp->up->num_to, TO_TRAMA_RAPIDO);
			cp->up->duracion_to = TO_TRAMA_RAPIDO;
			do
				{
				while ( !(hubo_dato = Rx_dato(cp->up, &dato)) &&
						  !Tm_Hubo_timeout(&t_control, cp->up->num_to) )
					Cr_Ejecute(&tcb_ppal);
				if (hubo_dato)
					Uart_Control_flujo_rx(cp->up);
				} while (hubo_dato);
			// Termino el paquete errado.
			cp->up->duracion_to = 0;
			};
		if (respuesta)
			// Envia la respuesta programada
			Cm_Respuesta(cp, respuesta);
		}; // Completo la trama (bien o mal) y vuelve a esperar la siguiente
		
	// La corrutina nunca termina: no debe llegar aquí
	}


/* ------------------ Modulo de tiempo --------------- */
#pragma interrupt TIMER Int_timer
void Int_timer (void)
   {
	Tm_Procese_tiempo(&t_control);
	}
	
/* --------------------------------------------------- */
/* Rutina de verificacion y atencion del timer (hardware) */
static char Atender_timer (char atienda)
   {
   if (TIMER_STAT & TIMER_TC2)
      {
      if (atienda)
         TIMER_STAT &= ~TIMER_TC2;
      return SI;
      }
      else
      return NO;
   }

void main (void)
	{

	// Condiciones iniciales
	Tm_Defina_control(&t_control, t_periodico, NUM_PERIODICOS, t_timeout, 
	                  NUM_TIMEOUTS, Atender_timer );
	usa_buffer_1 = NO;
	c_display.banderas = 0;
	c_display.bp = NULL;
	c_display.num_c = 0;
	c_display.pos = 0;
	c_display.col_25 = 0;
	// Aqui se deben inicia las direcciones de los puertos en c_display
	Tm_Inicie_periodico(&t_control, N_PER_MUX, PER_MUX);
	Cola_Defina(&(u_control.cola_rx), buffer_rx, LONG_COLA_RX);
	Cola_Defina(&(u_control.cola_tx), buffer_tx, LONG_COLA_TX);
	u_control.rx_on = SI;
	u_control.tx_on = SI;
	u_control.num_to = N_TO_TRAMA;
	u_control.duracion_to = 0;
	cm_control.up = &u_control;
	Cr_Inicie(&tcb_cm, stack_cm + CR_LONG_STACK_CM - 1, Cm_Procese,
	          (void *) &cm_control);
	cr_tcb_actual = &tcb_ppal;

	// Asignacion de direcciones de los perifericos
	timer_stat_p = (unsigned char *) 0x0082;
	uart_stat_p = (unsigned char *) 0x0100;
	uart_dato_p = (unsigned char *) 0x0101;

	// Programacion de perifericos
	


	
	while (SI)
		{
		if ( Tm_Hubo_periodico(&t_control, N_PER_MUX) )
			Display_procese(&c_display);
		if ( Tm_Hubo_periodico(&t_control, N_PER_CORRIMIENTO) )
			Display_desplace(&c_display);
		if ( Tm_Hubo_periodico(&t_control, N_PER_RELOJ) )
			Reloj_procese(&reloj, &c_display);
		Cr_Ejecute(&tcb_cm);
		/* La siguiente funcion debe ejecutarse si el modulo de comunicaciones no lo hace cuando retira
		    datos de la cola de rx.
		Uart_Control_flujo_rx(&u_control); */
		};
	}