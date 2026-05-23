/*
  HCSR04.c

  Driver portable para el sensor HC-SR04.
  No accede directamente a registros del microcontrolador.
  Las operaciones de hardware se realizan mediante punteros a funci�n.

  Esta versi�n mide el pulso ECHO usando un timer de hardware pasado
  desde el main mediante punteros a funci�n.

  Supuesto de temporizado:
      F_CPU = 16 MHz, prescaler = 8 -> frecuencia del timer = 2 MHz.
      1 tick = 0,5 us.
      10 us  =  20 ticks (pulso TRIG).
      58 us/cm = 116 ticks/cm.
*/

// __________________________________________________________________________________________________________________
//|                                                                                                                  |
//|													INCLUDES			                                             |
//|__________________________________________________________________________________________________________________|

#include "HCSR04.h"		// Se incluye el archivo de cabecera propio.

// __________________________________________________________________________________________________________________
//|                                                                                                                  |
//|													DEFINICIONES		                                             |
//|__________________________________________________________________________________________________________________|

/*
   Constantes privadas del driver.

   Esta versi�n mide el pulso ECHO usando un timer de hardware pasado
   desde el main mediante punteros a funci�n.

   Supuesto usado para las constantes en ticks:
   - F_CPU = 16 MHz
   - Timer usado con prescaler 8
   - Frecuencia del timer = 2 MHz
   - 1 tick = 0,5 us

   Por lo tanto:
   - 10 us = 20 ticks
   - 30000 us = 60000 ticks
   - 58 us/cm = 116 ticks/cm
*/

#define HCSR04_PERIOD_MS              250u

#define HCSR04_TRIG_TIME_TICKS        20u
#define HCSR04_ECHO_TIMEOUT_TICKS     60000u
#define HCSR04_CM_DIVISOR_TICKS       116u

// __________________________________________________________________________________________________________________
//|                                                                                                                  |
//|											C�DIGO DE FUNCIONES			                                             |
//|__________________________________________________________________________________________________________________|

void HCSR04_Init(_sHCSR04 *sensor, _sHCSR04_IO *io)
{
	// Copio la interfaz de hardware dentro del driver.
	sensor->io = *io;

	// Dejo el sensor en reposo.
	sensor->state = HCSR04_IDLE;

	// Limpio variables internas.
	sensor->counter_us = 0;
	sensor->period_ms = HCSR04_PERIOD_MS;

	sensor->echo_time_us = 0;
	sensor->distance_cm = 0;

	// La distancia m�xima depende del montaje f�sico y se configura desde el main.
	sensor->max_distance_cm = 0;

	// Limpio todas las banderas.
	sensor->GRHCSR04 = 0;

	// Me aseguro de que TRIG arranque en bajo.
	sensor->io.trig_write(0);

	// Dejo el timer detenido y en cero.
	sensor->io.timer_stop();
	sensor->io.timer_reset();
}

void HCSR04_Start(_sHCSR04 *sensor)
{
	/*
	   Esta funci�n se conserva por compatibilidad con la estructura anterior.

	   En esta versi�n, la medici�n real se hace con HCSR04_Measure(),
	   porque necesitamos capturar el pulso ECHO completo usando el timer
	   de hardware.
	*/

	HCSR04_Measure(sensor);
}

void HCSR04_Update(_sHCSR04 *sensor)
{
	/*
	   Esta funci�n se conserva por compatibilidad con la estructura anterior.

	   En esta versi�n no se usa la m�quina de estados anterior para medir,
	   porque esa l�gica pod�a perder el pulso ECHO si el while principal
	   tardaba demasiado en volver a llamar a Update().
	*/

	(void)sensor;
}

void HCSR04_On1us(_sHCSR04 *sensor)
{
	/*
	   Esta funci�n se conserva por compatibilidad.

	   Ya no se usa para medir el ECHO, porque el ancho del pulso lo mide
	   directamente el timer de hardware.
	*/

	(void)sensor;
}

void HCSR04_On1ms(_sHCSR04 *sensor)
{
	// Incremento el contador de milisegundos hasta llegar al per�odo m�nimo entre mediciones.
	if(sensor->period_ms < HCSR04_PERIOD_MS)
	{
		sensor->period_ms++;
	}
}

void HCSR04_Measure(_sHCSR04 *sensor)
{
	uint16_t ticks = 0;

	// Solo permito medir si ya pas� el per�odo m�nimo entre mediciones.
	if(sensor->period_ms < HCSR04_PERIOD_MS)
	{
		return;
	}

	// Limpio datos anteriores.
	sensor->counter_us = 0;
	sensor->echo_time_us = 0;
	sensor->distance_cm = 0;

	// Limpio banderas anteriores.
	sensor->GRHCSR04 &= ~(1 << RDY0);
	sensor->GRHCSR04 &= ~(1 << ERR0);
	sensor->GRHCSR04 &= ~(1 << ODS0);

	// Reinicio el per�odo entre mediciones.
	sensor->period_ms = 0;

	// Estado inicial de medici�n.
	sensor->state = HCSR04_TRIG;

	/*
	   Genero el pulso TRIG de 10 us usando el timer de hardware.

	   El driver no sabe qu� timer usa el micro.
	   Solo pide:
	   - resetear timer
	   - arrancar timer
	   - leer ticks
	*/

	sensor->io.trig_write(0);

	sensor->io.timer_reset();
	sensor->io.timer_start();

	sensor->io.trig_write(1);

	while(sensor->io.timer_get_ticks() < HCSR04_TRIG_TIME_TICKS)
	{
		// Espero hasta completar el pulso de 10 us.
	}

	sensor->io.trig_write(0);

	/*
	   Espero a que ECHO suba.
	   Si no sube antes del timeout, marco error.
	*/

	sensor->state = HCSR04_WAIT_ECHO_HIGH;

	sensor->io.timer_reset();

	while(!sensor->io.echo_read())
	{
		if(sensor->io.timer_get_ticks() >= HCSR04_ECHO_TIMEOUT_TICKS)
		{
			sensor->io.timer_stop();

			sensor->GRHCSR04 |= (1 << ERR0);
			sensor->state = HCSR04_TIMEOUT;

			return;
		}
	}

	/*
	   Cuando ECHO sube, reinicio el timer.
	   Desde este momento empiezo a medir el ancho real del pulso ECHO.
	*/

	sensor->state = HCSR04_WAIT_ECHO_LOW;

	sensor->io.timer_reset();

	while(sensor->io.echo_read())
	{
		if(sensor->io.timer_get_ticks() >= HCSR04_ECHO_TIMEOUT_TICKS)
		{
			sensor->io.timer_stop();

			sensor->GRHCSR04 |= (1 << ERR0);
			sensor->state = HCSR04_TIMEOUT;

			return;
		}
	}

	// Cuando ECHO baja, leo la duraci�n medida por el timer.
	ticks = sensor->io.timer_get_ticks();

	// Detengo el timer porque ya termin� la medici�n.
	sensor->io.timer_stop();

	// Guardo el tiempo aproximado en microsegundos.
	// Con prescaler 8, cada tick equivale a 0,5 us.
	sensor->echo_time_us = ticks / 2;

	// Calculo distancia en cent�metros.
	// Como 1 cm equivale aproximadamente a 58 us:
	// 58 us * 2 ticks/us = 116 ticks/cm.
	sensor->distance_cm = ticks / HCSR04_CM_DIVISOR_TICKS;

	// Eval�o si la distancia medida corresponde a un objeto v�lido.
	if(sensor->max_distance_cm > 0)
	{
		if(sensor->distance_cm <= sensor->max_distance_cm)
		{
			sensor->GRHCSR04 |= (1 << ODS0);
		}
		else
		{
			sensor->GRHCSR04 &= ~(1 << ODS0);
		}
	}
	else
	{
		sensor->GRHCSR04 &= ~(1 << ODS0);
	}

	// Marco que hay una medici�n lista.
	sensor->GRHCSR04 |= (1 << RDY0);

	// Estado final correcto.
	sensor->state = HCSR04_DONE;
}

uint8_t HCSR04_IsReady(_sHCSR04 *sensor)
{
	// Devuelvo 1 si est� activa la bandera de medici�n lista.
	return (sensor->GRHCSR04 & (1 << RDY0)) ? 1 : 0;
}

uint8_t HCSR04_HasError(_sHCSR04 *sensor)
{
	// Devuelvo 1 si est� activa la bandera de error.
	return (sensor->GRHCSR04 & (1 << ERR0)) ? 1 : 0;
}

uint16_t HCSR04_GetDistanceCm(_sHCSR04 *sensor)
{
	// Limpio la bandera de dato listo porque la distancia ya va a ser le�da.
	sensor->GRHCSR04 &= ~(1 << RDY0);

	// Devuelvo la �ltima distancia calculada en cent�metros.
	return sensor->distance_cm;
}

uint8_t HCSR04_IsObjectDetected(_sHCSR04 *sensor)
{
	// Devuelvo 1 si la �ltima medici�n est� dentro de la distancia m�xima configurada.
	return (sensor->GRHCSR04 & (1 << ODS0)) ? 1 : 0;
}

void HCSR04_SetMaxDistanceCm(_sHCSR04 *sensor, uint16_t distance_cm)
{
	// Guardo la distancia m�xima v�lida.
	// Este valor depende del montaje f�sico y se define desde el main.
	sensor->max_distance_cm = distance_cm;
}

