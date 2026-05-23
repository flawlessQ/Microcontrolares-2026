/*
  COMUNICATION.c

  M�dulo de comunicaci�n USART para el protocolo UNER.
  Inicializaci�n del USART en 115200 8N1 (modo doble velocidad asincr�nico).
  Codificaci�n y decodificaci�n de tramas con el formato UNER.
*/

// __________________________________________________________________________________________________________________
//|                                                                                                                  |
//|													INCLUDES			                                             |
//|__________________________________________________________________________________________________________________|

#include "COMUNICATION.h"	// Se incluye el archivo de cabecera propio.

// __________________________________________________________________________________________________________________
//|                                                                                                                  |
//|													DEFINICIONES		                                             |
//|__________________________________________________________________________________________________________________|

#define TRUE 1				// Defino "TRUE" como 1.
#define FALSE 0				// Defino "FALSE" como 0.

// __________________________________________________________________________________________________________________
//|                                                                                                                  |
//|												VARIABLES GLOBALES		                                             |
//|__________________________________________________________________________________________________________________|

uint8_t bufRX[BUFRXSIZE];	// Buffer circular de recepci�n (128 bytes).
uint8_t bufTX[BUFTXSIZE];	// Buffer circular de transmisi�n (128 bytes).

// __________________________________________________________________________________________________________________
//|                                                                                                                  |
//|											C�DIGO DE FUNCIONES			                                             |
//|__________________________________________________________________________________________________________________|

void ini_USART0 ()
{
// En esta funci�n se establecen las condiciones de la comunicaci�n USART.
// La configuraci�n del USART ser�: 115200 8N1.

/*
   Se debe de configurar el modo de operaci�n para el USART.
   De acuerdo a la tabla 20-1 de la hoja de datos del microcontrolador,
   para poder setear el modo del USART se debe de configurar el registro UBRRn (En este caso es UBRR0 para USART0) y 
   estableciendo un valor de baud rate (velocidad de transmisi�n):
	
    _____________________________________________________________
   |		  MODE			   |       Ecuaci�n para UBRRn       |
   |---------------------------|---------------------------------|
   |Normal Asincr�nico         | UBRRn = F_CPU / (16 * BAUD) - 1 |
   |---------------------------|---------------------------------|
   |Doble velocidad Asincr�nico| UBRRn = F_CPU / (8 * BAUD) - 1  |
   |---------------------------|---------------------------------|
   |Maestro sincr�nico         | UBRRn = F_CPU / (2 * BAUD) - 1  |
    -------------------------------------------------------------
	 __________________________________________________________
	| Si:                                                      |
	|                                                          |
	| F_CPU = 16MHz											   |
	| 														   |
	| Como el baud rate se lo debe establecer de acuerdo a las |
	| necesidades (velocidad de transmisi�n que se necesite),  |
	| lo establecer� en 115200:								   |
	| 														   |
	| BAUD = 115200											   |
	| 														   |
	| MODO:  Doble Velocidad Asincr�nico					   |
	| 														   |
	| Entonces:												   |
	|                                                          |
	| UBRRn = 16,3 (redondeamos en UBRRn = 16)				   |
	|__________________________________________________________|

*/
	UBRR0H = (uint8_t)(16 >> 8);		// Guardo la parte alta de 16 en UBRR0H.
	UBRR0L = (uint8_t)(16 & 0xFF);		// Guarda esa parte baja de 16 en UBRR0L.


   // Adem�s de cargar UBRR0 con el valor calculado, para usar el modo de doble velocidad asincr�nico 
   // se debe activar el bit U2X0 del registro UCSR0A.
   
	UCSR0A |= (1 << U2X0);	// Pone en 1 el bit U2X0 del registro UCSR0A.

/* Se deben de habilitar las recepciones, las transmisiones y la interrupci�n durante las transmisiones.
   Esto se realiza mediante la activaci�n de las banderas "RXCIE0", "RXENO" y "TXEN0" del registro "UCSR0B".
   RXCIE0: Habilita interrupciones durante las transmisiones.
   RXEN0: Habilita la recepci�n de datos.
   TXEN0: Habilita la transmisi�n de datos.
*/

	UCSR0B |= (1 << RXCIE0) | (1 << RXEN0) | (1 << TXEN0);	/*  Activo las banderas para habilitar:
															    interrupciones durante las transmisiones, recepci�n de datos y transmisi�n de datos. */
	
/*
   Para terminar de configurar las condiciones de comunicaci�n, se debe modificar las banderas de registro "UCSR0C".
   En este registro se establece el modo de comunicaci�n, la paridad, la selecci�n del bit de parada, el tama�o de los datos durante la comunicaci�n
   y la polaridad del reloj.
   En este momento se trabajar� en modo asincr�nico, sin paridad, 1 bit de parada y con un tama�o de datos de 8bits.
   Se dejan todas las banderas en 0 y solo se modificar�n las banderas correspondientes al tama�o de los datos.
   De acuerdo a la tabla 21-11 de la hoja de datos del microcontrolador, para poder setear el tama�o de los datos se debe modificar
   los bits "UCSZ02", "UCSZ01" y "UCSZ00" del registro "UCSR0C": 
    _______________________________
   |UCSZ02|UCSZ01|UCSZ00|  TAMA�O  |
   |------|------|------|----------|
   |   0  |   0  |   0  |  5 bits  |
   |   0  |   0  |   1  |  6 bits  |
   |   0  |   1  |   0  |  7 bits  |
   |   0  |   1  |   1  |  8 bits  |
    -------------------------------
*/

	UCSR0C |= (1 << UCSZ01) | (1 << UCSZ00);	// Modifico las banderas UCSZ01 y UCSZ00 de configuraci�n correspondiente a 8 bits de datos.

}

void ini_COM(_sRX *srx, _sTX *stx){
// En esta funci�n se inicializa buffer y protocolo.

	srx->rBuf.buf = bufRX;		// Guardo la direcci�n del primer byte del buffer de recepci�n.
	srx->rBuf.size = BUFRXSIZE;	// Guardo en la estructura el tama�o total del buffer de recepci�n.
	srx->rBuf.ir = 0;			// Inicializo el �ndice de lectura en 0.
	srx->rBuf.iw = 0;			// Inicializo el �ndice de escritura en 0.
	srx->hdrState = 0;			// Inicializo el estado del decodificador de cabecera en 0.
	srx->timeout = 0;			// Inicializo el contador de timeout en 0.
	srx->nBytes = 0;			// Inicializo en 0 la cantidad de bytes que faltan recibir o procesar del cuerpo de la trama.
	srx->cks = 0;				// Inicializo el checksum recibido/calculado en 0.
	srx->cmd = 0;				// Inicializo el campo comando en 0.
	srx->bodyIndex = 0;			// Inicializo el �ndice del cuerpo de la trama en 0.

	stx->rBuf.buf = bufTX;		// Guardo la direcci�n del primer byte del buffer de transmisi�n.
	stx->rBuf.size = BUFTXSIZE;	// Guardo el tama�o del buffer de transmisi�n.
	stx->rBuf.ir = 0;			// Inicializo el �ndice de lectura del buffer de transmisi�n en 0.
	stx->rBuf.iw = 0;			// Inicializo el �ndice de escritura del buffer de transmisi�n en 0.
}

_Bool decodeHeader(_sRX *srx)
{
/*
	En esta funci�n de valida la trama recibida por USART.
    El formato de trama esperado por el protocolo de comunicaci�n:
       _________________________________________________________________
      |     |     |     |     |        |     |     |         |          |
      | 'U' | 'N' | 'E' | 'R' | LENGTH | ':' | CMD | PAYLOAD | CHECKSUM |
      |_____|_____|_____|_____|________|_____|_____|_________|__________|

    Descripci�n de los campos:
	_______________________________________________________________________________
   | 'U','N','E','R'  : Cabecera fija que identifica el inicio de la trama.		   |
   |_______________________________________________________________________________|
   | LENGTH           : Cantidad de bytes del cuerpo de la trama.				   |
   |                    Incluye:											       |
   |                        - 1 byte de CMD										   |
   |                        - N bytes de PAYLOAD								   |
   |                        - 1 byte de CHECKSUM								   |
   |																			   |
   |                    LENGTH = payloadLen + 2									   |
   |_______________________________________________________________________________|
   | ':'              : Separador entre la cabecera y el cuerpo del mensaje.	   |
   |_______________________________________________________________________________|
   | CMD              : Byte de comando que indica la acci�n o tipo de mensaje.    |
   |_______________________________________________________________________________|
   | PAYLOAD          : Datos �tiles del mensaje (0 a PAYLOADMAX bytes).		   |
   |_______________________________________________________________________________|
   | CHECKSUM         : Byte de verificaci�n calculado con XOR de todos los		   |
   |                    bytes anteriores de la trama:							   |
   |																			   |
   |                    CHECKSUM =												   |
   |                    'U' ^ 'N' ^ 'E' ^ 'R' ^ LENGTH ^ ':' ^ CMD ^ PAYLOAD[i]    |
   |_______________________________________________________________________________|
   | Ejemplo de trama:															   |
   |																			   |	
   |     U N E R 04 : F0 12 34 XX												   |
   |																			   |
   |     donde:                                                                    |  
   |     LENGTH  =  4															   |
   |     CMD     =  F0															   |
   |     PAYLOAD =  {0x12, 0x34}												   |
   |     XX      =  checksum calculado											   |
   |_______________________________________________________________________________|
*/
	
	static uint8_t hdr [] = {'U','N','E','R',0x00,':'};				// Se define un arreglo est�tico con la cabecera esperada del protocolo.
		
	uint8_t i;														// Variable utilizada como �ndice auxiliar.
	
	_Bool decodeOK = FALSE;											// Booleano que la funci�n retornar�. La inicializ� en "FALSE".
	
	i = srx->rBuf.iw;												// Guardo en "i" el valor actual del �ndice de escritura del buffer.				
	
	while(srx->rBuf.ir != i)										// Mientras haya datos pendientes para leer, se ejecuta el bucle.
	{							
		switch(srx->hdrState)										// Se eval�a el estado actual de la m�quina de estados del protocolo.
		{							
			case 0:													// Estado inicial: busca el primer byte de la cabecera.
			
				if(srx->rBuf.buf[srx->rBuf.ir] == 'U')				// Compruebo si el primer byte es "U" para comenzar a reconocer la cabecera.
				{		 
					srx->hdrState = 1;								// Avanza al siguiente estado.
					srx->timeout = 20;								// Carga un timeout de 20 unidades.
				}
				
			break;										
			
			case 1:													// Segundo estado: busca el segundo byte de la cabecera.
			
				if(srx->rBuf.buf[srx->rBuf.ir] == 'N')				// Compruebo si el segundo byte es "N" para continuar reconociendo la cabecera.
				{
					srx->hdrState = 2;								// Avanza al siguiente estado.
					srx->timeout = 20;								// Carga un timeout de 20 unidades.
				}
			break;
			
			case 2:													// Tercer estado: busca el tercero byte de la cabecera.
			
				if(srx->rBuf.buf[srx->rBuf.ir] == 'E')				// Compruebo si el tercer byte es "E" para continuar reconociendo la cabecera.
				{
					srx->hdrState = 3;								// Avanza al siguiente estado.
					srx->timeout = 20;								// Carga un timeout de 20 unidades.
				}
			break;
				
			case 3:													// Cuarto estado: busca el cuarto byte de la cabecera.
			
				if(srx->rBuf.buf[srx->rBuf.ir] == 'R')				// Compruebo si el cuarto byte es "R" para continuar reconociendo la cabecera.
				{
					srx->hdrState = 4;								// Avanza al siguiente estado.
					srx->timeout = 20;								// Carga un timeout de 20 unidades.
				}
			
			break;
			
			case 4:													// Quinto estado: busca el quinto byte de la cabecera.
			
			srx->nBytes = srx->rBuf.buf[srx->rBuf.ir];				// Guardo el valor del byte actual, que representa la cantidad de bytes del cuerpo.
																	// El cuerpo est� constru�do por: CMD, PAYLOAD, CHECKSUM.
			if(srx->nBytes < 2)										// Verifico si la cantidad de bytes es menor a 2 (Entre el CMD Y CHECKSUM suman 2 bytes, siento este valor el m�nimo que debe de tener el cuerpo). 
			{
				srx->hdrState = 0;									// Vuelvo al estado inicial para empezar a buscar otra vez desde "U".
				srx->timeout = 0;									// Cancelo el timeout porque la cabecera actual qued� descartada.
				
				break;
			}
			
			srx->payloadLen = srx->nBytes - 2;						// Calculo la longitud del payload sin tener en cuenta los 2 bytes del CMD y el CHECKSUM. 
			
			if(srx->payloadLen > PAYLOADMAX)						// Verifico que el payload no supere el tama�o m�ximo permitido.
			{
				srx->hdrState = 0;									// Vuelvo al estado inicial para empezar a buscar otra vez desde "U".
				srx->timeout = 0;									// Cancelo el timeout porque la cabecera actual qued� descartada.
				
				break;
			}
			
			srx->hdrState = 5;										// Avanzo al siguiente estado.
			srx->bodyIndex = 0;										// Reinicio el �ndice del cuerpo.
			srx->cks = 'U' ^ 'N' ^ 'E' ^ 'R' ^ srx->nBytes ^ ':';	// Inicializo el checksum calculando los bytes de la cabecera.
			
			break;			
			
			case 5:													// Sexto estado: busca el sexto byte de la cabecera.
			if(srx->rBuf.buf[srx->rBuf.ir] == hdr[srx->hdrState])	// Comparo el byte actual del buffer con el byte esperado de la cabecera seg�n el estado actual.
			{
				srx->hdrState = 6;									// Si coincide, avanzo al siguiente estado.
			}
			else
			{														// Si el byte no coincide con lo esperado, la cabecera fall�.
				if(srx->rBuf.ir != 0)								// Compruebo si el �ndice de lectura no es 0.
				{
					srx->rBuf.ir--;									// Si no es 0, retrocede una posici�n el �ndice de lectura.
				}
				else if(srx->rBuf.ir == 0)							// Compruebo si el �ndice de lectura es igual a 0.
				{
					srx->rBuf.ir = (BUFRXSIZE-1);					// Si el �ndice de lectura es 0, como es buffer circular, vuelve al final.
				}
				
				srx->timeout = 0;									// Cancelo el timeout porque la cabecera actual qued� descartada.
				srx->hdrState = 0;									// Vuelvo al estado inicial para empezar a buscar otra vez desde "U".
			}
			break;

			case 6:													// Septimo estado: se valida el checksum.
			
			if(srx->nBytes > 1)										// Si quedan m�s de 1 byte, significa que todav�a no lleg� el checksum final.
			{
				if(srx->bodyIndex == 0)								// Si "bodyIndex" es 0, significa que este es el primer byte del cuerpo.
				{
					srx->cmd = srx->rBuf.buf[srx->rBuf.ir];			// Guarda ese byte como comando CMD.
					}
					else if(srx->bodyIndex > 0 )					// Si "bodyIndex" es mayor que 0, entonces ya no estamos en el comando, sino en el payload.
					{
						if((srx->bodyIndex - 1) < PAYLOADMAX)		// Verifico que la posici�n dentro del payload siga siendo v�lida.
						{
							srx->payload[srx->bodyIndex - 1] = srx->rBuf.buf[srx->rBuf.ir];	// Guardo el byte actual dentro del arreglo payload.
						}
						else
						{											// Si por alguna raz�n se pasa del tama�o permitido:
						srx->hdrState = 0;							// Vuelvo al estado inicial para empezar a buscar otra vez desde "U".
						srx->timeout = 0;							// Cancelo el timeout porque la cabecera actual qued� descartada.
						break;
						}
					}
				
					srx->cks ^= srx->rBuf.buf[srx->rBuf.ir];		// Actualizo el checksum calculando los bytes actuales del cuerpo.
					srx->nBytes--;									// Reduce en 1 la cantidad de bytes pendientes.
					srx->bodyIndex++;								// Avanza el �ndice del cuerpo para el pr�ximo byte.
				
				}
				else if(srx->nBytes == 1)							// Si queda exactamente 1 byte, ese byte ya no es parte del cuerpo sino que debe ser el checksum recibido.
				{
					if(srx->cks == srx->rBuf.buf[srx->rBuf.ir])		// Compara el checksum calculado con el checksum recibido.
					{
						decodeOK = TRUE;							// Si coinciden, la trama fue decodificada correctamente.
					}	
				srx->timeout = 0;									// Si no coincide, cancelo el timeout porque la cabecera actual qued� descartada.
				srx->hdrState = 0;									// Vuelvo al estado inicial para empezar a buscar otra vez desde "U".
				}

			break;
			
			default:												// En caso de que el estado actual no sea uno de los estados anteriores:
			
				srx->hdrState = 0;									// Vuelvo al estado inicial para empezar a buscar otra vez desde "U".
			
			break;
		}
		
		srx->rBuf.ir &= (srx->rBuf.size-1);							// Hago una m�scara para asegurar que el �ndice de lectura quede dentro del tama�o del buffer.
		srx->rBuf.ir++;												// Avanzo al siguiente byte del buffer.
		srx->rBuf.ir &= (srx->rBuf.size-1);							// Vuelvo a aplicar la m�scara por si el �ndice se pas� del final y debe �volver al principio�.
	}
	
return decodeOK;													// Si se encontr� una trama v�lida, retorna "TRUE" y, si no, retorna "FALSE".
}


void USART_SendByte(_sTX *stx)
{
	uint8_t ir;

	// No hay nada para enviar
	if (stx->rBuf.ir == stx->rBuf.iw)
	return;

	// El registro UDR0 todav�a no est� libre
	if (!(UCSR0A & (1 << UDRE0)))
	return;

	ir = stx->rBuf.ir;

	UDR0 = stx->rBuf.buf[ir];

	ir++;
	ir &= (stx->rBuf.size - 1);

	stx->rBuf.ir = ir;
}

void buildCMD(_sTX *stx)
{
/* En esta funci�n se arma el mensaje con el protocolo, el comando y el payload.
	El formato de trama creado por la funci�n:
	_________________________________________________________________
	|     |     |     |     |        |     |     |         |          |
	| 'U' | 'N' | 'E' | 'R' | LENGTH | ':' | CMD | PAYLOAD | CHECKSUM |
	|_____|_____|_____|_____|________|_____|_____|_________|__________|
*/		

	uint8_t i;																// Variable utilizada como �ndice auxiliar. 
	uint8_t aux_buf[7];														// Array auxiliar utilizado en la escritura del buffer de transmisi�n.
	
	stx->length = stx->payloadLen + 2;										// Length de los datos (cmd + payload + cks)
	
	aux_buf[0] = 'U';														// Le agrego al primer byte del buffer auxiliar la 'U'.
	aux_buf[1] = 'N';														// Le agrego al segundo byte del buffer auxiliar la 'N'.
	aux_buf[2] = 'E';														// Le agrego al tercer byte del buffer auxiliar la 'E'.
	aux_buf[3] = 'R';														// Le agrego al cuarto byte del buffer auxiliar la 'R'.
	aux_buf[4] = stx->payloadLen + 2;										// Le agrego al quinto byte del buffer auxiliar el payloadLen.
	aux_buf[5] = ':';														// Le agrego al sexto byte del buffer auxiliar el ':'.
	aux_buf[6] = stx->cmd;													// Le agrego al septimo byte del buffer auxiliar el cmd.

	stx->cks = 'U' ^ 'N' ^ 'E' ^ 'R' ^ stx->length ^ ':' ^ stx->cmd;		// Calculo el checksum del header y cmd.
	
	for (i = 0; i < stx->payloadLen; i++)									// Bucle para recorrer el payload mediante su tama�o.
	{
		stx->cks ^= stx->payload[i];										// Sumo al checksum los bytes del payload.
	}
	
	for (i = 0; i < 7; i++)													// Bucle para recorrer la cabecera de 6 bytes y el cmd ('U' 'N' 'E' 'R' payloadLen ':' cmd).
	{
		stx->rBuf.buf[stx->rBuf.iw] = aux_buf[i];							// Agrego la cabecera y el cmd al buffer de transmisi�n.
		
		stx->rBuf.iw++;														// Avanzo al siguiente byte del buffer.
		stx->rBuf.iw &= (stx->rBuf.size-1);									// Hago una m�scara para asegurar que el �ndice de escritura quede dentro del tama�o del buffer.
	}
	
	for (i = 0; i < stx->payloadLen; i++)									// Bucle para recorrer el payload mediante su tama�o.
	{
		stx->rBuf.buf[stx->rBuf.iw] = stx->payload[i];						// Agrego los valores del payload al buffer de transmisi�n.
		
		stx->rBuf.iw++;														// Avanzo al siguiente byte del buffer.
		stx->rBuf.iw &= (stx->rBuf.size-1);									// Hago una m�scara para asegurar que el �ndice de escritura quede dentro del tama�o del buffer.
	}
	
	stx->rBuf.buf[stx->rBuf.iw] = stx->cks;									// Agrego el checksum al buffer de transmisi�n.
	
	stx->rBuf.iw++;															// Avanzo al siguiente byte del buffer.
	stx->rBuf.iw &= (stx->rBuf.size-1);										// Hago una m�scara para asegurar que el �ndice de escritura quede dentro del tama�o del buffer.
}

