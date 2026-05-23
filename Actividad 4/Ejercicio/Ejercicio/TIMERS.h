#ifndef TIMERS_H_
#define TIMERS_H_

#include <avr/io.h>	//Esta librería incluye acceso a los registros del microcontrolador.

void ini_TIMER2 (); // Función de inicialización del Timer2, configurado en modo CTC a 1us sin interrupciones.
					// Debe de llamarse antes de iniciar el bucle principal.
					
void ini_TIMER1 (); // Función de inicialización del Timer1, configurado en modo CTC a 1ms con interrupciones.
					// Debe de llamarse antes de iniciar el bucle principal.
					
void ini_TIMER0 (); // Función de inicialización del Timer0, configurado en modo CTC a 1ms con interrupciones 
					// Debe de llamarse antes de iniciar el bucle principal.
					
#endif