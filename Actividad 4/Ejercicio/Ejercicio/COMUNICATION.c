/*
  COMUNICATION.c
 
  Inicialización del USART en 115200 8N1.
  Decodificación de los datos recibidos.
*/
#include "COMUNICATION.h"	// Se incluye el archivo de cabecera propio.

#define TRUE 1				// Defino "TRUE" como 1. 
#define FALSE 0				// Defino "FALSE" como 0. 

uint8_t bufRX[BUFRXSIZE];	// Buffer de recepción.
uint8_t bufTX[BUFTXSIZE];	// Buffer de transmisión.

void ini_USART0 ()
{
// En esta función se establecen las condiciones de la comunicación USART.
// La configuración del USART será: 115200 8N1.

/*
   Se debe de configurar el modo de operación para el USART.
   De acuerdo a la tabla 20-1 de la hoja de datos del microcontrolador,
   para poder setear el modo del USART se debe de configurar el registro UBRRn (En este caso es UBRR0 para USART0) y 
   estableciendo un valor de baud rate (velocidad de transmisión):
	
    _____________________________________________________________
   |		  MODE			   |       Ecuación para UBRRn       |
   |---------------------------|---------------------------------|
   |Normal Asincrónico         | UBRRn = F_CPU / (16 * BAUD) - 1 |
   |---------------------------|---------------------------------|
   |Doble velocidad Asincrónico| UBRRn = F_CPU / (8 * BAUD) - 1  |
   |---------------------------|---------------------------------|
   |Maestro sincrónico         | UBRRn = F_CPU / (2 * BAUD) - 1  |
    -------------------------------------------------------------
	 __________________________________________________________
	| Si:                                                      |
	|                                                          |
	| F_CPU = 16MHz											   |
	| 														   |
	| Como el baud rate se lo debe establecer de acuerdo a las |
	| necesidades (velocidad de transmisión que se necesite),  |
	| lo estableceré en 115200:								   |
	| 														   |
	| BAUD = 115200											   |
	| 														   |
	| MODO:  Doble Velocidad Asincrónico					   |
	| 														   |
	| Entonces:												   |
	|                                                          |
	| UBRRn = 16,3 (redondeamos en UBRRn = 16)				   |
	|__________________________________________________________|

*/
	UBRR0H = (uint8_t)(16 >> 8);		// Guardo la parte alta de 16 en UBRR0H.
	UBRR0L = (uint8_t)(16 & 0xFF);		// Guarda esa parte baja de 16 en UBRR0L.


   // Además de cargar UBRR0 con el valor calculado, para usar el modo de doble velocidad asincrónico 
   // se debe activar el bit U2X0 del registro UCSR0A.
   
	UCSR0A |= (1 << U2X0);	// Pone en 1 el bit U2X0 del registro UCSR0A.

/* Se deben de habilitar las recepciones, las transmisiones y la interrupción durante las transmisiones.
   Esto se realiza mediante la activación de las banderas "RXCIE0", "RXENO" y "TXEN0" del registro "UCSR0B".
   RXCIE0: Habilita interrupciones durante las transmisiones.
   RXEN0: Habilita la recepción de datos.
   TXEN0: Habilita la transmisión de datos.
*/

	UCSR0B |= (1 << RXCIE0) | (1 << RXEN0) | (1 << TXEN0);	/*  Activo las banderas para habilitar:
															    interrupciones durante las transmisiones, recepción de datos y transmisión de datos. */
	
/*
   Para terminar de configurar las condiciones de comunicación, se debe modificar las banderas de registro "UCSR0C".
   En este registro se establece el modo de comunicación, la paridad, la selección del bit de parada, el tamaño de los datos durante la comunicación
   y la polaridad del reloj.
   En este momento se trabajará en modo asincrónico, sin paridad, 1 bit de parada y con un tamaño de datos de 8bits.
   Se dejan todas las banderas en 0 y solo se modificarán las banderas correspondientes al tamaño de los datos.
   De acuerdo a la tabla 21-11 de la hoja de datos del microcontrolador, para poder setear el tamaño de los datos se debe modificar
   los bits "UCSZ02", "UCSZ01" y "UCSZ00" del registro "UCSR0C": 
    _______________________________
   |UCSZ02|UCSZ01|UCSZ00|  TAMAÑO  |
   |------|------|------|----------|
   |   0  |   0  |   0  |  5 bits  |
   |   0  |   0  |   1  |  6 bits  |
   |   0  |   1  |   0  |  7 bits  |
   |   0  |   1  |   1  |  8 bits  |
    -------------------------------
*/

	UCSR0C |= (1 << UCSZ01) | (1 << UCSZ00);	// Modifico las banderas UCSZ01 y UCSZ00 de configuración correspondiente a 8 bits de datos.

}

void ini_COM(_sRX *srx, _sTX *stx){
// En esta función se inicializa buffer y protocolo.

	srx->rBuf.buf = bufRX;		// Guardo la dirección del primer byte del buffer de recepción.
	srx->rBuf.size = BUFRXSIZE;	// Guardo en la estructura el tamaño total del buffer de recepción.
	srx->rBuf.ir = 0;			// Inicializo el índice de lectura en 0.
	srx->rBuf.iw = 0;			// Inicializo el índice de escritura en 0.
	srx->hdrState = 0;			// Inicializo el estado del decodificador de cabecera en 0.
	srx->timeout = 0;			// Inicializo el contador de timeout en 0.
	srx->nBytes = 0;			// Inicializo en 0 la cantidad de bytes que faltan recibir o procesar del cuerpo de la trama.
	srx->cks = 0;				// Inicializo el checksum recibido/calculado en 0.
	srx->cmd = 0;				// Inicializo el campo comando en 0.
	srx->bodyIndex = 0;			// Inicializo el índice del cuerpo de la trama en 0.

	stx->rBuf.buf = bufTX;		// Guardo la dirección del primer byte del buffer de transmisión.
	stx->rBuf.size = BUFTXSIZE;	// Guardo el tamaño del buffer de transmisión.
	stx->rBuf.ir = 0;			// Inicializo el índice de lectura del buffer de transmisión en 0.
	stx->rBuf.iw = 0;			// Inicializo el índice de escritura del buffer de transmisión en 0.
}

_Bool decodeHeader(_sRX *srx)
{
/*
	En esta función de valida la trama recibida por USART.
    El formato de trama esperado por el protocolo de comunicación:
       _________________________________________________________________
      |     |     |     |     |        |     |     |         |          |
      | 'U' | 'N' | 'E' | 'R' | LENGTH | ':' | CMD | PAYLOAD | CHECKSUM |
      |_____|_____|_____|_____|________|_____|_____|_________|__________|

    Descripción de los campos:
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
   | CMD              : Byte de comando que indica la acción o tipo de mensaje.    |
   |_______________________________________________________________________________|
   | PAYLOAD          : Datos útiles del mensaje (0 a PAYLOADMAX bytes).		   |
   |_______________________________________________________________________________|
   | CHECKSUM         : Byte de verificación calculado con XOR de todos los		   |
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
	
	static uint8_t hdr [] = {'U','N','E','R',0x00,':'};				// Se define un arreglo estático con la cabecera esperada del protocolo.
		
	uint8_t i;														// Variable utilizada como índice auxiliar.
	
	_Bool decodeOK = FALSE;											// Booleano que la función retornará. La inicializó en "FALSE".
	
	i = srx->rBuf.iw;												// Guardo en "i" el valor actual del índice de escritura del buffer.				
	
	while(srx->rBuf.ir != i)										// Mientras haya datos pendientes para leer, se ejecuta el bucle.
	{							
		switch(srx->hdrState)										// Se evalúa el estado actual de la máquina de estados del protocolo.
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
																	// El cuerpo está construído por: CMD, PAYLOAD, CHECKSUM.
			if(srx->nBytes < 2)										// Verifico si la cantidad de bytes es menor a 2 (Entre el CMD Y CHECKSUM suman 2 bytes, siento este valor el mínimo que debe de tener el cuerpo). 
			{
				srx->hdrState = 0;									// Vuelvo al estado inicial para empezar a buscar otra vez desde "U".
				srx->timeout = 0;									// Cancelo el timeout porque la cabecera actual quedó descartada.
				
				break;
			}
			
			srx->payloadLen = srx->nBytes - 2;						// Calculo la longitud del payload sin tener en cuenta los 2 bytes del CMD y el CHECKSUM. 
			
			if(srx->payloadLen > PAYLOADMAX)						// Verifico que el payload no supere el tamaño máximo permitido.
			{
				srx->hdrState = 0;									// Vuelvo al estado inicial para empezar a buscar otra vez desde "U".
				srx->timeout = 0;									// Cancelo el timeout porque la cabecera actual quedó descartada.
				
				break;
			}
			
			srx->hdrState = 5;										// Avanzo al siguiente estado.
			srx->bodyIndex = 0;										// Reinicio el índice del cuerpo.
			srx->cks = 'U' ^ 'N' ^ 'E' ^ 'R' ^ srx->nBytes ^ ':';	// Inicializo el checksum calculando los bytes de la cabecera.
			
			break;			
			
			case 5:													// Sexto estado: busca el sexto byte de la cabecera.
			if(srx->rBuf.buf[srx->rBuf.ir] == hdr[srx->hdrState])	// Comparo el byte actual del buffer con el byte esperado de la cabecera según el estado actual.
			{
				srx->hdrState = 6;									// Si coincide, avanzo al siguiente estado.
			}
			else
			{														// Si el byte no coincide con lo esperado, la cabecera falló.
				if(srx->rBuf.ir != 0)								// Compruebo si el índice de lectura no es 0.
				{
					srx->rBuf.ir--;									// Si no es 0, retrocede una posición el índice de lectura.
				}
				else if(srx->rBuf.ir == 0)							// Compruebo si el índice de lectura es igual a 0.
				{
					srx->rBuf.ir = (BUFRXSIZE-1);					// Si el índice de lectura es 0, como es buffer circular, vuelve al final.
				}
				
				srx->timeout = 0;									// Cancelo el timeout porque la cabecera actual quedó descartada.
				srx->hdrState = 0;									// Vuelvo al estado inicial para empezar a buscar otra vez desde "U".
			}
			break;

			case 6:													// Septimo estado: se valida el checksum.
			
			if(srx->nBytes > 1)										// Si quedan más de 1 byte, significa que todavía no llegó el checksum final.
			{
				if(srx->bodyIndex == 0)								// Si "bodyIndex" es 0, significa que este es el primer byte del cuerpo.
				{
					srx->cmd = srx->rBuf.buf[srx->rBuf.ir];			// Guarda ese byte como comando CMD.
					}
					else if(srx->bodyIndex > 0 )					// Si "bodyIndex" es mayor que 0, entonces ya no estamos en el comando, sino en el payload.
					{
						if((srx->bodyIndex - 1) < PAYLOADMAX)		// Verifico que la posición dentro del payload siga siendo válida.
						{
							srx->payload[srx->bodyIndex - 1] = srx->rBuf.buf[srx->rBuf.ir];	// Guardo el byte actual dentro del arreglo payload.
						}
						else
						{											// Si por alguna razón se pasa del tamaño permitido:
						srx->hdrState = 0;							// Vuelvo al estado inicial para empezar a buscar otra vez desde "U".
						srx->timeout = 0;							// Cancelo el timeout porque la cabecera actual quedó descartada.
						break;
						}
					}
				
					srx->cks ^= srx->rBuf.buf[srx->rBuf.ir];		// Actualizo el checksum calculando los bytes actuales del cuerpo.
					srx->nBytes--;									// Reduce en 1 la cantidad de bytes pendientes.
					srx->bodyIndex++;								// Avanza el índice del cuerpo para el próximo byte.
				
				}
				else if(srx->nBytes == 1)							// Si queda exactamente 1 byte, ese byte ya no es parte del cuerpo sino que debe ser el checksum recibido.
				{
					if(srx->cks == srx->rBuf.buf[srx->rBuf.ir])		// Compara el checksum calculado con el checksum recibido.
					{
						decodeOK = TRUE;							// Si coinciden, la trama fue decodificada correctamente.
					}	
				srx->timeout = 0;									// Si no coincide, cancelo el timeout porque la cabecera actual quedó descartada.
				srx->hdrState = 0;									// Vuelvo al estado inicial para empezar a buscar otra vez desde "U".
				}

			break;
			
			default:												// En caso de que el estado actual no sea uno de los estados anteriores:
			
				srx->hdrState = 0;									// Vuelvo al estado inicial para empezar a buscar otra vez desde "U".
			
			break;
		}
		
		srx->rBuf.ir &= (srx->rBuf.size-1);							// Hago una máscara para asegurar que el índice de lectura quede dentro del tamaño del buffer.
		srx->rBuf.ir++;												// Avanzo al siguiente byte del buffer.
		srx->rBuf.ir &= (srx->rBuf.size-1);							// Vuelvo a aplicar la máscara por si el índice se pasó del final y debe “volver al principio”.
	}
	
return decodeOK;													// Si se encontró una trama válida, retorna "TRUE" y, si no, retorna "FALSE".
}


void USART_SendByte(_sTX *stx)
{
	uint8_t ir;

	// No hay nada para enviar
	if (stx->rBuf.ir == stx->rBuf.iw)
	return;

	// El registro UDR0 todavía no está libre
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
/* En esta función se arma el mensaje con el protocolo, el comando y el payload.
	El formato de trama creado por la función:
	_________________________________________________________________
	|     |     |     |     |        |     |     |         |          |
	| 'U' | 'N' | 'E' | 'R' | LENGTH | ':' | CMD | PAYLOAD | CHECKSUM |
	|_____|_____|_____|_____|________|_____|_____|_________|__________|
*/		

	uint8_t i;																// Variable utilizada como índice auxiliar. 
	uint8_t aux_buf[7];														// Array auxiliar utilizado en la escritura del buffer de transmisión.
	
	stx->length = stx->payloadLen + 2;										// Length de los datos (cmd + payload + cks)
	
	aux_buf[0] = 'U';														// Le agrego al primer byte del buffer auxiliar la 'U'.
	aux_buf[1] = 'N';														// Le agrego al segundo byte del buffer auxiliar la 'N'.
	aux_buf[2] = 'E';														// Le agrego al tercer byte del buffer auxiliar la 'E'.
	aux_buf[3] = 'R';														// Le agrego al cuarto byte del buffer auxiliar la 'R'.
	aux_buf[4] = stx->payloadLen + 2;										// Le agrego al quinto byte del buffer auxiliar el payloadLen.
	aux_buf[5] = ':';														// Le agrego al sexto byte del buffer auxiliar el ':'.
	aux_buf[6] = stx->cmd;													// Le agrego al septimo byte del buffer auxiliar el cmd.

	stx->cks = 'U' ^ 'N' ^ 'E' ^ 'R' ^ stx->length ^ ':' ^ stx->cmd;		// Calculo el checksum del header y cmd.
	
	for (i = 0; i < stx->payloadLen; i++)									// Bucle para recorrer el payload mediante su tamaño.
	{
		stx->cks ^= stx->payload[i];										// Sumo al checksum los bytes del payload.
	}
	
	for (i = 0; i < 7; i++)													// Bucle para recorrer la cabecera de 6 bytes y el cmd ('U' 'N' 'E' 'R' payloadLen ':' cmd).
	{
		stx->rBuf.buf[stx->rBuf.iw] = aux_buf[i];							// Agrego la cabecera y el cmd al buffer de transmisión.
		
		stx->rBuf.iw++;														// Avanzo al siguiente byte del buffer.
		stx->rBuf.iw &= (stx->rBuf.size-1);									// Hago una máscara para asegurar que el índice de escritura quede dentro del tamaño del buffer.
	}
	
	for (i = 0; i < stx->payloadLen; i++)									// Bucle para recorrer el payload mediante su tamaño.
	{
		stx->rBuf.buf[stx->rBuf.iw] = stx->payload[i];						// Agrego los valores del payload al buffer de transmisión.
		
		stx->rBuf.iw++;														// Avanzo al siguiente byte del buffer.
		stx->rBuf.iw &= (stx->rBuf.size-1);									// Hago una máscara para asegurar que el índice de escritura quede dentro del tamaño del buffer.
	}
	
	stx->rBuf.buf[stx->rBuf.iw] = stx->cks;									// Agrego el checksum al buffer de transmisión.
	
	stx->rBuf.iw++;															// Avanzo al siguiente byte del buffer.
	stx->rBuf.iw &= (stx->rBuf.size-1);										// Hago una máscara para asegurar que el índice de escritura quede dentro del tamaño del buffer.
}

