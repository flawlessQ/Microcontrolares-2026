/*
 * SERVO.c
 *
 * Created: 23/5/2026 17:23:56
 *  Author: juani
 */ 
#include "SERVO.h"
#include <avr/interrupt.h>

static _sServoConfig config [3];
static _eServoPos pos [3];
static uint8_t contPick, ancho;

ISR(TIMER1_COMPA_vect){
	contPick++;

	if(contPick == 21){
		contPick = 1;
	}
	
	for(uint8_t i = 0; i < 3; i++){

		if(pos[i] == SERVO_HOME){
			ancho = config[i].home_ms;
		}else{
			ancho = config[i].push_ms;
		}

		if(i == 0){
			if(contPick <= ancho) PORTD |=  (1 << PIND7);
			else                  PORTD &= ~(1 << PIND7);
		}else if(i == 1){
			if(contPick <= ancho) PORTB |=  (1 << PINB3);
			else                  PORTB &= ~(1 << PINB3);
		}else{
			if(contPick <= ancho) PORTB |=  (1 << PINB4);
			else                  PORTB &= ~(1 << PINB4);
		}
	}
	
	
}

void SERVO_Init(){
	
	DDRD |= (1 << PD7);
	DDRB |= (1 << PB3) | (1 << PB4);
	
	contPick = 1;
	
	for(uint8_t i = 0; i < 3; i++){
		config[i].home_ms = 1;
		config[i].push_ms = 2;
		
		pos[i] = SERVO_HOME;
	}
}

void SERVO_Set(_eServoID servo, _eServoPos posicion){
	pos[servo] = posicion;
}

void SERVO_SetConfig(_eServoID servo, uint8_t home_ms, uint8_t push_ms){
	config[servo].home_ms = home_ms;
	config[servo].push_ms = push_ms; 
}