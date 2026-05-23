#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QTimer>
#include <QLabel>
#include <QComboBox>
#include <QPushButton>
#include <QRadioButton>
#include <QSpinBox>
#include <QPlainTextEdit>
#include <QGroupBox>
#include <QLCDNumber>
#include "protocoloUNERQt.h"

// === Comandos firmware -> GUI (TX) ===
#define CMD_ERR_SENSOR   0xA0
#define CMD_DIST_MEAS    0xA1
#define CMD_BOX_CLASSIF  0xA2
#define CMD_BOX_EJECTED  0xA3
#define CMD_STATE        0xA4
#define CMD_COUNTS       0xA5
#define CMD_ACK          0xA6
#define CMD_BOX_DISCARDED 0xA7

// === Comandos GUI -> firmware (RX) ===
#define CMD_GET_STATE    0xB0
#define CMD_GET_COUNTS   0xB1
#define CMD_SET_MODE     0xB2
#define CMD_SET_BOX_MAP  0xB3
#define CMD_SET_THRESH   0xB4
#define CMD_SET_CALIB    0xB5
#define CMD_RESET_COUNTS 0xB6

// === ACK status ===
#define ACK_OK           0x00
#define ACK_BUSY         0x01
#define ACK_INVALID      0x02

// === Estados del transportador (deben coincidir con _eConveyorState del firmware) ===
#define CONV_MEASURE     0
#define CONV_PUSHER1     1
#define CONV_PUSHER2     2
#define CONV_PUSHER3     3

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    void onConnectClicked();
    void onRefreshPorts();
    void onLeerDatos();
    void onPaqueteRecibido(quint8 cmd, QByteArray payload);
    void onBadChecksum();
    void onAplicarConfig();
    void onResetContadores();
    void onPollTimer();

private:
    Ui::MainWindow *ui;

    // Hardware / protocolo
    QSerialPort     *serial;
    protocoloUNERQt *protocolo;
    QTimer          *pollTimer;
    bool             conectado = false;

    // Conexion
    QComboBox   *comboPuertos;
    QPushButton *btnRefresh;
    QPushButton *btnConectar;
    QLabel      *labelConexion;

    // Estado
    QLabel *labelEstadoCinta;
    QLabel *labelDistancia;

    // Contadores
    QLCDNumber *lcdPequena;
    QLCDNumber *lcdMediana;
    QLCDNumber *lcdGrande;
    QLCDNumber *lcdDescartadas;

    // Configuracion
    QGroupBox    *groupConfig;
    QRadioButton *radioNormal;
    QRadioButton *radioEstimado;
    QSpinBox     *spinSmall;
    QSpinBox     *spinMedium;
    QSpinBox     *spinBig;
    QSpinBox     *spinRefDist;
    QLabel       *labelLockWarning;
    QPushButton  *btnAplicar;

    // Log
    QPlainTextEdit *logEdit;

    // Helpers
    void buildUI();
    void cargarPuertos();
    bool enviarComando(quint8 cmd, const QByteArray &payload = QByteArray());
    void actualizarEstadoCinta(quint8 state);
    void actualizarContadores(const QByteArray &payload);
    void setConfigLocked(bool locked);
    void log(const QString &msg);
    QString ahora() const;
};

#endif // MAINWINDOW_H
