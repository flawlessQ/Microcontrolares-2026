#ifndef PROTOCOLOUNERQT_H
#define PROTOCOLOUNERQT_H

#include <QObject>
#include <QByteArray>
#include <QElapsedTimer>

class protocoloUNERQt: public QObject {
    Q_OBJECT

public:
    explicit protocoloUNERQt (QObject *parent = nullptr);
    static QByteArray buildFrame(quint8 cmd, const QByteArray &payload = QByteArray());
    void decodeByte(quint8 byte);
    void reset();

signals:
    void packageReceived(quint8 cmd, QByteArray payload);
    void badChecksum();

private:
    enum State{
        WaitU,
        WaitN,
        WaitE,
        WaitR,
        WaitLength,
        WaitToken,
        WaitCmd,
        WaitPayload,
        WaitChecksum
    };

    State state;
    quint8 length;
    quint8 cmd;
    quint8 payloadLen;
    quint8 payloadIdx;
    quint8 checksum;

    QByteArray payload;
    QElapsedTimer frameTimer;

};

#endif // PROTOCOLOUNERQT_H
