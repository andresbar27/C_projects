/* Control de la tejedora industrial */

#include <Adc.h>
#include <Tiempo.h>
#include "MotorPaso.h"
#include "Pwm.h"

#define TM_PER_ADC			0
#define TM_PER_M_PASO		1

#define TM_MAX_PERIODOS		5
#define TM_MAX_TIMEOUT		4
static Tm_Periodico t_periodico[TM_MAX_PERIODOS];
static Tm_Timeout t_timeout[TM_MAX_TIMEOUT];
Tm_Control c_tiempo;

Adc_Control c_adc;
Mp_Control c_m_paso;
Pwm_Control c_pwm;

char Atienda_timer (char atienda)
   {
	if (TPM1SC_TOF)
		{
		if (atienda)
			TPM1SC_TOF = 0;
		return SI;
		}
		else
		return NO;
	};

interrupt VINT_TIMER_1 void Int_timer_1(void)
	{
	Pwm_Procese(&c_pwm);
	};

interrupt VINT_TIMER_2 void Int_timer_2 (void)
	{
	Tm_Procese_tiempo(&c_tiempo);
	};

void main (void)
	{
	
	/* Condiciones iniciales */
	Tm_Defina_control(&c_tiempo, t_periodico,
                     TM_MAX_PERIODOS, t_timeout,
                     TM_MAX_TIMEOUT, &Atienda_timer);
	Adc_Defina_control(&c_adc, TM_PER_ADC);
   Mp_Defina_control (&c_m_paso, TM_PER_M_PASO);
	Pwm_Defina_control(&c_pwm);
	
	
	while (SI)
		{
		if ( Tm_Hubo_periodico(&c_tiempo, TM_PER_ADC) )
			Adc_Procese(&c_adc);
		if ( !Tm_Hubo_periodico(&c_tiempo, TM_PER_M_PASO) )
			Mp_Procese(&c_m_paso);

		};
	};
	
	
	
	
	