/*
  HCSR04.c
 
  Driver portable para el sensor HC-SR04.
  No depende directamente del hardware del microcontrolador.
*/

#include "HCSR04.h"   // Se incluye el archivo de cabecera propio del driver.

/*
   Constantes privadas del driver.
   Se usan internamente para controlar tiempos y conversión de distancia.
*/
#define HCSR04_TRIG_TIME_US     10u     // Tiempo que el pin TRIG debe permanecer en nivel alto para disparar el sensor.
#define HCSR04_PERIOD_MS        250u	// Periodo mínimo entre mediciones del Echo.
#define HCSR04_ECHO_TIMEOUT_US  30000u  // Tiempo máximo de espera de respuesta del sensor, expresado en microsegundos (30 ms).
#define HCSR04_CM_DIVISOR       58      // Divisor usado para convertir el ancho del pulso ECHO en distancia.
                                        // Según esta fórmula:
                                        //      distancia_cm = tiempo_us / 58

void HCSR04_Init(_sHCSR04 *sensor, _sHCSR04_IO *io)
{
	// Copio la interfaz de hardware dentro de la estructura del sensor.
	// De esta forma el driver puede usar las funciones para manejar TRIG y leer ECHO
	// sin depender directamente de los registros del microcontrolador.
	sensor->io = *io;

	// Dejo el sensor en estado inicial, sin ninguna medición en curso.
	sensor->state = HCSR04_IDLE;

	// Inicializo el contador de microsegundos en cero.
	// Este contador se va a usar para el pulso de TRIG y para medir el tiempo de ECHO.
	sensor->counter_us = 0;

	// Cargo el período en el valor máximo para permitir que la primera medición
	// pueda arrancar inmediatamente, sin esperar los 60 ms iniciales.
	sensor->period_ms = HCSR04_PERIOD_MS;

	// Limpio el tiempo medido del pulso ECHO.
	sensor->echo_time_us = 0;

	// Limpio la distancia calculada.
	sensor->distance_cm = 0;

	// Dejo la distancia máxima en cero porque este valor depende del montaje físico
	// y se configura después desde el main.
	sensor->max_distance_cm = 0;

	// Limpio la bandera de dato listo.
	sensor->GRHCSR04 &= ~(1 << RDY0);

	// Limpio la bandera de error.
	sensor->GRHCSR04 &= ~(1 << ERR0);

	// Limpio la bandera de objeto detectado.
	sensor->GRHCSR04 &= ~(1 << ODS0);

	// Me aseguro de que el pin TRIG arranque en bajo.
	sensor->io.trig_write(0);
}

void HCSR04_Start(_sHCSR04 *sensor)
{
	// Solo permito iniciar una nueva medición si el sensor está en reposo,
	// si terminó una medición anterior o si quedó en timeout.
	// Si está en otro estado, significa que todavía hay una medición en curso.
	if(sensor->state != HCSR04_IDLE &&
	sensor->state != HCSR04_DONE &&
	sensor->state != HCSR04_TIMEOUT)
	{
		return;
	}

	// Verifico que haya pasado el tiempo mínimo entre mediciones.
	// Esto evita disparar el sensor demasiado rápido.
	if(sensor->period_ms < HCSR04_PERIOD_MS)
	{
		return;
	}

	// Reinicio el contador de microsegundos para comenzar una medición nueva.
	sensor->counter_us = 0;

	// Limpio el tiempo de ECHO anterior.
	sensor->echo_time_us = 0;

	// Limpio la distancia anterior.
	sensor->distance_cm = 0;

	// Limpio la bandera de medición lista.
	sensor->GRHCSR04 &= ~(1 << RDY0);

	// Limpio la bandera de error.
	sensor->GRHCSR04 &= ~(1 << ERR0);

	// Limpio la bandera de objeto detectado.
	sensor->GRHCSR04 &= ~(1 << ODS0);

	// Reinicio el contador de milisegundos para volver a contar el período
	// mínimo antes de permitir otra medición.
	sensor->period_ms = 0;

	// Pongo TRIG en alto para iniciar el pulso de disparo del HC-SR04.
	sensor->io.trig_write(1);

	// Cambio al estado donde se controla que el pulso TRIG dure 10 us.
	sensor->state = HCSR04_TRIG;
}

void HCSR04_On1us(_sHCSR04 *sensor)
{
	// Solo incremento el contador de microsegundos cuando el sensor está
	// en una etapa donde realmente necesita medir tiempo.
	if(sensor->state == HCSR04_TRIG ||
	sensor->state == HCSR04_WAIT_ECHO_HIGH ||
	sensor->state == HCSR04_WAIT_ECHO_LOW)
	{
		// Limito el contador para evitar que se desborde.
		// Si llega al timeout, deja de incrementarse.
		if(sensor->counter_us < HCSR04_ECHO_TIMEOUT_US)
		{
			sensor->counter_us++;
		}
	}
}

void HCSR04_Update(_sHCSR04 *sensor)
{
	// Máquina de estados principal del driver.
	// Según el estado actual, se realiza la acción correspondiente.
	switch(sensor->state)
	{
		case HCSR04_IDLE:
		// Estado de reposo. No hay medición en curso.
		break;

		case HCSR04_TRIG:

		// Mantengo TRIG en alto hasta que se cumplan los 10 us necesarios.
		if(sensor->counter_us >= HCSR04_TRIG_TIME_US)
		{
			// Bajo el TRIG para finalizar el pulso de disparo.
			sensor->io.trig_write(0);

			// Reinicio el contador para empezar a medir la espera del ECHO.
			sensor->counter_us = 0;

			// Paso al estado donde espero que ECHO suba a nivel alto.
			sensor->state = HCSR04_WAIT_ECHO_HIGH;
		}

		break;

		case HCSR04_WAIT_ECHO_HIGH:

		// Espero a que el pin ECHO pase a nivel alto.
		if(sensor->io.echo_read())
		{
			// Cuando ECHO sube, reinicio el contador para medir cuánto tiempo
			// permanece en alto.
			sensor->counter_us = 0;

			// Paso al estado donde se mide el ancho del pulso ECHO.
			sensor->state = HCSR04_WAIT_ECHO_LOW;
		}
		else if(sensor->counter_us >= HCSR04_ECHO_TIMEOUT_US)
		{
			// Si ECHO nunca sube dentro del tiempo máximo, marco error.
			sensor->GRHCSR04 |= (1 << ERR0);

			// Paso al estado de timeout.
			sensor->state = HCSR04_TIMEOUT;
		}

		break;

		case HCSR04_WAIT_ECHO_LOW:

		// En este estado ECHO ya está en alto.
		// Espero a que vuelva a bajo para saber que terminó la medición.
		if(!sensor->io.echo_read())
		{
			// Guardo cuánto tiempo estuvo ECHO en alto.
			sensor->echo_time_us = sensor->counter_us;

			// Calculo la distancia en centímetros usando el divisor definido.
			sensor->distance_cm = sensor->echo_time_us / HCSR04_CM_DIVISOR;

			// Marco que ya hay una medición lista para ser leída.
			sensor->GRHCSR04 |= (1 << RDY0);

			// Paso al estado de medición terminada correctamente.
			sensor->state = HCSR04_DONE;
		}
		else if(sensor->counter_us >= HCSR04_ECHO_TIMEOUT_US)
		{
			// Si ECHO queda en alto demasiado tiempo, marco error.
			sensor->GRHCSR04 |= (1 << ERR0);

			// Paso al estado de timeout.
			sensor->state = HCSR04_TIMEOUT;
		}

		break;

		case HCSR04_DONE:
		// La medición terminó correctamente.
		// El driver queda esperando que se inicie una nueva medición.
		break;

		case HCSR04_TIMEOUT:
		// Hubo un timeout.
		// El driver queda esperando que se inicie una nueva medición.
		break;

		default:
		// Si por algún motivo el estado no es válido, vuelvo a IDLE.
		sensor->state = HCSR04_IDLE;
		break;
	}
}

void HCSR04_On1ms(_sHCSR04 *sensor)
{
	// Incremento el contador de milisegundos hasta llegar al período mínimo entre mediciones.
	if(sensor->period_ms < HCSR04_PERIOD_MS)
	{
		sensor->period_ms++;
	}
}

uint8_t HCSR04_IsReady(_sHCSR04 *sensor)
{
	// Devuelvo 1 si está activa la bandera de medición lista.
	// Devuelvo 0 si todavía no hay dato nuevo.
	return (sensor->GRHCSR04 & (1 << RDY0)) ? 1 : 0;
}

uint8_t HCSR04_HasError(_sHCSR04 *sensor)
{
	// Devuelvo 1 si está activa la bandera de error.
	// Devuelvo 0 si no hubo error.
	return (sensor->GRHCSR04 & (1 << ERR0)) ? 1 : 0;
}

uint16_t HCSR04_GetDistanceCm(_sHCSR04 *sensor)
{
	sensor->GRHCSR04 &= ~(1 << RDY0); 	// Limpio la bandera de dato listo porque la distancia ya va a ser leída.

	return sensor->distance_cm;			// Devuelvo la última distancia calculada en centímetros.
}

uint8_t HCSR04_IsObjectDetected(_sHCSR04 *sensor)
{
	// Devuelvo 1 si está activa la bandera de objeto detectado.
	// Devuelvo 0 si no se detectó un objeto válido.
	return (sensor->GRHCSR04 & (1 << ODS0)) ? 1 : 0;
}

void HCSR04_SetMaxDistanceCm(_sHCSR04 *sensor, uint16_t distance_cm)
{
	// Guardo la distancia máxima válida.
	// Este valor no lo define el driver porque depende del montaje físico.
	sensor->max_distance_cm = distance_cm;
}