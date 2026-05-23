#ifndef HCSR04_H_
#define HCSR04_H_

#include <stdint.h>

/*
    Bits del registro general del driver.
*/
#define RDY0    0   // Medición lista.
#define ERR0    1   // Error o timeout.
#define ODS0    2   // Objeto detectado.

/*
    Estados internos del driver HC-SR04.
*/
typedef enum
{
    HCSR04_IDLE = 0,          // Sensor en reposo.
    HCSR04_TRIG,              // Pulso TRIG en proceso.
    HCSR04_WAIT_ECHO_HIGH,    // Esperando que ECHO suba.
    HCSR04_WAIT_ECHO_LOW,     // Midiendo ECHO en alto.
    HCSR04_DONE,              // Medición finalizada correctamente.
    HCSR04_TIMEOUT            // Error por timeout.

} _eHCSR04_STATE;

/*
    Interfaz de hardware.

    Estas funciones se implementan desde el main.
    El driver no accede directamente a registros del microcontrolador.
*/
typedef struct
{
    void (*trig_write)(uint8_t level);      // Escribe el pin TRIG.
    uint8_t (*echo_read)(void);             // Lee el pin ECHO.

    void (*timer_reset)(void);              // Reinicia el timer usado para medir.
    void (*timer_start)(void);              // Arranca el timer.
    void (*timer_stop)(void);               // Detiene el timer.
    uint16_t (*timer_get_ticks)(void);      // Devuelve los ticks actuales del timer.

} _sHCSR04_IO;

/*
    Estructura principal del driver HC-SR04.
*/
typedef struct
{
    _sHCSR04_IO io;              // Funciones externas de hardware.

    _eHCSR04_STATE state;        // Estado actual del sensor.

    uint16_t counter_us;         // Se conserva por compatibilidad con la versión anterior.
    uint8_t period_ms;           // Contador para respetar el período entre mediciones.

    uint16_t echo_time_us;       // Tiempo medido del pulso ECHO en microsegundos.
    uint16_t distance_cm;        // Distancia calculada en centímetros.

    uint16_t max_distance_cm;    // Distancia máxima válida configurada desde el main.

    uint8_t GRHCSR04;            // Registro de banderas del driver.

} _sHCSR04;

/*
    Inicializa el driver HC-SR04.
*/
void HCSR04_Init(_sHCSR04 *sensor, _sHCSR04_IO *io);

/*
    Inicia una medición no bloqueante.
    Se conserva por compatibilidad con la versión anterior.
*/
void HCSR04_Start(_sHCSR04 *sensor);

/*
    Actualiza la máquina de estados no bloqueante.
    Se conserva por compatibilidad con la versión anterior.
*/
void HCSR04_Update(_sHCSR04 *sensor);

/*
    Función asociada al conteo de 1 us.
    En la nueva lógica con timer por hardware puede no usarse.
*/
void HCSR04_On1us(_sHCSR04 *sensor);

/*
    Función asociada al conteo de 1 ms.
    Se usa para respetar el período mínimo entre mediciones.
*/
void HCSR04_On1ms(_sHCSR04 *sensor);

/*
    Realiza una medición usando el timer del microcontrolador
    mediante punteros a función.
*/
void HCSR04_Measure(_sHCSR04 *sensor);

/*
    Devuelve 1 si hay una medición lista.
*/
uint8_t HCSR04_IsReady(_sHCSR04 *sensor);

/*
    Devuelve 1 si ocurrió un error o timeout.
*/
uint8_t HCSR04_HasError(_sHCSR04 *sensor);

/*
    Devuelve la última distancia calculada en centímetros.
*/
uint16_t HCSR04_GetDistanceCm(_sHCSR04 *sensor);

/*
    Devuelve 1 si la última medición corresponde a un objeto válido.
*/
uint8_t HCSR04_IsObjectDetected(_sHCSR04 *sensor);

/*
    Configura la distancia máxima válida para detectar objeto.
    Este valor se define desde el main porque depende del montaje físico.
*/
void HCSR04_SetMaxDistanceCm(_sHCSR04 *sensor, uint16_t distance_cm);

#endif