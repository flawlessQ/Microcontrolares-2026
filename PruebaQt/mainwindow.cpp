#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFrame>
#include <QPixmap>
#include <QDateTime>

// ─────────────────────────────────────────────────────────────────────────────
// Stylesheet global
// ─────────────────────────────────────────────────────────────────────────────
static const char *APP_STYLE = R"(
QMainWindow, QWidget {
    background-color: #F0F4F8;
    font-family: "Segoe UI", sans-serif;
    font-size: 12px;
}
QGroupBox {
    background-color: #FFFFFF;
    border: 1px solid #CBD5E0;
    border-radius: 8px;
    margin-top: 12px;
    padding: 8px 6px 6px 6px;
    font-weight: bold;
    color: #2D3748;
}
QGroupBox::title {
    subcontrol-origin: margin;
    left: 10px;
    padding: 0 5px;
    color: #1E5FA3;
    font-size: 12px;
}
QPushButton {
    background-color: #1E5FA3;
    color: white;
    border: none;
    border-radius: 5px;
    padding: 6px 14px;
    font-weight: bold;
    min-height: 26px;
}
QPushButton:hover   { background-color: #2B76C7; }
QPushButton:pressed { background-color: #164D87; }
QPushButton:disabled{ background-color: #A0AEC0; }
QComboBox {
    border: 1px solid #CBD5E0;
    border-radius: 5px;
    padding: 4px 8px;
    background: white;
    color: #2D3748;
    min-height: 26px;
}
QComboBox::drop-down { border: none; width: 20px; }
QComboBox QAbstractItemView {
    background: white;
    color: #2D3748;
    selection-background-color: #EBF8FF;
}
QSpinBox {
    border: 1px solid #CBD5E0;
    border-radius: 4px;
    padding: 2px 4px;
    background: white;
    color: #2D3748;
    min-height: 24px;
}
QPlainTextEdit {
    background-color: #1A202C;
    color: #68D391;
    font-family: "Consolas", "Courier New", monospace;
    font-size: 11px;
    border-radius: 6px;
    border: none;
    padding: 6px;
}
QLabel       { color: #2D3748; background: transparent; }
QRadioButton { font-weight: normal; color: #2D3748; spacing: 6px; }
QRadioButton:checked { color: #1E5FA3; font-weight: bold; }
QRadioButton::indicator {
    width: 14px; height: 14px;
    border: 2px solid #CBD5E0;
    border-radius: 7px;
    background: white;
}
QRadioButton::indicator:checked {
    background-color: #1E5FA3;
    border: 2px solid #1E5FA3;
}
QLCDNumber   { background-color: #1A202C; border-radius: 4px; }
)";

// ─────────────────────────────────────────────────────────────────────────────
// Constructor / Destructor
// ─────────────────────────────────────────────────────────────────────────────
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    setStyleSheet(APP_STYLE);
    setMinimumSize(860, 680);

    serial    = new QSerialPort(this);
    protocolo = new protocoloUNERQt(this);
    pollTimer = new QTimer(this);
    pollTimer->setInterval(500);

    buildUI();
    cargarPuertos();

    connect(serial,    &QSerialPort::readyRead,           this, &MainWindow::onLeerDatos);
    connect(protocolo, &protocoloUNERQt::packageReceived, this, &MainWindow::onPaqueteRecibido);
    connect(protocolo, &protocoloUNERQt::badChecksum,     this, &MainWindow::onBadChecksum);
    connect(pollTimer, &QTimer::timeout,                  this, &MainWindow::onPollTimer);
}

MainWindow::~MainWindow()
{
    delete ui;
}

// ─────────────────────────────────────────────────────────────────────────────
// buildUI — construye toda la interfaz programaticamente
// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::buildUI()
{
    QWidget *central = new QWidget(this);
    setCentralWidget(central);

    QVBoxLayout *mainLayout = new QVBoxLayout(central);
    mainLayout->setSpacing(10);
    mainLayout->setContentsMargins(14, 10, 14, 10);

    // ── Cabecera ─────────────────────────────────────────────────────────────
    QHBoxLayout *headerLayout = new QHBoxLayout();
    headerLayout->setSpacing(14);

    QLabel *logoLabel = new QLabel();
    QPixmap logo(":/resources/logofcal.png");
    if (!logo.isNull())
        logoLabel->setPixmap(logo.scaledToHeight(60, Qt::SmoothTransformation));
    else
        logoLabel->setText("UNER FCAL");
    logoLabel->setStyleSheet("background: transparent;");
    headerLayout->addWidget(logoLabel);

    QFrame *vline = new QFrame();
    vline->setFrameShape(QFrame::VLine);
    vline->setStyleSheet("color: #CBD5E0; background: #CBD5E0; max-width: 1px;");
    headerLayout->addWidget(vline);

    QVBoxLayout *titleLayout = new QVBoxLayout();
    titleLayout->setSpacing(2);
    QLabel *lblTitulo = new QLabel("Sistema de Clasificación de Cajas");
    lblTitulo->setStyleSheet("font-size: 18px; font-weight: bold; color: #1E5FA3; background: transparent;");
    QLabel *lblSubtitulo = new QLabel("Microcontroladores  ·  Ingeniería en Mecatrónica  ·  UNER  ·  FCAL");
    lblSubtitulo->setStyleSheet("font-size: 10px; color: #718096; background: transparent;");
    titleLayout->addWidget(lblTitulo);
    titleLayout->addWidget(lblSubtitulo);
    headerLayout->addLayout(titleLayout);
    headerLayout->addStretch();
    mainLayout->addLayout(headerLayout);

    QFrame *hline = new QFrame();
    hline->setFrameShape(QFrame::HLine);
    hline->setStyleSheet("color: #CBD5E0; background: #CBD5E0; max-height: 1px;");
    mainLayout->addWidget(hline);

    // ── Conexion ─────────────────────────────────────────────────────────────
    QGroupBox *grpConn = new QGroupBox("Conexión Serial");
    QHBoxLayout *connLayout = new QHBoxLayout(grpConn);
    connLayout->setSpacing(8);

    connLayout->addWidget(new QLabel("Puerto:"));
    comboPuertos = new QComboBox();
    comboPuertos->setMinimumWidth(110);
    connLayout->addWidget(comboPuertos);

    btnRefresh = new QPushButton("↻");
    btnRefresh->setFixedWidth(34);
    btnRefresh->setToolTip("Actualizar lista de puertos");
    connLayout->addWidget(btnRefresh);

    btnConectar = new QPushButton("CONECTAR");
    btnConectar->setMinimumWidth(110);
    connLayout->addWidget(btnConectar);

    labelConexion = new QLabel("● Desconectado");
    labelConexion->setStyleSheet("color: #E53E3E; font-weight: bold; background: transparent;");
    connLayout->addWidget(labelConexion);
    connLayout->addStretch();

    connect(btnConectar, &QPushButton::clicked, this, &MainWindow::onConnectClicked);
    connect(btnRefresh,  &QPushButton::clicked, this, &MainWindow::onRefreshPorts);
    mainLayout->addWidget(grpConn);

    // ── Fila central ─────────────────────────────────────────────────────────
    QHBoxLayout *midLayout = new QHBoxLayout();
    midLayout->setSpacing(10);

    // --- Estado del transportador ---
    QGroupBox *grpEstado = new QGroupBox("Estado del Transportador");
    QVBoxLayout *estadoLayout = new QVBoxLayout(grpEstado);
    estadoLayout->setSpacing(8);

    labelEstadoCinta = new QLabel("DESCONECTADO");
    labelEstadoCinta->setAlignment(Qt::AlignCenter);
    labelEstadoCinta->setStyleSheet(
        "background-color: #718096; color: white; font-size: 15px; font-weight: bold;"
        "border-radius: 8px; padding: 16px 10px;");
    labelEstadoCinta->setMinimumHeight(64);
    estadoLayout->addWidget(labelEstadoCinta);

    labelDistancia = new QLabel("Altura: -- cm");
    labelDistancia->setAlignment(Qt::AlignCenter);
    labelDistancia->setStyleSheet("font-size: 13px; color: #4A5568; background: transparent;");
    estadoLayout->addWidget(labelDistancia);
    estadoLayout->addStretch();

    grpEstado->setMinimumWidth(190);
    midLayout->addWidget(grpEstado, 2);

    // --- Contadores ---
    QGroupBox *grpCnt = new QGroupBox("Contadores de Cajas");
    QGridLayout *cntLayout = new QGridLayout(grpCnt);
    cntLayout->setSpacing(6);
    cntLayout->setColumnStretch(1, 1);

    auto makeCounter = [&](const QString &nombre, const QString &color,
                           QLCDNumber *&lcd, int row) {
        QLabel *lbl = new QLabel(nombre);
        lbl->setStyleSheet(QString("color: %1; font-weight: bold; background: transparent;").arg(color));
        lcd = new QLCDNumber(4);
        lcd->setSegmentStyle(QLCDNumber::Filled);
        lcd->setMinimumHeight(46);
        lcd->display(0);
        cntLayout->addWidget(lbl, row, 0);
        cntLayout->addWidget(lcd, row, 1);
    };
    makeCounter("Pequeña",     "#2B6CB0", lcdPequena,     0);
    makeCounter("Mediana",     "#276749", lcdMediana,     1);
    makeCounter("Grande",      "#C05621", lcdGrande,      2);
    makeCounter("Descartadas", "#742A2A", lcdDescartadas, 3);

    QPushButton *btnReset = new QPushButton("RESET CONTADORES");
    btnReset->setStyleSheet("background-color: #718096;");
    btnReset->setToolTip("Reinicia los contadores en el firmware");
    cntLayout->addWidget(btnReset, 4, 0, 1, 2);
    connect(btnReset, &QPushButton::clicked, this, &MainWindow::onResetContadores);

    grpCnt->setMinimumWidth(220);
    midLayout->addWidget(grpCnt, 2);

    // --- Configuracion ---
    groupConfig = new QGroupBox("Configuración");
    QVBoxLayout *cfgLayout = new QVBoxLayout(groupConfig);
    cfgLayout->setSpacing(4);

    QLabel *lblModo = new QLabel("Modo de operación:");
    lblModo->setStyleSheet("font-weight: bold; background: transparent;");
    cfgLayout->addWidget(lblModo);

    QHBoxLayout *modoRow = new QHBoxLayout();
    radioNormal   = new QRadioButton("Normal (IR)");
    radioEstimado = new QRadioButton("Estimado (Velocidad)");
    radioNormal->setChecked(true);
    modoRow->addWidget(radioNormal);
    modoRow->addWidget(radioEstimado);
    cfgLayout->addLayout(modoRow);

    QFrame *cfgLine = new QFrame();
    cfgLine->setFrameShape(QFrame::HLine);
    cfgLine->setStyleSheet("color: #E2E8F0;");
    cfgLayout->addWidget(cfgLine);

    QLabel *lblUmbrales = new QLabel("Alturas mínimas de detección (cm):");
    lblUmbrales->setStyleSheet("font-weight: bold; background: transparent;");
    cfgLayout->addWidget(lblUmbrales);

    auto addThreshRow = [&](const QString &lbl, QSpinBox *&spin, int val) {
        spin = new QSpinBox();
        spin->setRange(1, 30);
        spin->setSuffix(" cm");
        spin->setValue(val);
        spin->setFixedSize(100, 26);
        QHBoxLayout *row = new QHBoxLayout();
        row->setSpacing(8);
        row->setContentsMargins(0, 0, 0, 0);
        QLabel *l = new QLabel(lbl);
        l->setFixedWidth(62);
        row->addWidget(l);
        row->addWidget(spin);
        row->addStretch();
        cfgLayout->addLayout(row);
    };
    addThreshRow("Pequeña:",  spinSmall,  6);
    addThreshRow("Mediana:",  spinMedium, 8);
    addThreshRow("Grande:",   spinBig,    10);

    QHBoxLayout *refRow = new QHBoxLayout();
    QLabel *lblRef = new QLabel("Dist. referencia:");
    lblRef->setStyleSheet("font-weight: bold; background: transparent;");
    refRow->addWidget(lblRef);
    spinRefDist = new QSpinBox();
    spinRefDist->setRange(5, 50);
    spinRefDist->setSuffix(" cm");
    spinRefDist->setValue(20);
    spinRefDist->setFixedSize(100, 26);
    refRow->addWidget(spinRefDist);
    refRow->addStretch();
    cfgLayout->addLayout(refRow);

    labelLockWarning = new QLabel("⚠  Cinta en movimiento — configuración bloqueada");
    labelLockWarning->setStyleSheet(
        "color: #C05621; background-color: #FEFCE8; border: 1px solid #F6AD55;"
        "border-radius: 4px; padding: 5px 7px 8px 7px; font-size: 11px;");
    labelLockWarning->setWordWrap(true);
    labelLockWarning->setMinimumHeight(38);
    labelLockWarning->setVisible(false);
    cfgLayout->addWidget(labelLockWarning);

    btnAplicar = new QPushButton("APLICAR CONFIGURACIÓN");
    btnAplicar->setEnabled(false);
    cfgLayout->addWidget(btnAplicar);
    connect(btnAplicar, &QPushButton::clicked, this, &MainWindow::onAplicarConfig);

    groupConfig->setMinimumWidth(240);
    midLayout->addWidget(groupConfig, 3);

    mainLayout->addLayout(midLayout);

    // ── Log ──────────────────────────────────────────────────────────────────
    QGroupBox *grpLog = new QGroupBox("Log de comunicación");
    QVBoxLayout *logLayout = new QVBoxLayout(grpLog);
    logEdit = new QPlainTextEdit();
    logEdit->setReadOnly(true);
    logEdit->setMaximumBlockCount(300);
    logEdit->setMinimumHeight(130);
    logLayout->addWidget(logEdit);
    mainLayout->addWidget(grpLog);
}

// ─────────────────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────────────────
QString MainWindow::ahora() const
{
    return QDateTime::currentDateTime().toString("[hh:mm:ss]");
}

void MainWindow::log(const QString &msg)
{
    logEdit->appendPlainText(ahora() + "  " + msg);
}

void MainWindow::cargarPuertos()
{
    comboPuertos->clear();
    for (const QSerialPortInfo &p : QSerialPortInfo::availablePorts())
        comboPuertos->addItem(p.portName());
    if (comboPuertos->count() == 0)
        log("No se encontraron puertos COM disponibles.");
}

bool MainWindow::enviarComando(quint8 cmd, const QByteArray &payload)
{
    if (!serial->isOpen()) return false;
    QByteArray trama = protocoloUNERQt::buildFrame(cmd, payload);
    if (trama.isEmpty()) return false;
    serial->write(trama);
    return true;
}

void MainWindow::setConfigLocked(bool locked)
{
    spinSmall->setEnabled(!locked);
    spinMedium->setEnabled(!locked);
    spinBig->setEnabled(!locked);
    spinRefDist->setEnabled(!locked);
    radioNormal->setEnabled(!locked);
    radioEstimado->setEnabled(!locked);
    btnAplicar->setEnabled(!locked && conectado);
    labelLockWarning->setVisible(locked);
}

void MainWindow::actualizarEstadoCinta(quint8 state)
{
    struct { const char *texto; const char *color; } info[] = {
        { "MIDIENDO",    "#276749" },
        { "PATEADOR  1", "#C05621" },
        { "PATEADOR  2", "#C05621" },
        { "PATEADOR  3", "#C05621" },
    };
    if (state > 3) return;
    labelEstadoCinta->setText(info[state].texto);
    labelEstadoCinta->setStyleSheet(
        QString("background-color: %1; color: white; font-size: 15px; font-weight: bold;"
                "border-radius: 8px; padding: 16px 10px;").arg(info[state].color));
    setConfigLocked(state != CONV_MEASURE);
}

void MainWindow::actualizarContadores(const QByteArray &payload)
{
    if (payload.size() < 6) return;
    quint16 s = (static_cast<quint8>(payload[0]) << 8) | static_cast<quint8>(payload[1]);
    quint16 m = (static_cast<quint8>(payload[2]) << 8) | static_cast<quint8>(payload[3]);
    quint16 b = (static_cast<quint8>(payload[4]) << 8) | static_cast<quint8>(payload[5]);
    lcdPequena->display(s);
    lcdMediana->display(m);
    lcdGrande->display(b);
    if (payload.size() >= 8) {
        quint16 n = (static_cast<quint8>(payload[6]) << 8) | static_cast<quint8>(payload[7]);
        lcdDescartadas->display(n);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Slots
// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::onConnectClicked()
{
    if (!conectado) {
        QString puerto = comboPuertos->currentText();
        if (puerto.isEmpty()) { log("Error: no hay puerto seleccionado."); return; }

        serial->setPortName(puerto);
        serial->setBaudRate(QSerialPort::Baud115200);
        serial->setDataBits(QSerialPort::Data8);
        serial->setParity(QSerialPort::NoParity);
        serial->setStopBits(QSerialPort::OneStop);
        serial->setFlowControl(QSerialPort::NoFlowControl);

        if (serial->open(QIODevice::ReadWrite)) {
            conectado = true;
            btnConectar->setText("DESCONECTAR");
            btnConectar->setStyleSheet(
                "background-color: #E53E3E; color: white; border-radius: 5px;"
                "padding: 6px 14px; font-weight: bold;");
            labelConexion->setText("● Conectado");
            labelConexion->setStyleSheet("color: #276749; font-weight: bold; background: transparent;");
            btnAplicar->setEnabled(true);
            pollTimer->start();
            log("Puerto " + puerto + " abierto — 115200 8N1.");
        } else {
            log("Error al abrir " + puerto + ": " + serial->errorString());
        }
    } else {
        serial->close();
        conectado = false;
        pollTimer->stop();
        btnConectar->setText("CONECTAR");
        btnConectar->setStyleSheet("");
        labelConexion->setText("● Desconectado");
        labelConexion->setStyleSheet("color: #E53E3E; font-weight: bold; background: transparent;");
        btnAplicar->setEnabled(false);
        labelEstadoCinta->setText("DESCONECTADO");
        labelEstadoCinta->setStyleSheet(
            "background-color: #718096; color: white; font-size: 15px; font-weight: bold;"
            "border-radius: 8px; padding: 16px 10px;");
        labelDistancia->setText("Altura: -- cm");
        setConfigLocked(false);
        log("Puerto cerrado.");
    }
}

void MainWindow::onRefreshPorts()
{
    cargarPuertos();
}

void MainWindow::onLeerDatos()
{
    QByteArray datos = serial->readAll();
    for (qsizetype i = 0; i < datos.size(); i++)
        protocolo->decodeByte(static_cast<quint8>(static_cast<unsigned char>(datos.at(i))));
}

void MainWindow::onPaqueteRecibido(quint8 cmd, QByteArray payload)
{
    switch (cmd) {

    case CMD_ERR_SENSOR:
        log("[A0] ⚠ Error sensor HC-SR04");
        labelDistancia->setText("Altura: ERROR");
        labelDistancia->setStyleSheet("font-size: 13px; color: #E53E3E; background: transparent;");
        break;

    case CMD_DIST_MEAS:
        if (!payload.isEmpty()) {
            quint8 d = static_cast<quint8>(payload[0]);
            labelDistancia->setText(QString("Altura: %1 cm").arg(d));
            labelDistancia->setStyleSheet("font-size: 13px; color: #4A5568; background: transparent;");
        }
        break;

    case CMD_BOX_CLASSIF:
        if (!payload.isEmpty()) {
            static const char *tipos[] = {"(ninguna)", "PEQUEÑA", "MEDIANA", "GRANDE"};
            quint8 t = static_cast<quint8>(payload[0]);
            log(QString("[A2]  Caja clasificada: %1").arg(t <= 3 ? tipos[t] : "?"));
        }
        break;

    case CMD_BOX_EJECTED:
        if (!payload.isEmpty()) {
            static const char *tipos[] = {"(ninguna)", "PEQUEÑA", "MEDIANA", "GRANDE"};
            quint8 t = static_cast<quint8>(payload[0]);
            log(QString("[A3] ✓ Caja eyectada:   %1").arg(t <= 3 ? tipos[t] : "?"));
        }
        break;

    case CMD_STATE:
        if (!payload.isEmpty())
            actualizarEstadoCinta(static_cast<quint8>(payload[0]));
        break;

    case CMD_COUNTS:
        actualizarContadores(payload);
        break;

    case CMD_BOX_DISCARDED:
        log("[A7] ✗ Caja descartada — fuera de rango o cajas pegadas");
        break;

    case CMD_ACK:
        if (payload.size() >= 2) {
            quint8 echocmd = static_cast<quint8>(payload[0]);
            quint8 status  = static_cast<quint8>(payload[1]);
            const char *st = (status == ACK_OK) ? "OK" : (status == ACK_BUSY) ? "BUSY" : "INVALID";
            log(QString("[A6]  ACK  cmd=0x%1  →  %2")
                    .arg(echocmd, 2, 16, QChar('0')).toUpper()
                    .arg(st));
        }
        break;

    default:
        log(QString("[??]  CMD desconocido: 0x%1").arg(cmd, 2, 16, QChar('0')).toUpper());
        break;
    }
}

void MainWindow::onBadChecksum()
{
    log("[ERR]  Checksum incorrecto — trama descartada");
}

void MainWindow::onPollTimer()
{
    enviarComando(CMD_GET_STATE);
    enviarComando(CMD_GET_COUNTS);
}

void MainWindow::onAplicarConfig()
{
    QByteArray p;

    p.clear();
    p.append(static_cast<char>(radioEstimado->isChecked() ? 1 : 0));
    enviarComando(CMD_SET_MODE, p);

    p.clear();
    p.append(static_cast<char>(spinSmall->value()));
    p.append(static_cast<char>(spinMedium->value()));
    p.append(static_cast<char>(spinBig->value()));
    enviarComando(CMD_SET_THRESH, p);

    p.clear();
    p.append(static_cast<char>(spinRefDist->value()));
    enviarComando(CMD_SET_CALIB, p);

    log(QString("[CFG]  Modo: %1  |  Umbrales: %2/%3/%4 cm  |  Ref: %5 cm")
            .arg(radioEstimado->isChecked() ? "Estimado" : "Normal")
            .arg(spinSmall->value())
            .arg(spinMedium->value())
            .arg(spinBig->value())
            .arg(spinRefDist->value()));
}

void MainWindow::onResetContadores()
{
    enviarComando(CMD_RESET_COUNTS);
    lcdPequena->display(0);
    lcdMediana->display(0);
    lcdGrande->display(0);
    lcdDescartadas->display(0);
    log("[RST]  Contadores reseteados.");
}
