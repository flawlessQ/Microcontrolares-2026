/*
  TIMERS.h

  Inicializaci�n de los timers del ATmega328P:
  - TIMER0: CTC a 1ms, con interrupciones. Genera la bandera GPIOR00 del sistema.
  - TIMER1: CTC a 1ms, con interrupciones. Usado para el PWM de los tres servos.
  - TIMER2: CTC a 1us, sin interrupciones. Polling sobre TIFR2/OCF2A.
*/

#ifndef TIMERS_H_
#define TIMERS_H_

// __________________________________________________________________________________________________________________
//|                                                                                                                  |
//|													INCLUDES			                                             |
//|__________________________________________________________________________________________________________________|

#include <avr/io.h>	// Esta librer�a incluye acceso a los registros del microcontrolador.

// __________________________________________________________________________________________________________________
//|                                                                                                                  |
//|											FUNCIONES PROTOTIPO			                                             |
//|__________________________________________________________________________________________________________________|

void ini_TIMER0();	// Inicializa el Timer0 en modo CTC a 1ms con interrupciones.
					// Genera la bandera GPIOR00 que se usa como tick principal del sistema.
					// Debe llamarse antes de iniciar el bucle principal.

void ini_TIMER1();	// Inicializa el Timer1 en modo CTC a 1ms con interrupciones.
					// Genera la interrupci�n TIMER1_COMPA_vect usada por el driver de servos.
					// Debe llamarse antes de iniciar el bucle principal.

void ini_TIMER2();	// Inicializa el Timer2 en modo CTC a 1us sin interrupciones.
					// El evento de 1us se detecta por polling sobre el flag TIFR2/OCF2A.
					// Debe llamarse antes de iniciar el bucle principal.

#endif /* TIMERS_H_ */