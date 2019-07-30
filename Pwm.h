/* Definiciones del modulo de Pwm. */
#if !defined(PWM_H)

#define PWM_H

/* ------------------- Tipos de datos -------------------- */
/* Estructura de control del PWM */
typedef struct Pwm Pwm;
struct Pwm
	{
	char	a_pulso,
			c_pulso;
	};

/* Estructura de control del PWM */
typedef struct Pwm_Control Pwm_Control;
#define PWM_NUM_PWMS		2
struct Pwm_Control
	{
	Pwm t_pwm[PWM_NUM_PWMS];
	char c_periodo;
	unsigned char m_bits;
	};


/* Configura y da condicion inicial a la estructura de control del Pwm */
void Pwm_Defina_control (Pwm_Control *pcp);

/* Rutina de proceso (para llamar desde la interrupción) */
void Pwm_Procese (Pwm_Control *pcp);

/* Rutina para definir el valor de ancho de pulso */
void Pwm_Ancho_pulso (Pwm_Control *pcp, char num, char pulso);

#endif