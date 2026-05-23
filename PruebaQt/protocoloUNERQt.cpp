#include "protocoloUNERQt.h"

static constexpr quint8 TOKEN = 0x3A;
static constexpr int FRAME_TIMEOUT_MS = 40;

protocoloUNERQt::protocoloUNERQt(QObject *parent): QObject(parent)
{
    reset();
    frameTimer.start();
}

void protocoloUNERQt::reset()
{
    state = WaitU;
    length =0;
    cmd = 0;
    payloadLen = 0;
    payloadIdx = 0;
    checksum = 0;
    payload.clear();
}

QByteArray protocoloUNERQt::buildFrame(quint8 cmd, const QByteArray &payload)
{
    QByteArray frame;
    if(payload.size() > 16)
    {
        return frame;
    }
    quint8 xorCalc = 0;

    auto addByte = [&](quint8 byte)
    {
        frame.append(static_cast<char>(byte));
        xorCalc ^=byte;
    };
    addByte('U');
    addByte('N');
    addByte('E');
    addByte('R');

    quint8 length = static_cast<quint8>(1 + payload.size()+1);
    addByte(length);

    addByte(TOKEN);

    addByte(cmd);

    for(qsizetype i = 0; i < payload.size(); i++){
        char c = payload.at(i);
        addByte(static_cast<quint8>(static_cast<unsigned char>(c)));
    }
    frame.append(static_cast<char>(xorCalc));

    return frame;

}

void protocoloUNERQt::decodeByte(quint8 byte){
    if(state != WaitU){
        if(frameTimer.elapsed()>FRAME_TIMEOUT_MS){
            reset();
        }
    }

    switch(state){
    case WaitU:
        if(byte == 'U'){
            checksum ^= byte;
            frameTimer.restart();
            state = WaitN;
        }
        break;
    case WaitN:
        if(byte == 'N'){
            checksum ^=byte;
            state = WaitE;
        }else{
            reset();
        }
        break;
    case WaitE:
        if(byte == 'E'){
            checksum ^= byte;
            state = WaitR;
        }else{
            reset();
        }
        break;
    case WaitR:
        if(byte == 'R'){
            checksum ^= byte;
            state = WaitLength;
        }else{
            reset();
        }
        break;
    case WaitLength:
        length = byte;
        checksum ^= byte;
        if(length >= 2){
            payloadLen = length -2 ;
            payloadIdx = 0;
            payload.clear();
            state = WaitToken;
        }else{
            reset();
        }
        break;

    case WaitToken:
        if(byte == TOKEN){
            checksum ^= byte;
            state = WaitCmd;
        }else{
            reset();
        }
        break;

    case WaitCmd:
        cmd = byte;
        checksum ^= byte;
        payloadIdx = 0;

        if(payloadLen > 0 ){
            state = WaitPayload;
        }else{
            state = WaitChecksum;
        }
        break;

    case WaitPayload:
        payload.append(static_cast<char>(byte));
        checksum ^= byte;
        payloadIdx++;
        if(payloadIdx >= payloadLen){
            state = WaitChecksum;
        }
        break;

    case WaitChecksum:
        if(checksum == byte){
            emit packageReceived(cmd, payload);
        }else{
            emit badChecksum();
        }
        reset();
        break;
    }
}