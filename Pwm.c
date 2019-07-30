/* Implementación del modulo de Pwm. */

#include "Pwm.h"

#define PERIODO		32
#define PERIODO_M		16

#define PUERTO			PORTB
#define TIMER_STAT	TOF
#define TIMER_TC		0x01

/* Configura y da condicion inicial a la estructura de control del Pwm */
void Pwm_Defina_control (Pwm_Control *pcp)
	{
	Pwm *pp;
	char i;
	char m_bit;
	
	for (pp = pcp->t_pwm, i = PWM_NUM_PWMS, m_bit = 0x01, pcp->m_bits = 0;
		  i;
		  ++pp, --i, m_bit <<= 1)
		{
		pp->a_pulso = PERIODO_M;
		pp->c_pulso = 0;
		pcp->m_bits |= m_bit;
		};
	PUERTO &= ~(pcp->m_bits);
	pcp->c_periodo = PERIODO;
	
	/* Programacion del timer */
	
	};

/* Rutina de proceso (para llamar desde la interrupción) */
void Pwm_Procese (Pwm_Control *pcp)
	{
	Pwm *pp;
	char i;
	char m_bit;
	
	/* Atiende al timer */
	TIMER_STAT &= ~TIMER_TC;
	
	--(pcp->c_periodo);
	if ( !(pcp->c_periodo) )
		{
		PUERTO |= pcp->m_bits;
		
		for (pp = pcp->t_pwm, i = PWM_NUM_PWMS;
			  i;
			  ++pp, --i)
			pp->c_pulso = pp->a_pulso;
		pcp->c_periodo = PERIODO;
		return;
		};
		
	for (pp = pcp->t_pwm, i = PWM_NUM_PWMS, m_bit = 0x01;
		  i;
		  ++pp, --i, m_bit <<= 1)
		if (pp->c_pulso)
			{
			--(pp->c_pulso);
			if ( !(pp->c_pulso) )
				PUERTO &= ~m_bit;
			};
	};

/* Rutina para definir el valor de ancho de pulso */
void Pwm_Ancho_pulso (Pwm_Control *pcp, 
							 char num, 
							 char pulso)
	{
	pcp->t_pwm[num].a_pulso = pulso;
	};

#endif