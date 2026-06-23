#ifndef HCSR04_H_
#define HCSR04_H_

#include <stdint.h>

#define HCSR04_MAX_SAMPLES 10	// Número máximo de mediciones que realizará el sensor.

#define HCSR04_CM_DIVISOR 58	/* La velocidad del sonido en el aire es: 343 (m/s) --> 34300 (cm/s).
								   Si 1 (s) = 1000000 (us), entonces: 34300 (cm/s) / 1000000 (us) = 0.0343 (cm/us)
								   Como tiempo medido por el HC-SR04 es ida y vuelta:
								   distancia = (tiempo_us * 0.0343 (cm/us)) / 2
								   distancia = tiempo_us * 0.01715 (cm/us)
								   distancia = tiempo_us / 58 (cm/us) --> Por esta razón se utiliza 58 como divisor. */
/*
	En este typedef enum se establecen los estados de error
	durante la medición del sensor HC-SR04.
*/
typedef enum
{
	HCSR04_OK = 0,						//	Estado sin errores.
	HCSR04_ERROR_NULL,					//	Estado de error donde el puntero al sensor es nulo.
	HCSR04_ERROR_TIMEOUT,				//	Estado de error donde no respondió a tiempo.
	HCSR04_ERROR_OUT_OF_RANGE,			//	Estado de error donde la distancia está fuera de rango.
	HCSR04_ERROR_NOT_ENOUGH_SAMPLES		//	Estado de error donde no hay suficientes muestras.

} _sHCSR04_status_t;

typedef void (*HCSR04_TriggerFn_t)(void *context);									// Tipo de funcion que genera el pulso Trigger del sensor.
																			// La implementacion depende del hardware y debe realizarse en el main.
	
typedef uint8_t (*HCSR04_ReadEchoUsFn_t)(void *context, uint32_t *echo_us);	// Tipo de funcion que mide el tiempo del pulso Echo en microsegundos.
																			// Debe devolver 1 si la medicion fue correcta o 0 si hubo error/timeout.
																			// La medicion se entrega mediante el puntero echo_us.

typedef struct
{

	HCSR04_TriggerFn_t trigger;				// Funcion externa encargada de generar el pulso Trigger.
	
	HCSR04_ReadEchoUsFn_t read_echo_us;		// Funcion externa encargada de medir el ancho del pulso Echo en us.
	
	void *context;							// Puntero generico a la informacion de hardware definida en el main.
																// La libreria no interpreta este dato, solo lo pasa a las funciones externas.

	uint16_t samples[HCSR04_MAX_SAMPLES];	// Arreglo donde se guardan las ultimas mediciones realizadas en centimetros.
	
	uint8_t sample_count;					// Cantidad de muestras validas almacenadas actualmente.
	
	uint8_t sample_index;					// Indice donde se guardara la proxima muestra dentro del buffer circular.
	
	uint16_t min_distance_cm;				// Distancia minima valida del sensor, expresada en centimetros.
	
	uint16_t max_distance_cm;				// Distancia maxima valida del sensor, expresada en centimetros.
	
	uint8_t tolerance_cm;					// Tolerancia permitida para considerar mediciones similares como un mismo grupo.

} _sHCSR04_t;

/*
    HCSR04_Init

    Inicializa la estructura principal del sensor HC-SR04.

    Esta funcion debe llamarse una sola vez antes de realizar mediciones.
    Su objetivo es guardar dentro de la estructura del sensor las funciones
    externas de hardware y preparar los valores internos de trabajo.

    Parametros:
        sensor:
            Puntero a la estructura HCSR04_t que representa al sensor.
            Ejemplo:
                HCSR04_t sensor_ultrasonico;
                HCSR04_Init(&sensor_ultrasonico, ...);

        trigger:
            Puntero a la funcion encargada de generar el pulso Trigger.
            Esta funcion debe estar implementada en el main, porque depende
            del pin y del microcontrolador utilizado.

        read_echo_us:
            Puntero a la funcion encargada de medir el tiempo del pulso Echo
            en microsegundos. Tambien debe estar implementada en el main.

        context:
            Puntero generico a informacion de hardware.
            Puede apuntar a una estructura creada en el main con datos como
            puerto, pin Trigger, pin Echo, timer, etc.
            La libreria no interpreta este puntero, solamente lo guarda y lo
            pasa a las funciones trigger y read_echo_us.

    Retorno:
        HCSR04_OK:
            Inicializacion correcta.

        HCSR04_ERROR_NULL:
            sensor, trigger o read_echo_us son punteros nulos.
*/
_sHCSR04_status_t HCSR04_Init(_sHCSR04_t *sensor, HCSR04_TriggerFn_t trigger, HCSR04_ReadEchoUsFn_t read_echo_us, void *context);


/*
    HCSR04_MeasureCm

    Realiza una nueva medicion de distancia con el sensor HC-SR04.

    Cada vez que se llama a esta funcion, se hace una medicion nueva.
    La funcion genera el pulso Trigger usando la funcion externa recibida,
    mide el tiempo de Echo mediante la funcion externa correspondiente,
    convierte ese tiempo a centimetros y guarda la muestra en el buffer.

    Parametros:
        sensor:
            Puntero a la estructura HCSR04_t previamente inicializada.

        distance_cm:
            Puntero a una variable donde se guardara la distancia medida.
            La distancia se entrega en centimetros.

            Ejemplo:
                uint16_t distancia;
                HCSR04_MeasureCm(&sensor_ultrasonico, &distancia);

    Retorno:
        HCSR04_OK:
            Medicion realizada correctamente.

        HCSR04_ERROR_NULL:
            sensor o distance_cm son punteros nulos.

        HCSR04_ERROR_TIMEOUT:
            No se pudo medir el pulso Echo porque hubo timeout.

        HCSR04_ERROR_OUT_OF_RANGE:
            La distancia calculada esta fuera del rango valido configurado.
*/
_sHCSR04_status_t HCSR04_MeasureCm(_sHCSR04_t *sensor, uint16_t *distance_cm);


/*
    HCSR04_GetStableCm

    Obtiene una distancia estable usando las mediciones almacenadas.

    Esta funcion no dispara una nueva medicion.
    Solamente analiza las muestras que ya fueron guardadas por llamadas
    anteriores a HCSR04_MeasureCm.

    La idea es buscar el valor o grupo de valores mas repetido.
    Esto sirve para evitar errores aislados del sensor.

    Ejemplo:
        muestras guardadas:
            8, 8, 12, 8, 20

        required_repetitions:
            3

        resultado:
            8 cm

    Porque el valor 8 aparece 3 veces.

    Parametros:
        sensor:
            Puntero a la estructura HCSR04_t previamente inicializada.

        required_repetitions:
            Cantidad minima de mediciones similares necesarias para aceptar
            una distancia como estable.

            Ejemplo:
                Si required_repetitions = 3, entonces al menos 3 muestras
                deben coincidir o estar dentro de la tolerancia configurada.

        stable_distance_cm:
            Puntero a una variable donde se guardara la distancia estable
            obtenida a partir del analisis de las muestras.

            Ejemplo:
                uint16_t distancia_estable;
                HCSR04_GetStableCm(&sensor_ultrasonico, 3, &distancia_estable);

    Retorno:
        HCSR04_OK:
            Se encontro una medicion estable.

        HCSR04_ERROR_NULL:
            sensor o stable_distance_cm son punteros nulos.

        HCSR04_ERROR_NOT_ENOUGH_SAMPLES:
            No hay suficientes muestras similares para aceptar una distancia
            como estable.
*/
_sHCSR04_status_t HCSR04_GetStableCm(_sHCSR04_t *sensor, uint8_t required_repetitions, uint16_t *stable_distance_cm);


/*
    HCSR04_ClearSamples

    Borra todas las muestras almacenadas en el sensor.

    Esta funcion sirve cuando se quiere empezar una nueva tanda de mediciones
    desde cero, descartando las mediciones anteriores.

    Parametros:
        sensor:
            Puntero a la estructura HCSR04_t cuyas muestras se quieren borrar.

            Ejemplo:
                HCSR04_ClearSamples(&sensor_ultrasonico);

    Retorno:
        Esta funcion no devuelve ningun valor.
*/
void HCSR04_ClearSamples(_sHCSR04_t *sensor);

#endif