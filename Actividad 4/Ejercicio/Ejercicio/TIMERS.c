/*
  TIMERS.c

  Inicializaci�n de los timers del ATmega328P.
  - TIMER0: modo CTC, prescaler 64, OCR0A=249 -> interrupci�n cada 1ms.
  - TIMER1: modo CTC, prescaler 64, OCR1A=249 -> interrupci�n cada 1ms.
  - TIMER2: modo CTC, prescaler 8,  OCR2A=1   -> flag cada 1us (polling).
*/

// __________________________________________________________________________________________________________________
//|                                                                                                                  |
//|													INCLUDES			                                             |
//|__________________________________________________________________________________________________________________|

#include "TIMERS.h"	// Se incluye el archivo de cabecera propio.

// __________________________________________________________________________________________________________________
//|                                                                                                                  |
//|											C�DIGO DE FUNCIONES			                                             |
//|__________________________________________________________________________________________________________________|

void ini_TIMER2 ()
{
// En esta funci�n se modifica un timer para que pueda generar un evento cada 1us, no bloquente y sin interrupciones, utilizando el TIMER2 en modo CTC.

	TCNT2 = 0;		// Seteo en 0 el registro "TCNT2".

/* De acuerdo a la tabla 18-8 de la hoja de datos del microcontrolador, para colocar el TIMER2 en modo CTC se debe
   tocar los bits WGM20 y WGM21 del registro TCCR2A, y el bit WGM22 del registro TCCR2B.
    ____________________________________
   |WGM22|WGM21|WGM20|MODE              |
   |-----|-----|-----|------------------|
   |  0  |  0  |  0  |NORMAL            |
   |  0  |  0  |  1  |PWM, PHASE CORRECT|
   |  0  |  1  |  0  |CTC               |
   |  0  |  1  |  1  |FAST PWM          |
    ------------------------------------
   Se prefiere utilizar el modo CTC ya que nos permite una mejor precisi�n en el timer para 1us. */

	TCCR2A = TCCR2A;			// Limpio el registro "TCCR2A" antes de comenzar.
	TCCR2B = TCCR2B;			// Limpio el registro "TCCR2B" antes de comenzar.

	TCCR2A |= (1 << WGM21);		// En TCCR2A pongo el bit de WGM21 en 1.

/* Para realizar el contador de 1us definimos el prescaler en 8 (clk 8) y siendo 16MHz la frecuencia del cristal del microcontrolador.
   Luego se resuelve la siguientes ecuaciones:
	 ______________________________
	|							   |
	|	16MHz / prescaler = 2MHz   |
	|	1 / 2MHz  = 0.5us          |
	|                              |
	|Como se busca 1us:            |
	|                              |
	|	1us / 0.5us = 2 ticks      |
	|                              |
	|Pero como comienza contando   |
	|desde 0:                      |
	|                              |
	|	OCR2A = 2                  |
	|______________________________|
*/

/*De acuerdo a la tabla 18-9 de la hoja de datos del microcontrolador, para poder setear el precaler se debe modificar
  los bits CS20, CS21 y CS22 del registro TCCR2B:
    ____________________________
   |CS22 |CS21 |CS20 |PRESCALER |
   |-----|-----|-----|----------|
   |  0  |  1  |  0  |   8      |
   |  0  |  1  |  1  |   64     |
   |  1  |  0  |  0  |   256    |
   |  1  |  0  |  1  |   1024   |
    ----------------------------
  En este caso, al utilizar un precaler de 8, pondremos en 1 los bit CS20 y CS21. */

	TCCR2B |= (1 << CS21);	// En TCCR2B pongo el bit de CS21 en 1.

// Como se mencion� anteriormente, se debe establecer un OCR2A en 2.
	
	OCR2A = 1;	 // Se establece el valor para OCR2A. (1 -> 2 ticks x 0.5us = 1us exacto)

// De este modo TIMER2 har�: 0 -> 1 -> 2.

}

void ini_TIMER1 ()
{
// En esta funci�n se modifica un timer para que pueda generar un evento cada 1ms, no bloquente y con interrupciones, utilizando el TIMER1 en modo CTC.

//Considerando que es un timer de 16 bits, para setear el contador TCNT1 hay que hacerlo en su parte alta (high) y en su parte baja (low).
	
	TCNT1H = 0;		// Seteo en 0 el registro "TCNT1H".
	TCNT1L = 0;		// Seteo en 0 el registro "TCNT1L".

/* De acuerdo a la tabla 16-4 de la hoja de datos del microcontrolador, para colocar el TIMER1 en modo CTC se debe
   tocar los bits WGM10 y WGM11 del registro TCCR1A, y los bits WGM12 y WGM13 del registro TCCR1B.
    _________________________________________________
   |WGM13|WGM12|WGM11|WGM10|		  MODE			 |
   |-----|-----|-----|-----|-------------------------|
   |  0  |  0  |  0  |  0  |NORMAL                   |
   |  0  |  0  |  0  |  1  |PWM, PHASE CORRECT       |
   |  0  |  1  |  0  |  0  |CTC                      |
   |  0  |  1  |  0  |  1  |FAST PWM				 |
   |  1  |  0  |  0  |  0  |FREQUENCY & PHASE CORRECT|
    -------------------------------------------------

   Se prefiere utilizar el modo CTC ya que nos permite una mejor precisi�n en el timer para 1ms. */

	TCCR1A = TCCR1A;			// Limpio el registro "TCCR1A" antes de comenzar.
	TCCR1B = TCCR1B;			// Limpio el registro "TCCR1B" antes de comenzar.

	TCCR1B = (1 << WGM12);	// En TCCR1B pongo el bit de la bandera WGM12 en 1.

/*De acuerdo a la tabla 16-5 de la hoja de datos del microcontrolador, para poder setear el precaler se debe modificar
  los bits CS10, CS11 y CS12 del registro TCCR1B:
    ____________________________
   |CS12 |CS11 |CS10 |PRESCALER |
   |-----|-----|-----|----------|
   |  0  |  1  |  0  |   8      |
   |  0  |  1  |  1  |   64     |
   |  1  |  0  |  0  |   256    |
   |  1  |  0  |  1  |   1024   |
    ----------------------------
  En este caso, al utilizar un precaler de 64, pondremos en 1 los bit CS10 y CS11. */

	TCCR1B |= (1 << CS11) | (1 << CS10);	// Activo los bits de las banderas CS11 y CS10 del registro TCCR1B.

/* Para realizar la interrupci�n de 1ms definimos el prescaler en 64 (clk 64) y siendo 16MHz la frecuencia del cristal del microcontrolador (F_CPU).
   Luego se resuelve la siguientes ecuaciones:
	 ______________________________
	|							   |
	|	16MHz / prescaler = 250KHz |
	|	1 / 250KHz  = 4us          |
	|                              |
	|Como se busca 1ms:            |
	|                              |
	|	1ms / 4us = 250 ticks      |
	|                              |
	|Pero como comienza contando   |
	|desde 0:                      |
	|                              |
	|	OCR1A = 249                |
	|______________________________|
*/
// Como se mencion� anteriormente, se debe establecer un OCR1A en 249. Debe de hacerse en su parte alta (high) y en su parte baja (low).

	OCR1AH = (uint8_t)(249 >> 8);		// Se guardan los bits m�s significativos en el registro OCR1AH.
	OCR1AL = (uint8_t)(249 & 0xFF);		// Se guardan los bits menos significativos en el registro OCR1AL.

// De este modo TIMER1 har�: 0 -> 1 -> 2 -> 3 -> ... -> 249 .

/* Se debe permitir la interrupci�n que suceder� durante cada comparaci�n.
   Para que suceda la interrupci�n, se debe de activar la bandera "OCIE1A" situada en el registro "TIMSK1".
   El registro "TIMSK1" es un registro que gestiona interrupciones para determinados eventos.
*/
	
	TIMSK1 |= (1 << OCIE1A);	// Activo la bandera "OCIE1A" en el registro "TIMSK1".
	
}

void ini_TIMER0 ()
{
// En esta funci�n se modifica un timer para que pueda generar un evento cada 1ms, no bloquente y con interrupciones, utilizando el TIMER0 en modo CTC.

	TCNT0 = 0;		// Seteo en 0 el registro "TCNT0".

/* De acuerdo a la tabla 15-8 de la hoja de datos del microcontrolador, para colocar el TIMER0 en modo CTC se debe
   tocar los bits WGM00 y WGM01 del registro TCCR0A, y el bit WGM02 del registro TCCR0B.
    ____________________________________
   |WGM02|WGM01|WGM00|MODE              |
   |-----|-----|-----|------------------|
   |  0  |  0  |  0  |NORMAL            |
   |  0  |  0  |  1  |PWM, PHASE CORRECT|
   |  0  |  1  |  0  |CTC               |
   |  0  |  1  |  1  |FAST PWM          |
    ------------------------------------
   Se prefiere utilizar el modo CTC ya que nos permite una mejor precisi�n en el timer para 1ms. */

	TCCR0A = TCCR0A;			// Limpio el registro "TCCR1A" antes de comenzar.
	TCCR0B = TCCR0B;			// Limpio el registro "TCCR1B" antes de comenzar.

	TCCR0A |= (1 << WGM01);		// En TCCR0A pongo el bit de WGM01 en 1.

/* Para realizar la interrupci�n de 1ms definimos el prescaler en 64 (clk 64) y siendo 16MHz la frecuencia del cristal del microcontrolador.
   Luego se resuelve la siguientes ecuaciones:
	 ______________________________
	|							   |
	|	16MHz / prescaler = 250KHz |
	|	1 / 250KHz  = 4us          |
	|                              |
	|Como se busca 1ms:            |
	|                              |
	|	1ms / 4us = 250 ticks      |
	|                              |
	|Pero como comienza contando   |
	|desde 0:                      |
	|                              |
	|	OCR0A = 249                |
	|______________________________|
*/

/*De acuerdo a la tabla 15-9 de la hoja de datos del microcontrolador, para poder setear el precaler se debe modificar
  los bits CS00, CS01 y CS02 del registro TCCR0B:
    ____________________________
   |CS02 |CS01 |CS00 |PRESCALER |
   |-----|-----|-----|----------|
   |  0  |  1  |  0  |   8      |
   |  0  |  1  |  1  |   64     |
   |  1  |  0  |  0  |   256    |
   |  1  |  0  |  1  |   1024   |
    ----------------------------
  En este caso, al utilizar un precaler de 64, pondremos en 1 los bit CS00 y CS01. */

	TCCR0B |= (1 << CS01) | (1 << CS00);	// En TCCR0B pongo los bits de CS01 y CS00 en 1.

// Como se mencion� anteriormente, se debe establecer un OCR0A en 249.
	
	OCR0A = 249;	 // Se establece el valor para OCR0A.

// De este modo TIMER0 har�: 0 -> 1 -> 2 -> 3 -> ... -> 249

/* Se debe permitir la interrupci�n que suceder� durante cada comparaci�n.
   Para que suceda la interrupci�n, se debe de activar la bandera "OCIE0A" situada en el registro "TIMSK0".
   El registro "TIMSK0" es un registro que gestiona interrupciones para determinados eventos.
*/
	
	TIMSK0 |= (1 << OCIE0A);	// Activo la bandera "OCIE0A" en el registro "TIMSK0".

}