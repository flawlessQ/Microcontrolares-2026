/*
  SERVO.c

  Driver de software PWM para tres servomotores SG90 sobre ATmega328P.

  Principio de funcionamiento:
  Un contador de 0 a 19 (pwm_tick) avanza 1ms por cada llamada a SERVO_On1ms().
  - Cuando el contador llega a 0, se ponen en alto los tres pines de se�al.
  - Cuando el contador iguala el ancho de pulso configurado (1ms o 2ms),
    se baja el pin correspondiente.
  - Esto genera un per�odo de 20ms (50Hz) con pulsos de 1ms o 2ms por servo.

  SERVO_On1ms() debe llamarse desde ISR(TIMER1_COMPA_vect) exactamente cada 1ms.
*/

// __________________________________________________________________________________________________________________
//|                                                                                                                  |
//|													INCLUDES			                                             |
//|__________________________________________________________________________________________________________________|

#include "SERVO.h"		// Se incluye el archivo de cabecera propio.

// __________________________________________________________________________________________________________________
//|                                                                                                                  |
//|													DEFINICIONES		                                             |
//|__________________________________________________________________________________________________________________|

#define SERVO_PERIOD_MS  20		// Per�odo del ciclo PWM en ms (50Hz).

// __________________________________________________________________________________________________________________
//|                                                                                                                  |
//|												VARIABLES INTERNAS		                                             |
//|__________________________________________________________________________________________________________________|

static uint8_t pwm_tick       = 0;			// Contador de ms dentro del per�odo PWM (rango: 0 a 19).
static uint8_t pulse_width[3] = {1, 1, 1};	// Ancho de pulso en ms para cada servo (HOME=1ms por defecto).

// __________________________________________________________________________________________________________________
//|                                                                                                                  |
//|											C�DIGO DE FUNCIONES			                                             |
//|__________________________________________________________________________________________________________________|

void SERVO_Init(void)
{
// En esta funci�n se inicializan los tres servos en posici�n HOME y se reinicia el contador PWM.

	pulse_width[0] = (uint8_t)SERVO_HOME;		// Servo 1: posici�n HOME (pulso 1ms).
	pulse_width[1] = (uint8_t)SERVO_HOME;		// Servo 2: posici�n HOME (pulso 1ms).
	pulse_width[2] = (uint8_t)SERVO_HOME;		// Servo 3: posici�n HOME (pulso 1ms).
	pwm_tick       = 0;							// Reinicio el contador del ciclo PWM.
}

void SERVO_Set(_eServoID id, _eServoPos pos)
{
// En esta funci�n se establece la posici�n del servo indicado.
// El cambio se aplica en el siguiente ciclo PWM (m�ximo 20ms de demora).

	pulse_width[id] = (uint8_t)pos;				// Guardo el nuevo ancho de pulso del servo indicado.
}

void SERVO_On1ms(void)
{
// En esta funci�n se genera la se�al PWM de los tres servos.
// Debe llamarse desde ISR(TIMER1_COMPA_vect) exactamente cada 1ms.

	pwm_tick++;								// Incremento el contador del ciclo PWM.
	if(pwm_tick >= SERVO_PERIOD_MS)			// Si se complet� un per�odo de 20ms...
		pwm_tick = 0;						// ...reinicio el contador para comenzar el nuevo ciclo.

	// ── Servo 1 — PD7 ───────────────────────────────────────────────────────────────────────

	if(pwm_tick == 0)								// Al inicio del per�odo, levanto la se�al del Servo 1.
		PORTD |=  (1 << PIND7);

	if(pwm_tick == pulse_width[SERVO_ID_1])			// Cuando el contador iguala el ancho de pulso configurado...
		PORTD &= ~(1 << PIND7);						// ...bajo la se�al: fin del pulso del Servo 1.

	// ── Servo 2 — PB3 (D11) ────────────────────────────────────────────────────────────────

	if(pwm_tick == 0)								// Al inicio del per�odo, levanto la se�al del Servo 2.
		PORTB |=  (1 << PINB3);

	if(pwm_tick == pulse_width[SERVO_ID_2])			// Cuando el contador iguala el ancho de pulso configurado...
		PORTB &= ~(1 << PINB3);						// ...bajo la se�al: fin del pulso del Servo 2.

	// ── Servo 3 — PB4 (D12) ────────────────────────────────────────────────────────────────

	if(pwm_tick == 0)								// Al inicio del per�odo, levanto la se�al del Servo 3.
		PORTB |=  (1 << PINB4);

	if(pwm_tick == pulse_width[SERVO_ID_3])			// Cuando el contador iguala el ancho de pulso configurado...
		PORTB &= ~(1 << PINB4);						// ...bajo la se�al: fin del pulso del Servo 3.
}
