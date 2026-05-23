#include "CLASSIFIER.h"

// Distancia de referencia: sensor al transportador sin caja (cm).
// Ajustar segun la instalacion real antes de flashear al banco.
static uint8_t reference_dist_cm = 20;

typedef struct {
	uint8_t h_small;	// Altura minima para caja pequena  (cm).
	uint8_t h_medium;	// Altura minima para caja mediana  (cm).
	uint8_t h_big;		// Altura minima para caja grande   (cm).
} _sBoxConfig;

static _sBoxConfig config = {
	.h_small  = 6,
	.h_medium = 8,
	.h_big    = 10,
};

void CLASSIFIER_Init(void)
{
	// Sin inicializacion de hardware — preparado para clasificar.
}

_eBoxType CLASSIFIER_Classify(uint8_t d_cm)
{
// Calcula la altura de la caja y la clasifica.
// Si d_cm >= reference_dist_cm no hay caja medible.

	if (d_cm >= reference_dist_cm)
		return BOX_NONE;

	uint8_t h = reference_dist_cm - d_cm;	// Altura de la caja en cm.

	if (h >= config.h_big)    return BOX_BIG;
	if (h >= config.h_medium) return BOX_MEDIUM;
	if (h >= config.h_small)  return BOX_SMALL;
	return BOX_NONE;
}

void CLASSIFIER_SetThresholds(uint8_t h_small, uint8_t h_medium, uint8_t h_big)
{
	config.h_small  = h_small;
	config.h_medium = h_medium;
	config.h_big    = h_big;
}

void CLASSIFIER_SetRefDist(uint8_t ref_cm)
{
	reference_dist_cm = ref_cm;
}
