/*
 * IR.h
 *
 * Created: 23/5/2026 17:23:03
 *  Author: juani
 */ 


#ifndef IR_H_
#define IR_H_

#include <stdint.h>
#include <avr/io.h>

typedef enum{
	IR0,
	IR1,
	IR2,
	IR3
}_eIRID;

void IR_Init();
void IR_Update();
uint8_t IR_FallingEdge(_eIRID id);




#endif /* IR_H_ */