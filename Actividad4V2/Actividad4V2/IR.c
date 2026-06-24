/*
 * IR.c
 *
 * Created: 23/5/2026 17:23:13
 *  Author: juani
 */
#include "IR.h"

// Debounce por conteo de muestras: la señal debe estar LOW durante
// IR_CONFIRM_SAMPLES ms consecutivos para registrar un flanco.
// Evita disparos por ruido o reflexiones breves.
#define IR_CONFIRM_SAMPLES  5    // ms consecutivos en LOW para confirmar
#define IR_LOCKOUT_MS       20   // ms de bloqueo tras deteccion confirmada

static uint8_t actualState[4];
static uint8_t contDB[4];    // contador de lockout post-deteccion
static uint8_t contConf[4];  // contador de muestras consecutivas LOW
static uint8_t wasHigh[4];   // 1 si la ultima vez que no estaba en lockout era HIGH
static uint8_t flag[4];

static uint8_t ReadPin(uint8_t i) {
    switch(i) {
        case 0: return (PIND & (1 << PD5)) ? 1 : 0;
        case 1: return (PIND & (1 << PD2)) ? 1 : 0;
        case 2: return (PIND & (1 << PD3)) ? 1 : 0;
        default:return (PIND & (1 << PD4)) ? 1 : 0;
    }
}

void IR_Init() {
    DDRD &= ~(1 << PD5);
    DDRD &= ~(1 << PD2);
    DDRD &= ~(1 << PD3);
    DDRD &= ~(1 << PD4);

    for(uint8_t i = 0; i < 4; i++){
        actualState[i] = 1;
        contDB[i]      = 0;
        contConf[i]    = 0;
        wasHigh[i]     = 1;   // reposo: señal HIGH (pull-up, sin objeto)
        flag[i]        = 0;
    }
}

void IR_Update() {
    for(uint8_t i = 0; i < 4; i++){

        if(contDB[i] > 0){
            contDB[i]--;
            continue;
        }

        actualState[i] = ReadPin(i);

        if(actualState[i] == 0) {       // LOW: objeto presente
            contConf[i]++;
            if(contConf[i] >= IR_CONFIRM_SAMPLES && wasHigh[i]) {
                flag[i]     = 1;
                contDB[i]   = IR_LOCKOUT_MS;
                contConf[i] = 0;
                wasHigh[i]  = 0;
            }
        } else {                         // HIGH: sin objeto (o ruido breve)
            contConf[i] = 0;
            wasHigh[i]  = 1;
        }
    }
}

uint8_t IR_FallingEdge(_eIRID id) {
    uint8_t f = flag[id];
    flag[id]  = 0;
    return f;
}
