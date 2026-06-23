/*
 * CLASIFFIER.h
 *
 * Created: 23/5/2026 17:24:20
 *  Author: juani
 */ 


#ifndef CLASIFFIER_H_
#define CLASIFFIER_H_

#include <stdint.h>
#include <avr/io.h>

typedef enum{
	BOX_SMALL,
	BOX_MEDIUM,
	BOX_BIG,
	BOX_NONE
}_eBoxType;

typedef struct{
	uint8_t h_small;
	uint8_t h_medium;
	uint8_t h_big;
	uint8_t ref_dist;
}_sClassifierConfig;

void CLASSIFIER_Init();
_eBoxType CLASSIFIER_Classify(uint8_t d_cm);
void CLASSIFIER_SetConfig (_sClassifierConfig config);




#endif /* CLASIFFIER_H_ */