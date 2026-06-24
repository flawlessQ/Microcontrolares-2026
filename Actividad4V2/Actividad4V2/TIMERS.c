/*
 * TIMERS.c
 *
 * Created: 23/5/2026 17:22:36
 *  Author: juani
 */ 

#include "TIMERS.h"

void ini_TIMER0 (){
	TCNT0 = 0;
	                          
	TCCR0A = 0;
	TCCR0B = 0;
	
	TCCR0A |= (1 << WGM01);
	
	TCCR0B |= (1 << CS01) | (1 << CS00);
	
	OCR0A = 249;
	
	TIMSK0 |= (1 << OCIE0A);
}

void ini_TIMER1 (){
	TCNT1H = 0;
	TCNT1L = 0;
	
	TCCR1A = 0;
	TCCR1B = 0;
	
	TCCR1B = (1 << WGM12);
	
	TCCR1B |= (1 << CS11) | (1 << CS10);
	
	OCR1AH = (uint8_t)(249 >> 8);
	OCR1AL = (uint8_t)(249 & 0xFF);
	
	TIMSK1 |= (1 << OCIE1A);
}

void ini_TIMER2 (){
	TCNT2 = 0;

	TCCR2A = 0;		// Modo Normal: WGM21=0, WGM20=0
	TCCR2B = 0;

	TIFR2 |= (1 << TOV2);		// Limpio bandera overflow pendiente

	TCCR2B |= (1 << CS22);		// Prescaler 64 -> 1 tick = 4us
}