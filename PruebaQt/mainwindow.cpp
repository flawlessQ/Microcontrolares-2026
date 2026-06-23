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
QComboBox::drop-down { border-left: 1px solid #CBD5E0; width: 20px; background: #EDF2F7; }
QComboBox::down-arrow { image: url(:/icons/down_arrow.svg); width: 10px; height: 10px; }
QComboBox QAbstractItemView {
    background: white;
    color: #2D3748;
    selection-background-color: #EBF8FF;
}
QSpinBox {
    border: 1px solid #CBD5E0;
    padding: 2px 20px 2px 4px;
    background: white;
    color: #2D3748;
    min-height: 22px;
}
QSpinBox::up-button {
    subcontrol-origin: border;
    subcontrol-position: top right;
    width: 18px;
    border-left: 1px solid #CBD5E0;
    background: #EDF2F7;
}
QSpinBox::up-button:pressed { background: #CBD5E0; }
QSpinBox::up-arrow {
    image: url(:/icons/up_arrow.svg);
    width: 8px;
    height: 8px;
}
QSpinBox::down-button {
    subcontrol-origin: border;
    subcontrol-position: bottom right;
    width: 18px;
    border-left: 1px solid #CBD5E0;
    border-top: 1px solid #CBD5E0;
    background: #EDF2F7;
}
QSpinBox::down-button:pressed { background: #CBD5E0; }
QSpinBox::down-arrow {
    image: url(:/icons/down_arrow.svg);
    width: 8px;
    height: 8px;
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

    // ── Pestañas ─────────────────────────────────────────────────────────────
    QTabWidget *tabs = new QTabWidget();
    tabs->setStyleSheet(R"(
        QTabWidget::pane  { border: 1px solid #CBD5E0; border-radius: 6px; background: white; }
        QTabBar::tab      { background: #EDF2F7; color: #4A5568; padding: 7px 20px;
                            border: 1px solid #CBD5E0; border-bottom: none;
                            border-top-left-radius: 5px; border-top-right-radius: 5px; }
        QTabBar::tab:selected { background: white; color: #1E5FA3; font-weight: bold; }
        QTabBar::tab:hover    { background: #E2E8F0; }
    )");

    // ════════════════════════════════════════════════════════════════════════
    // PESTAÑA 1 — ESTADO
    // ════════════════════════════════════════════════════════════════════════
    QWidget *tabEstado = new QWidget();
    QHBoxLayout *midLayout = new QHBoxLayout(tabEstado);
    midLayout->setSpacing(10);
    midLayout->setContentsMargins(10, 10, 10, 10);

    // --- Estado del transportador ---
    QGroupBox *grpEstado = new QGroupBox("Transportador");
    QVBoxLayout *estadoLayout = new QVBoxLayout(grpEstado);
    estadoLayout->setSpacing(8);

    labelEstadoCinta = new QLabel("DESCONECTADO");
    labelEstadoCinta->setAlignment(Qt::AlignCenter);
    labelEstadoCinta->setStyleSheet(
        "background-color: #718096; color: white; font-size: 15px; font-weight: bold;"
        "border-radius: 8px; padding: 16px 10px;");
    labelEstadoCinta->setMinimumHeight(64);
    estadoLayout->addWidget(labelEstadoCinta);

    labelDistancia = new QLabel("Altura medida: -- cm");
    labelDistancia->setAlignment(Qt::AlignCenter);
    labelDistancia->setStyleSheet("font-size: 13px; color: #4A5568; background: transparent;");
    estadoLayout->addWidget(labelDistancia);

    labelVelocidad = new QLabel("Velocidad de cinta: -- mm/s");
    labelVelocidad->setAlignment(Qt::AlignCenter);
    labelVelocidad->setStyleSheet("font-size: 13px; color: #4A5568; background: transparent;");
    estadoLayout->addWidget(labelVelocidad);

    btnConveyor = new QPushButton("INICIAR CINTA");
    btnConveyor->setEnabled(false);
    btnConveyor->setStyleSheet("background-color: #276749;");
    estadoLayout->addWidget(btnConveyor);
    connect(btnConveyor, &QPushButton::clicked, this, &MainWindow::onConveyorToggle);
    estadoLayout->addStretch();

    midLayout->addWidget(grpEstado, 1);

    // --- Contadores ---
    QGroupBox *grpCnt = new QGroupBox("Contadores de Cajas");
    QVBoxLayout *cntLayout = new QVBoxLayout(grpCnt);
    cntLayout->setSpacing(6);

    auto makeCounter = [&](const QString &nombre, const QString &color,
                           QLCDNumber *&lcd) {
        QHBoxLayout *row = new QHBoxLayout();
        row->setSpacing(10);
        QLabel *lbl = new QLabel(nombre);
        lbl->setFixedWidth(80);
        lbl->setStyleSheet(QString("color: %1; font-weight: bold; background: transparent;").arg(color));
        lcd = new QLCDNumber(4);
        lcd->setSegmentStyle(QLCDNumber::Filled);
        lcd->setFixedHeight(36);
        lcd->display(0);
        row->addWidget(lbl);
        row->addWidget(lcd);
        cntLayout->addLayout(row);
    };
    makeCounter("Pequeña",     "#2B6CB0", lcdPequena);
    makeCounter("Mediana",     "#276749", lcdMediana);
    makeCounter("Grande",      "#C05621", lcdGrande);
    makeCounter("Descartadas", "#742A2A", lcdDescartadas);

    cntLayout->addStretch();
    QPushButton *btnReset = new QPushButton("RESET CONTADORES");
    btnReset->setStyleSheet("background-color: #718096;");
    cntLayout->addWidget(btnReset);
    connect(btnReset, &QPushButton::clicked, this, &MainWindow::onResetContadores);

    midLayout->addWidget(grpCnt, 1);

    tabs->addTab(tabEstado, "Estado");

    // ════════════════════════════════════════════════════════════════════════
    // PESTAÑA 2 — CONFIGURACIÓN
    // ════════════════════════════════════════════════════════════════════════
    QWidget *tabConfig = new QWidget();
    QHBoxLayout *cfgMainLayout = new QHBoxLayout(tabConfig);
    cfgMainLayout->setSpacing(10);
    cfgMainLayout->setContentsMargins(10, 10, 10, 10);

    // --- Columna izquierda: modo + umbrales ---
    groupConfig = new QGroupBox("Clasificación");
    groupConfig->setMinimumHeight(220);
    QVBoxLayout *cfgLayout = new QVBoxLayout(groupConfig);
    cfgLayout->setSpacing(10);

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

    QLabel *lblUmbrales = new QLabel("Alturas minimas de deteccion:");
    lblUmbrales->setStyleSheet("font-weight: bold; background: transparent;");
    cfgLayout->addWidget(lblUmbrales);

    auto makeSpin = [](int val, int mn, int mx) {
        QSpinBox *s = new QSpinBox();
        s->setRange(mn, mx);
        s->setSuffix(" cm");
        s->setValue(val);
        s->setFixedWidth(90);
        return s;
    };
    spinSmall   = makeSpin(6,  1, 30);
    spinMedium  = makeSpin(8,  1, 30);
    spinBig     = makeSpin(10, 1, 30);
    spinRefDist = makeSpin(20, 5, 50);

    // Grid 2x2: [label][spin] | [label][spin]
    QGridLayout *threshGrid = new QGridLayout();
    threshGrid->setHorizontalSpacing(8);
    threshGrid->setVerticalSpacing(10);
    threshGrid->setColumnStretch(4, 1);  // columna final absorbe espacio sobrante

    struct { const char *lbl; QSpinBox **spin; int row; int col; } cells[] = {
        { "Pequena:",    &spinSmall,   0, 0 },
        { "Mediana:",    &spinMedium,  0, 2 },
        { "Grande:",     &spinBig,     1, 0 },
        { "Dist. ref.:", &spinRefDist, 1, 2 },
    };
    for (auto &c : cells) {
        threshGrid->addWidget(new QLabel(c.lbl),  c.row, c.col,     Qt::AlignLeft | Qt::AlignVCenter);
        threshGrid->addWidget(*c.spin,            c.row, c.col + 1, Qt::AlignLeft | Qt::AlignVCenter);
    }
    cfgLayout->addLayout(threshGrid);

    labelLockWarning = new QLabel("⚠  Cinta en movimiento — configuracion bloqueada");
    labelLockWarning->setStyleSheet(
        "color: #C05621; background-color: #FEFCE8; border: 1px solid #F6AD55;"
        "border-radius: 4px; padding: 5px 7px 8px 7px; font-size: 11px;");
    labelLockWarning->setWordWrap(true);
    labelLockWarning->setMinimumHeight(38);
    labelLockWarning->setVisible(false);
    cfgLayout->addWidget(labelLockWarning);

    btnAplicar = new QPushButton("APLICAR CLASIFICACIÓN");
    btnAplicar->setEnabled(false);
    cfgLayout->addWidget(btnAplicar);
    connect(btnAplicar, &QPushButton::clicked, this, &MainWindow::onAplicarConfig);

    cfgMainLayout->addWidget(groupConfig, 1);

    // --- Columna derecha: servos ---
    QGroupBox *grpServos = new QGroupBox("Posiciones de Servos");
    grpServos->setMinimumHeight(240);
    QVBoxLayout *srvLayout = new QVBoxLayout(grpServos);
    srvLayout->setSpacing(10);

    QLabel *lblSrvInfo = new QLabel("Configurar angulo HOME y PUSH para cada servo:");
    lblSrvInfo->setStyleSheet("font-weight: bold; background: transparent;");
    srvLayout->addWidget(lblSrvInfo);

    // Cabeceras de columna
    QGridLayout *srvGrid = new QGridLayout();
    srvGrid->setSpacing(10);
    srvGrid->setColumnStretch(0, 0);
    srvGrid->setColumnStretch(1, 1);
    srvGrid->setColumnStretch(2, 0);
    srvGrid->setColumnStretch(3, 1);

    QLabel *hdrHome = new QLabel("HOME");
    QLabel *hdrPush = new QLabel("PUSH");
    hdrHome->setStyleSheet("color: #1E5FA3; font-weight: bold; background: transparent;");
    hdrPush->setStyleSheet("color: #C05621; font-weight: bold; background: transparent;");
    srvGrid->addWidget(new QLabel(""),  0, 0);
    srvGrid->addWidget(hdrHome,         0, 1);
    srvGrid->addWidget(hdrPush,         0, 2);

    const char *servoNames[] = { "Servo 1", "Servo 2", "Servo 3" };
    for (int i = 0; i < 3; i++) {
        QLabel *lname = new QLabel(servoNames[i]);
        lname->setStyleSheet("font-weight: bold; background: transparent;");

        comboServoHome[i] = new QComboBox();
        comboServoHome[i]->addItem("0°");
        comboServoHome[i]->addItem("180°");
        comboServoHome[i]->setCurrentIndex(0);
        comboServoHome[i]->setFixedWidth(90);

        comboServoPush[i] = new QComboBox();
        comboServoPush[i]->addItem("0°");
        comboServoPush[i]->addItem("180°");
        comboServoPush[i]->setCurrentIndex(1);
        comboServoPush[i]->setFixedWidth(90);

        srvGrid->addWidget(lname,              i + 1, 0);
        srvGrid->addWidget(comboServoHome[i],  i + 1, 1);
        srvGrid->addWidget(comboServoPush[i],  i + 1, 2);
    }
    srvLayout->addLayout(srvGrid);

    btnAplicarServos = new QPushButton("APLICAR SERVOS");
    btnAplicarServos->setEnabled(false);
    srvLayout->addWidget(btnAplicarServos);
    connect(btnAplicarServos, &QPushButton::clicked, this, &MainWindow::onAplicarServos);

    cfgMainLayout->addWidget(grpServos, 1);

    tabs->addTab(tabConfig, "Configuración");

    mainLayout->addWidget(tabs, 1);

    // ── Log ──────────────────────────────────────────────────────────────────
    QGroupBox *grpLog = new QGroupBox("Log de comunicación");
    QVBoxLayout *logLayout = new QVBoxLayout(grpLog);
    logEdit = new QPlainTextEdit();
    logEdit->setReadOnly(true);
    logEdit->setMaximumBlockCount(300);
    logEdit->setMinimumHeight(80);
    logLayout->addWidget(logEdit);
    mainLayout->addWidget(grpLog);

    this->setMinimumSize(860, 640);
    this->resize(960, 700);
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
    btnAplicarServos->setEnabled(!locked && conectado);
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
            btnAplicarServos->setEnabled(true);
            btnConveyor->setEnabled(true);
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
        btnAplicarServos->setEnabled(false);
        btnConveyor->setEnabled(false);
        conveyorRunning = false;
        btnConveyor->setText("INICIAR CINTA");
        btnConveyor->setStyleSheet("background-color: #276749;");
        labelVelocidad->setText("Velocidad: -- mm/s");
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

    case CMD_BELT_SPEED:
        if (payload.size() >= 2) {
            quint16 spd = (static_cast<quint8>(payload[0]) << 8) | static_cast<quint8>(payload[1]);
            labelVelocidad->setText(QString("Velocidad: %1 mm/s").arg(spd));
        }
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

void MainWindow::onAplicarServos()
{
    // index 0 = "0°"  -> 1ms, index 1 = "180°" -> 2ms
    auto idxToMs = [](int idx) -> quint8 { return idx == 0 ? 1 : 2; };

    for (int i = 0; i < 3; i++) {
        quint8 home_ms = idxToMs(comboServoHome[i]->currentIndex());
        quint8 push_ms = idxToMs(comboServoPush[i]->currentIndex());
        QByteArray p;
        p.append(static_cast<char>(i));
        p.append(static_cast<char>(home_ms));
        p.append(static_cast<char>(push_ms));
        enviarComando(CMD_SET_SERVO, p);
        log(QString("[SRV]  Servo %1 — Home: %2  Push: %3")
                .arg(i + 1)
                .arg(comboServoHome[i]->currentText())
                .arg(comboServoPush[i]->currentText()));
    }
}

void MainWindow::onConveyorToggle()
{
    conveyorRunning = !conveyorRunning;
    QByteArray p;
    p.append(static_cast<char>(conveyorRunning ? 1 : 0));
    enviarComando(CMD_SET_CONVEYOR, p);

    if (conveyorRunning) {
        btnConveyor->setText("DETENER CINTA");
        btnConveyor->setStyleSheet("background-color: #E53E3E;");
        log("[CNV]  Cinta iniciada.");
    } else {
        btnConveyor->setText("INICIAR CINTA");
        btnConveyor->setStyleSheet("background-color: #276749;");
        labelVelocidad->setText("Velocidad: -- mm/s");
        log("[CNV]  Cinta detenida.");
    }
}
