/*
 * IRTARGET.c
 *
 * Created: 23/6/2026 12:21:41
 *  Author: juani
 */ 
#include "IRTARGET.h"
#include <string.h>

typedef struct{
	uint8_t count[8];
	uint8_t head;
	uint8_t tail;
	uint8_t qty;
}_sIRTargetQueue;

static _sIRTargetQueue cola[3];
static uint8_t acc[3];

static void ColaPush(uint8_t pusher, uint8_t valor);

void IRTARGET_Init(){
	
	for(uint8_t i = 0; i < 3; i++){
		acc[i] = 0;
		memset(&cola[i], 0, sizeof(_sIRTargetQueue));
	}
}

static void ColaPush(uint8_t pusher, uint8_t valor){
	
	if (cola[pusher].qty < 8){
		cola[pusher].count[cola[pusher].tail] = valor;
		cola[pusher].tail = (cola[pusher].tail + 1) % 8;
		cola[pusher].qty++;
	}
}

void IRTARGET_RegisterBox(_eBoxType	tipo){
	switch(tipo){
		case BOX_SMALL:
			ColaPush(0, acc[0]);
			acc[0] = 0;
			acc[1]++;
			acc[2]++;
		break;
		case BOX_MEDIUM:
			ColaPush(1, acc[1]);
			acc[1] = 0;
			acc[0]++;
			acc[2]++;
		break;
		case BOX_BIG:
			ColaPush(2, acc[2]);
			acc[2] = 0;
			acc[1]++;
			acc[0]++;
		break;
		case BOX_NONE:
			acc[0]++;
			acc[1]++;
			acc[2]++;
		break;
	}
}

uint8_t IRTARGET_Tick(_ePushersOn pusher){
	if(cola[pusher].qty > 0){
		if(cola[pusher].count[cola[pusher].head] == 0){
			cola[pusher].head = (cola[pusher].head + 1) % 8;
			cola[pusher].qty--;
			return 1;
		}else{
			cola[pusher].count[cola[pusher].head]--;
			return 0;
		}
	}
	return 0;
}