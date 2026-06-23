/*
 * CLASIFFIER.c
 *
 * Created: 23/5/2026 17:24:09
 *  Author: juani
 */

#include "CLASSIFIER.h"

static _sClassifierConfig dist;

void CLASSIFIER_Init(){
	dist.ref_dist = 20;
	dist.h_small = 6;
	dist.h_medium = 8;
	dist.h_big = 10;
}

void CLASSIFIER_SetConfig(_sClassifierConfig config){
	dist = config;
}

_eBoxType CLASSIFIER_Classify(uint8_t d_cm){
	uint8_t h;
	h = dist.ref_dist - d_cm;
	
	if(h >= dist.h_small && h < dist.h_small + 1){
		return BOX_SMALL;
	}else if(h >= dist.h_medium && h < dist.h_medium + 1){
		return BOX_MEDIUM;
	}else if(h >= dist.h_big && h < dist.h_big + 1){
		return BOX_BIG;
	}else{
		return BOX_NONE;
	}
}
