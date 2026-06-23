/*
 * IR.c
 *
 * Created: 23/5/2026 17:23:13
 *  Author: juani
 */ 
#include "IR.h"

static uint8_t lastState[4];
static uint8_t actualState[4];
static uint8_t contDB[4];
static uint8_t flag [4];

void IR_Init(){
	DDRD &= ~(1 << PD5);
	DDRD &= ~(1 << PD2);
	DDRD &= ~(1 << PD3);
	DDRD &= ~(1 << PD4);
	
	for(uint8_t i = 0; i < 4; i++){
		lastState[i] = 1;
		actualState[i] = 1;
		contDB[i] = 0;
		flag[i] = 0;
	}
	
}

void IR_Update(){
	for (uint8_t i = 0; i < 4; i++){
		if(contDB[i] > 0){
			contDB[i]--;
		}else{
			if(i == 0){
				actualState[i] = (PIND & (1 << PD5)) ? 1 : 0;
			}else if (i == 1){
				actualState[i] = (PIND & (1 << PD2)) ? 1 : 0;
			}else if(i == 2){
				actualState[i] = (PIND & (1 << PD3)) ? 1 : 0;
			}else{
				actualState[i] = (PIND & (1 << PD4)) ? 1 : 0;
			}
			
			if(lastState[i] == 1 &&  actualState[i] == 0){
				contDB [i] = 20;
				flag[i] = 1;
			}
			
			lastState[i] = actualState[i];
		}
	}
}

uint8_t IR_FallingEdge(_eIRID id){
	uint8_t flag2;
	
	flag2 = flag[id];
	
	flag[id] = 0;
	
	return flag2;
	
}