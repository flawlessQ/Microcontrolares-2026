/*
 * CONVEYOR.c
 *
 * Created: 23/5/2026 17:24:43
 *  Author: juani
 */

#include "CONVEYOR.h"

static uint8_t running;

void CONVEYOR_Init(){
    DDRC  |=  (1 << CONVEYOR_PIN);   // PC0 como salida
    PORTC &= ~(1 << CONVEYOR_PIN);   // motor apagado al arrancar
    running = 0;
}

void CONVEYOR_Start(){
	PORTC  &= ~(1 << CONVEYOR_PIN);
    running  = 1;
}

void CONVEYOR_Stop(){
    PORTC  |= (1 << CONVEYOR_PIN);
    running  = 0;
}

uint8_t CONVEYOR_IsRunning(){
    return running;
}
