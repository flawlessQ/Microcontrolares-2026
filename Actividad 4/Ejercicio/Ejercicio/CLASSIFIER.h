#ifndef CLASSIFIER_H_
#define CLASSIFIER_H_

#include <avr/io.h>

typedef enum {
	BOX_NONE   = 0,	// Sin caja detectada.
	BOX_SMALL  = 1,	// Caja pequena  (h = 6 cm).
	BOX_MEDIUM = 2,	// Caja mediana  (h = 8 cm).
	BOX_BIG    = 3,	// Caja grande   (h = 10 cm).
} _eBoxType;

void      CLASSIFIER_Init(void);
_eBoxType CLASSIFIER_Classify(uint8_t d_cm);	// Recibe distancia medida (cm), retorna tipo de caja.
void      CLASSIFIER_SetThresholds(uint8_t h_small, uint8_t h_medium, uint8_t h_big);
void      CLASSIFIER_SetRefDist(uint8_t ref_cm);

#endif /* CLASSIFIER_H_ */
