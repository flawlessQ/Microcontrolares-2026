/*
 * COMUNICATION.c
 *
 * Created: 23/5/2026 17:25:16
 *  Author: juani
 */

#define F_CPU   16000000UL
#define BAUD    115200UL
#define UBRR_VAL ((F_CPU / (16UL * BAUD)) - 1)

#include "COMUNICATION.h"

static uint8_t rx_buf[RX_BUFFER_SIZE];
static uint8_t rx_head;
static uint8_t rx_tail;

void COM_Init(){
    rx_head = 0;
    rx_tail = 0;

    // Baud rate
    UBRR0H = (uint8_t)(UBRR_VAL >> 8);
    UBRR0L = (uint8_t)(UBRR_VAL & 0xFF);

    // 8N1, habilitar TX, RX e interrupción RX
    UCSR0B = (1 << RXEN0) | (1 << TXEN0) | (1 << RXCIE0);
    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
}

// ISR recepción — guarda byte en buffer circular
ISR(USART_RX_vect){
    uint8_t dato = UDR0;
    uint8_t next = (rx_head + 1) % RX_BUFFER_SIZE;
    if(next != rx_tail){
        rx_buf[rx_head] = dato;
        rx_head = next;
    }
}

static void COM_SendByte(uint8_t b){
    while(!(UCSR0A & (1 << UDRE0)));
    UDR0 = b;
}

// Trama UNER: U N E R | LENGTH | ':' | CMD | PAYLOAD... | CHECKSUM
// LENGTH = CMD(1) + payload(n) + CHECKSUM(1) = n+2  (igual que Qt buildFrame)
// CHECKSUM = XOR acumulado desde 'U' hasta ultimo byte de payload
void COM_SendFrame(uint8_t cmd, uint8_t *payload, uint8_t len){
    uint8_t i;
    uint8_t chk = 0;
    uint8_t length = len + 2;

    chk ^= 'U'; COM_SendByte('U');
    chk ^= 'N'; COM_SendByte('N');
    chk ^= 'E'; COM_SendByte('E');
    chk ^= 'R'; COM_SendByte('R');
    chk ^= length; COM_SendByte(length);
    chk ^= ':'; COM_SendByte(':');
    chk ^= cmd; COM_SendByte(cmd);

    for(i = 0; i < len; i++){
        chk ^= payload[i];
        COM_SendByte(payload[i]);
    }

    COM_SendByte(chk);
}

static uint8_t COM_RxAvailable(){
    return (rx_head != rx_tail);
}

static uint8_t COM_RxRead(){
    uint8_t dato;
    if(rx_head == rx_tail) return 0;
    dato = rx_buf[rx_tail];
    rx_tail = (rx_tail + 1) % RX_BUFFER_SIZE;
    return dato;
}

// ── Parser UNER ───────────────────────────────────────────────────────────────
// Trama: U N E R | LENGTH | ':' | CMD | PAYLOAD... | CHECKSUM(XOR)

#define PS_IDLE     0
#define PS_U        1
#define PS_N        2
#define PS_E        3
#define PS_R        4
#define PS_LEN      5
#define PS_COLON    6
#define PS_PAYLOAD  7
#define PS_CHKSUM   8

static uint8_t ps_state = PS_IDLE;
static uint8_t ps_len;
static uint8_t ps_cmd;
static uint8_t ps_payload[COM_PAYLOAD_SIZE];
static uint8_t ps_idx;
static uint8_t ps_chk;

// Frame listo para ser leido por el main
static uint8_t frame_ready = 0;
static uint8_t frame_cmd;
static uint8_t frame_payload[COM_PAYLOAD_SIZE];
static uint8_t frame_len;

// COM_Update: llamar en cada iteracion del loop principal.
// Procesa todos los bytes disponibles en el buffer RX y reconstruye tramas UNER.
// Cuando la trama es valida (checksum OK), la almacena y activa el flag frame_ready.
// COM_Update: checksum acumulado desde 'U' hasta ultimo payload, igual que Qt.
// LENGTH de Qt: CMD(1) + payload(n) + CHECKSUM(1) = n+2 → payload bytes = LENGTH-2
void COM_Update(){
    while(COM_RxAvailable()){
        uint8_t b = COM_RxRead();
        switch(ps_state){
            case PS_IDLE:
                if(b=='U'){ ps_chk='U'; ps_state=PS_U; }
                break;
            case PS_U:
                if(b=='N'){ ps_chk^='N'; ps_state=PS_N; } else ps_state=PS_IDLE;
                break;
            case PS_N:
                if(b=='E'){ ps_chk^='E'; ps_state=PS_E; } else ps_state=PS_IDLE;
                break;
            case PS_E:
                if(b=='R'){ ps_chk^='R'; ps_state=PS_R; } else ps_state=PS_IDLE;
                break;
            case PS_R:
                ps_len=b; ps_chk^=b; ps_state=PS_LEN;
                break;
            case PS_LEN:
                if(b==':'){ ps_chk^=b; ps_state=PS_COLON; } else ps_state=PS_IDLE;
                break;
            case PS_COLON:
                ps_cmd=b; ps_chk^=b; ps_idx=0;
                ps_state = (ps_len > 2) ? PS_PAYLOAD : PS_CHKSUM;
                break;
            case PS_PAYLOAD:
                if(ps_idx < COM_PAYLOAD_SIZE) ps_payload[ps_idx] = b;
                ps_chk ^= b;
                if(++ps_idx >= ps_len - 2) ps_state = PS_CHKSUM;
                break;
            case PS_CHKSUM:{
                if(b == ps_chk){
                    frame_cmd = ps_cmd;
                    frame_len = ps_idx;
                    uint8_t i;
                    for(i = 0; i < frame_len; i++) frame_payload[i] = ps_payload[i];
                    frame_ready = 1;
                }
                ps_state = PS_IDLE;
                break;
            }
            default: ps_state = PS_IDLE; break;
        }
    }
}

// COM_FrameAvailable: retorna 1 si hay una trama valida lista para leer.
uint8_t COM_FrameAvailable(){
    return frame_ready;
}

// COM_GetFrame: copia la trama disponible y baja el flag.
// cmd     -> byte de comando
// payload -> bytes de payload
// len     -> cantidad de bytes de payload
void COM_GetFrame(uint8_t *cmd, uint8_t *payload, uint8_t *len){
    uint8_t i;
    *cmd = frame_cmd;
    *len = frame_len;
    for(i = 0; i < frame_len; i++) payload[i] = frame_payload[i];
    frame_ready = 0;
}
