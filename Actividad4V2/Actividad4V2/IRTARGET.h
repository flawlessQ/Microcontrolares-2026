/*
 * IRTARGET.h
 *
 * Created: 23/6/2026 12:21:29
 *  Author: juani
 */ 


#ifndef IRTARGET_H_
#define IRTARGET_H_

#include <stdint.h>
#include "CLASSIFIER.h"

typedef enum{
	PUSHER_1,
	PUSHER_2,
	PUSHER_3
}_ePushersOn;

void IRTARGET_Init();
void IRTARGET_RegisterBox(_eBoxType tipo);
uint8_t IRTARGET_Tick(_ePushersOn pusher);



#endif /* IRTARGET_H_ */