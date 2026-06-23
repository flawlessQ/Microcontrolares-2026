/*
  HCSR04.c

  Driver portable para el sensor HC-SR04.
  No accede directamente a registros del microcontrolador.
  Las operaciones de hardware se realizan mediante punteros a función.
*/

#include "HCSR04.h"

static uint16_t HCSR04_ConvertUsToCm(uint32_t echo_us)            
{
// Funcion interna que convierte tiempo de Echo en microsegundos a centimetros.
	
	uint16_t distance_cm = 0;															// Variable local donde se guardara la distancia calculada.

	distance_cm = (uint16_t)((echo_us + (HCSR04_CM_DIVISOR / 2)) / HCSR04_CM_DIVISOR);	// Convierte us a cm y redondea sumando la mitad del divisor.

	return distance_cm;																	// Devuelve la distancia calculada en centimetros.
}

static void HCSR04_AddSample(_sHCSR04_t *sensor, uint16_t distance_cm) 
{
// Funcion interna que guarda una nueva muestra en el buffer circular.

	sensor->samples[sensor->sample_index] = distance_cm;          // Guarda la distancia medida en la posicion actual del buffer.

	sensor->sample_index++;                                       // Avanza el indice para que la proxima muestra se guarde en la siguiente posicion.

	if (sensor->sample_index >= HCSR04_MAX_SAMPLES)               // Verifica si el indice llego al final del arreglo.
	{
		sensor->sample_index = 0;                                 // Si llego al final, vuelve a cero para formar un buffer circular.
	}

	if (sensor->sample_count < HCSR04_MAX_SAMPLES)                // Verifica si todavia no se lleno todo el buffer.
	{
		sensor->sample_count++;                                   // Aumenta la cantidad de muestras validas almacenadas.
	}
}

_sHCSR04_status_t HCSR04_Init(_sHCSR04_t *sensor, HCSR04_TriggerFn_t trigger, HCSR04_ReadEchoUsFn_t read_echo_us, void *context)
{
	uint8_t i = 0;                                                // Variable auxiliar para recorrer el arreglo de muestras.

	if (sensor == 0)                                              // Verifica si el puntero al sensor es nulo.
	{
		return HCSR04_ERROR_NULL;                                 // Devuelve error porque no se puede trabajar con un sensor inexistente.
	}

	if (trigger == 0)                                             // Verifica si el puntero a la funcion Trigger es nulo.
	{
		return HCSR04_ERROR_NULL;                                 // Devuelve error porque la libreria necesita una funcion para disparar el sensor.
	}

	if (read_echo_us == 0)                                        // Verifica si el puntero a la funcion de lectura Echo es nulo.
	{
		return HCSR04_ERROR_NULL;                                 // Devuelve error porque la libreria necesita una funcion para medir el Echo.
	}

	sensor->trigger = trigger;                                    // Guarda la funcion externa encargada de generar el pulso Trigger.

	sensor->read_echo_us = read_echo_us;                          // Guarda la funcion externa encargada de medir el pulso Echo.

	sensor->context = context;                                    // Guarda el puntero generico al hardware definido en el main.

	sensor->sample_count = 0;                                     // Inicializa la cantidad de muestras validas en cero.

	sensor->sample_index = 0;                                     // Inicializa el indice del buffer circular en la primera posicion.

	sensor->min_distance_cm = 2;                                  // Define la distancia minima valida aproximada del HC-SR04 en centimetros.

	sensor->max_distance_cm = 400;                                // Define la distancia maxima valida aproximada del HC-SR04 en centimetros.

	sensor->tolerance_cm = 1;                                     // Define una tolerancia de 1 cm para agrupar mediciones similares.

	for (i = 0; i < HCSR04_MAX_SAMPLES; i++)                      // Recorre todas las posiciones del arreglo de muestras.
	{
		sensor->samples[i] = 0;                                   // Inicializa cada muestra en cero para evitar valores basura.
	}

	return HCSR04_OK;                                             // Devuelve estado correcto porque la inicializacion finalizo sin errores.
}

_sHCSR04_status_t HCSR04_MeasureCm(_sHCSR04_t *sensor, uint16_t *distance_cm)
{
	uint32_t echo_us = 0;                                         // Variable donde se guardara el tiempo del pulso Echo en microsegundos.

	uint16_t distance = 0;											// Variable donde se guardara la distancia calculada en centimetros.

	uint8_t echo_status = 0;										// Variable donde se guardara si la medicion del Echo fue correcta o no.

	if (sensor == 0)												// Verifica si el puntero al sensor es nulo.
	{
		return HCSR04_ERROR_NULL;									// Devuelve error porque no se puede medir con un sensor inexistente.
	}

	if (distance_cm == 0)											// Verifica si el puntero donde se debe guardar la distancia es nulo.
	{
		return HCSR04_ERROR_NULL;									// Devuelve error porque no hay donde entregar el resultado de la medicion.
	}

	sensor->trigger(sensor->context);								// Llama a la funcion externa que genera el pulso Trigger del HC-SR04.

	echo_status = sensor->read_echo_us(sensor->context, &echo_us);	// Llama a la funcion externa que mide el tiempo del pulso Echo.

	if (echo_status == 0)											// Verifica si la medicion del Echo fallo.
	{
		return HCSR04_ERROR_TIMEOUT;	                            // Devuelve error de timeout porque no se obtuvo una medicion valida.
	}

	distance = HCSR04_ConvertUsToCm(echo_us);						// Convierte el tiempo medido en microsegundos a distancia en centimetros.

	if (distance < sensor->min_distance_cm)							// Verifica si la distancia calculada esta por debajo del rango valido.
	{
		return HCSR04_ERROR_OUT_OF_RANGE;							// Devuelve error porque la distancia es menor a la permitida.
	}

	if (distance > sensor->max_distance_cm)							// Verifica si la distancia calculada esta por encima del rango valido.
	{
		return HCSR04_ERROR_OUT_OF_RANGE;							// Devuelve error porque la distancia es mayor a la permitida.
	}

	HCSR04_AddSample(sensor, distance);								// Guarda la medicion valida en el buffer de muestras.

	*distance_cm = distance;										// Entrega la distancia medida al usuario mediante el puntero recibido.

	return HCSR04_OK;												// Devuelve estado correcto porque la medicion fue realizada con exito.
}

_sHCSR04_status_t HCSR04_GetStableCm(_sHCSR04_t *sensor, uint8_t required_repetitions, uint16_t *stable_distance_cm)
{
	uint8_t i = 0;                                                // Variable auxiliar para recorrer las muestras como referencia.

	uint8_t j = 0;                                                // Variable auxiliar para comparar cada muestra con la referencia.

	uint8_t count = 0;                                            // Cantidad de muestras similares encontradas para una referencia.

	uint8_t best_count = 0;                                       // Mayor cantidad de muestras similares encontradas.

	uint16_t reference = 0;                                       // Muestra tomada como referencia para formar un grupo.

	uint16_t value = 0;                                           // Muestra que se compara contra la referencia.

	uint16_t sum = 0;                                             // Suma de las muestras similares del grupo actual.

	uint16_t best_sum = 0;                                        // Suma de las muestras similares del mejor grupo encontrado.

	uint16_t difference = 0;                                      // Diferencia absoluta entre dos mediciones.

	if (sensor == 0)                                              // Verifica si el puntero al sensor es nulo.
	{
		return HCSR04_ERROR_NULL;                                 // Devuelve error porque no se puede analizar un sensor inexistente.
	}

	if (stable_distance_cm == 0)                                  // Verifica si el puntero donde se debe guardar la distancia estable es nulo.
	{
		return HCSR04_ERROR_NULL;                                 // Devuelve error porque no hay donde entregar el resultado estable.
	}

	if (sensor->sample_count == 0)                                // Verifica si todavia no hay muestras guardadas.
	{
		return HCSR04_ERROR_NOT_ENOUGH_SAMPLES;                   // Devuelve error porque no existen mediciones para analizar.
	}

	if (required_repetitions == 0)                                // Verifica si se pidieron cero repeticiones.
	{
		required_repetitions = 1;                                 // Corrige el valor minimo para que al menos una muestra pueda considerarse valida.
	}

	for (i = 0; i < sensor->sample_count; i++)                    // Recorre cada muestra guardada como posible referencia.
	{
		reference = sensor->samples[i];                           // Toma la muestra actual como valor de referencia.

		count = 0;                                                // Reinicia el contador de muestras similares para este grupo.

		sum = 0;                                                  // Reinicia la suma de muestras similares para este grupo.

		for (j = 0; j < sensor->sample_count; j++)                // Compara la referencia contra todas las muestras guardadas.
		{
			value = sensor->samples[j];                           // Toma una muestra para compararla con la referencia.

			if (value >= reference)                               // Verifica si el valor comparado es mayor o igual que la referencia.
			{
				difference = value - reference;                   // Calcula la diferencia absoluta entre value y reference.
			}
			else                                                  // Entra cuando value es menor que reference.
			{
				difference = reference - value;                   // Calcula la diferencia absoluta entre reference y value.
			}

			if (difference <= sensor->tolerance_cm)               // Verifica si ambas mediciones son suficientemente parecidas.
			{
				count++;                                          // Aumenta la cantidad de mediciones que pertenecen al mismo grupo.

				sum = sum + value;                                // Suma el valor de la medicion similar para luego promediar el grupo.
			}
		}

		if (count > best_count)                                   // Verifica si el grupo actual tiene mas coincidencias que el mejor anterior.
		{
			best_count = count;                                   // Guarda la cantidad de coincidencias del mejor grupo.

			best_sum = sum;                                       // Guarda la suma del mejor grupo encontrado.
		}
	}

	if (best_count < required_repetitions)                        // Verifica si el mejor grupo no alcanza las repeticiones minimas solicitadas.
	{
		return HCSR04_ERROR_NOT_ENOUGH_SAMPLES;                   // Devuelve error porque no hay suficientes mediciones similares.
	}

	*stable_distance_cm = (uint16_t)((best_sum + (best_count / 2)) / best_count); // Calcula y entrega el promedio redondeado del grupo mas repetido.

	return HCSR04_OK;											  // Devuelve estado correcto porque se encontro una distancia estable.
}

void HCSR04_ClearSamples(_sHCSR04_t *sensor)
{
	uint8_t i = 0;                                                // Variable auxiliar para recorrer el arreglo de muestras.

	if (sensor == 0)                                              // Verifica si el puntero al sensor es nulo.
	{
		return;                                                   // Sale de la funcion porque no hay estructura que limpiar.
	}

	sensor->sample_count = 0;                                     // Borra la cantidad de muestras validas almacenadas.

	sensor->sample_index = 0;                                     // Reinicia el indice del buffer circular.

	for (i = 0; i < HCSR04_MAX_SAMPLES; i++)                      // Recorre todas las posiciones del arreglo de muestras.
	{
		sensor->samples[i] = 0;                                   // Borra cada muestra almacenada.
	}
}

