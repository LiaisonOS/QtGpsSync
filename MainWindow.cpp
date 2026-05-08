#include "MainWindow.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDateTime>
#include <QProcess>
#include <QFont>
#include <QScrollBar>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDir>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle("QtGpsSync — LiaisonOS");
    setMinimumSize(520, 520);
    setupUi();

    m_clockTimer = new QTimer(this);
    connect(m_clockTimer, &QTimer::timeout, this, &MainWindow::updateClock);
    m_clockTimer->start(100);
    updateClock();

    refreshDevices();
}

MainWindow::~MainWindow()
{
    if (m_syncer)     m_syncer->abort();
    if (m_gpsMonitor) m_gpsMonitor->stop();
    if (m_thread)     { m_thread->quit();    m_thread->wait(2000); }
    if (m_gpsThread)  { m_gpsThread->quit(); m_gpsThread->wait(2000); }
}

void MainWindow::setupUi()
{
    QWidget *central = new QWidget(this);
    setCentralWidget(central);
    QVBoxLayout *root = new QVBoxLayout(central);
    root->setSpacing(10);
    root->setContentsMargins(14, 14, 14, 14);

    // ---- Live UTC clock ----
    QLabel *clockCaption = new QLabel("System Time (UTC)", this);
    clockCaption->setAlignment(Qt::AlignCenter);
    clockCaption->setStyleSheet("color: #666; font-size: 10px;");
    m_clockLabel = new QLabel("--:--:--", this);
    QFont clockFont = m_clockLabel->font();
    clockFont.setFamily("Monospace");
    clockFont.setPointSize(36);
    clockFont.setBold(true);
    m_clockLabel->setFont(clockFont);
    m_clockLabel->setAlignment(Qt::AlignCenter);
    m_clockLabel->setStyleSheet("color: #4ade80;");
    root->addWidget(clockCaption);
    root->addWidget(m_clockLabel);

    // ---- Device selector ----
    QHBoxLayout *devRow = new QHBoxLayout();
    m_deviceCombo = new QComboBox(this);
    m_refreshBtn  = new QPushButton("↻", this);
    m_refreshBtn->setFixedWidth(36);
    m_refreshBtn->setToolTip("Refresh paired devices");
    devRow->addWidget(new QLabel("BT Device:", this));
    devRow->addWidget(m_deviceCombo, 1);
    devRow->addWidget(m_refreshBtn);
    root->addLayout(devRow);

    // ---- Buttons ----
    QHBoxLayout *btnRow = new QHBoxLayout();
    m_syncBtn     = new QPushButton("Sync GPS Time", this);
    m_gpsModeBtn  = new QPushButton("GPS Mode", this);
    m_abortBtn    = new QPushButton("Abort", this);
    m_abortBtn->setEnabled(false);
    m_syncBtn->setStyleSheet(
        "QPushButton { background-color: #FFA500; color: #1a1a1a; font-weight: bold; }"
        "QPushButton:disabled { background-color: #555; color: #888; font-weight: bold; }");
    m_gpsModeBtn->setStyleSheet(
        "QPushButton { background-color: #60a5fa; color: #1a1a1a; font-weight: bold; }"
        "QPushButton:disabled { background-color: #555; color: #888; font-weight: bold; }");
    m_abortBtn->setStyleSheet(
        "QPushButton { background-color: #f87171; color: #1a1a1a; }"
        "QPushButton:disabled { background-color: #555; color: #888; }");
    m_syncBtn->setMinimumHeight(40);
    m_gpsModeBtn->setMinimumHeight(40);
    m_abortBtn->setMinimumHeight(40);
    btnRow->addWidget(m_syncBtn);
    btnRow->addWidget(m_gpsModeBtn);
    btnRow->addWidget(m_abortBtn);
    root->addLayout(btnRow);

    // ---- Status / fix ----
    m_statusLabel = new QLabel("Ready — select a paired Bluetooth GPS device", this);
    m_statusLabel->setStyleSheet("color: #9e9e9e; font-size: 11px;");
    m_fixLabel = new QLabel("", this);
    m_fixLabel->setStyleSheet("color: #60a5fa; font-size: 11px;");
    root->addWidget(m_statusLabel);
    root->addWidget(m_fixLabel);

    // ---- Log ----
    QLabel *logCaption = new QLabel("Session Log", this);
    logCaption->setStyleSheet("color: #666; font-size: 10px;");
    root->addWidget(logCaption);
    m_logView = new QTextEdit(this);
    m_logView->setReadOnly(true);
    m_logView->setFont(QFont("Monospace", 9));
    m_logView->setStyleSheet(
        "background: #111; color: #9dbfad; border: 1px solid #333; border-radius: 4px;");
    root->addWidget(m_logView, 1);

    connect(m_syncBtn,    &QPushButton::clicked, this, &MainWindow::onStartSync);
    connect(m_gpsModeBtn, &QPushButton::clicked, this, &MainWindow::onStartGpsMode);
    connect(m_abortBtn,   &QPushButton::clicked, this, &MainWindow::onAbort);
    connect(m_refreshBtn, &QPushButton::clicked, this, &MainWindow::refreshDevices);
}

void MainWindow::refreshDevices()
{
    m_deviceCombo->clear();
    QProcess proc;
    proc.start("bluetoothctl", {"devices", "Paired"});
    proc.waitForFinished(10000);
    QString out = proc.readAllStandardOutput();
    for (const QString &line : out.split('\n')) {
        QStringList parts = line.trimmed().split(' ', Qt::SkipEmptyParts);
        if (parts.size() >= 3 && parts[0] == "Device") {
            QString mac  = parts[1];
            QString name = parts.mid(2).join(' ');
            m_deviceCombo->addItem(name + "  [" + mac + "]", mac);
        }
    }
    if (m_deviceCombo->count() == 0)
        m_deviceCombo->addItem("No paired devices found");
}

void MainWindow::updateClock()
{
    QDateTime now = QDateTime::currentDateTimeUtc();
    m_clockLabel->setText(now.toString("HH:mm:ss"));
}

void MainWindow::log(const QString &msg, const QString &color)
{
    m_logView->append(
        QString("<span style='color:%1;'>%2</span>").arg(color, msg.toHtmlEscaped()));
    m_logView->verticalScrollBar()->setValue(m_logView->verticalScrollBar()->maximum());
}

void MainWindow::setRunning(bool running)
{
    m_syncBtn->setEnabled(!running);
    m_gpsModeBtn->setEnabled(!running);
    m_abortBtn->setEnabled(running);
    m_deviceCombo->setEnabled(!running);
    m_refreshBtn->setEnabled(!running);
    m_clockLabel->setStyleSheet(running ? "color: #FFA500;" : "color: #4ade80;");
}

void MainWindow::setGpsMode(bool active)
{
    m_syncBtn->setEnabled(!active);
    m_abortBtn->setEnabled(false);
    m_deviceCombo->setEnabled(!active);
    m_refreshBtn->setEnabled(!active);

    if (active) {
        m_gpsModeBtn->setText("Stop GPS");
        m_gpsModeBtn->setStyleSheet(
            "QPushButton { background-color: #f87171; color: #1a1a1a; font-weight: bold; }"
            "QPushButton:disabled { background-color: #555; color: #888; font-weight: bold; }");
        disconnect(m_gpsModeBtn, &QPushButton::clicked, this, &MainWindow::onStartGpsMode);
        connect(m_gpsModeBtn,    &QPushButton::clicked, this, &MainWindow::onStopGpsMode);
        m_clockLabel->setStyleSheet("color: #4ade80;");
    } else {
        m_gpsModeBtn->setText("GPS Mode");
        m_gpsModeBtn->setStyleSheet(
            "QPushButton { background-color: #60a5fa; color: #1a1a1a; font-weight: bold; }"
            "QPushButton:disabled { background-color: #555; color: #888; font-weight: bold; }");
        disconnect(m_gpsModeBtn, &QPushButton::clicked, this, &MainWindow::onStopGpsMode);
        connect(m_gpsModeBtn,    &QPushButton::clicked, this, &MainWindow::onStartGpsMode);
        m_gpsModeBtn->setEnabled(true);
        m_clockLabel->setStyleSheet("color: #4ade80;");
    }
}

// ---- Sync GPS Time ----

void MainWindow::onStartSync()
{
    QString mac = m_deviceCombo->currentData().toString();
    if (mac.isEmpty()) { m_statusLabel->setText("No device selected"); return; }

    m_logView->clear();
    m_fixLabel->setText("");
    m_statusLabel->setText("Connecting...");
    setRunning(true);
    log(QString("▸ Starting sync with %1").arg(m_deviceCombo->currentText()));

    m_thread = new QThread(this);
    m_syncer = new GpsSyncer(mac);
    m_syncer->moveToThread(m_thread);

    connect(m_thread,  &QThread::started,        m_syncer, &GpsSyncer::run);
    connect(m_syncer,  &GpsSyncer::status,       this,     &MainWindow::onSyncStatus);
    connect(m_syncer,  &GpsSyncer::finished,     this,     &MainWindow::onSyncFinished);
    connect(m_syncer,  &GpsSyncer::verifyResult, this,     &MainWindow::onVerifyResult);
    connect(m_syncer,  &GpsSyncer::finished,     m_thread, &QThread::quit);
    connect(m_thread,  &QThread::finished,        m_syncer, &QObject::deleteLater);
    connect(m_thread,  &QThread::finished,        m_thread, &QObject::deleteLater);

    m_thread->start();
}

void MainWindow::onAbort()
{
    if (m_syncer) m_syncer->abort();
    log("▸ Aborted by user", "#fbbf24");
    m_statusLabel->setText("Aborted");
    setRunning(false);
}

void MainWindow::onSyncStatus(const QString &msg)
{
    log("▸ " + msg);
    m_statusLabel->setText(msg.left(70));
}

void MainWindow::onVerifyResult(long delta)
{
    if (delta == 0)
        log("✓ Verification OK — 0s delta", "#4ade80");
    else
        log(QString("⚠ Correction applied: %1%2s").arg(delta > 0 ? "+" : "").arg(delta), "#fbbf24");
}

QString MainWindow::realUserHome()
{
    const char *sudoUser = qgetenv("SUDO_USER").constData();
    if (sudoUser && strlen(sudoUser) > 0)
        return QString("/home/%1").arg(sudoUser);
    return QDir::homePath();
}

void MainWindow::saveGrid(const RmcFix &fix, const QString &grid)
{
    QString configPath = realUserHome() + "/.config/emcomm-tools/user.json";
    QFile f(configPath);

    QJsonObject obj;
    if (f.open(QIODevice::ReadOnly)) {
        obj = QJsonDocument::fromJson(f.readAll()).object();
        f.close();
    }

    obj["grid"] = grid;
    obj["lat"]  = fix.lat;
    obj["lon"]  = fix.lon;

    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        f.write(QJsonDocument(obj).toJson());
        f.close();
        const char *sudoUser = qgetenv("SUDO_USER").constData();
        if (sudoUser && strlen(sudoUser) > 0)
            QProcess::execute("chown", {QString(sudoUser), configPath});
        log(QString("▸ Grid saved to user.json: %1").arg(grid));
    } else {
        log(QString("⚠ Could not write user.json: %1").arg(configPath), "#fbbf24");
    }
}

void MainWindow::onSyncFinished(bool success, const RmcFix &fix, long)
{
    setRunning(false);
    m_thread = nullptr;
    m_syncer = nullptr;

    if (success) {
        m_statusLabel->setText("Sync complete");
        m_clockLabel->setStyleSheet("color: #4ade80;");
        QString grid = NmeaParser::toGrid(fix.lat, fix.lon);
        m_fixLabel->setText(QString("Lat: %1  Lon: %2  Grid: %3")
            .arg(fix.lat, 0, 'f', 5).arg(fix.lon, 0, 'f', 5).arg(grid));
        log(QString("✓ Sync complete — Grid: %1").arg(grid), "#4ade80");
        saveGrid(fix, grid);
    } else {
        m_statusLabel->setText("Sync failed");
        m_clockLabel->setStyleSheet("color: #f87171;");
        log("✗ Sync failed", "#f87171");
    }
}

// ---- GPS Continuous Mode ----

void MainWindow::onStartGpsMode()
{
    QString mac = m_deviceCombo->currentData().toString();
    if (mac.isEmpty()) { m_statusLabel->setText("No device selected"); return; }

    m_logView->clear();
    m_fixLabel->setText("");
    m_statusLabel->setText("Starting GPS mode...");
    setRunning(true);
    log(QString("▸ Starting GPS mode with %1").arg(m_deviceCombo->currentText()));

    m_gpsThread  = new QThread(this);
    m_gpsMonitor = new GpsMonitor(mac);
    m_gpsMonitor->moveToThread(m_gpsThread);

    connect(m_gpsThread,   &QThread::started,      m_gpsMonitor, &GpsMonitor::start);
    connect(m_gpsMonitor,  &GpsMonitor::status,    this,         &MainWindow::onGpsStatus);
    connect(m_gpsMonitor,  &GpsMonitor::connected, this,         &MainWindow::onGpsConnected);
    connect(m_gpsMonitor,  &GpsMonitor::disconnected, this,      &MainWindow::onGpsDisconnected);
    connect(m_gpsMonitor,  &GpsMonitor::finished,  m_gpsThread,  &QThread::quit);
    connect(m_gpsThread,   &QThread::finished,     m_gpsMonitor, &QObject::deleteLater);
    connect(m_gpsThread,   &QThread::finished,     m_gpsThread,  &QObject::deleteLater);

    m_gpsThread->start();
}

void MainWindow::onStopGpsMode()
{
    if (m_gpsMonitor) {
        m_gpsMonitor->stop();
        log("▸ Stopping GPS mode...", "#fbbf24");
        m_statusLabel->setText("Stopping GPS...");
    }
}

void MainWindow::onGpsStatus(const QString &msg)
{
    log("▸ " + msg);
    m_statusLabel->setText(msg.left(70));
}

void MainWindow::onGpsConnected()
{
    setRunning(false);
    setGpsMode(true);
    log("✓ GPS mode active — gpsd running on /dev/et-gps", "#4ade80");
    m_statusLabel->setText("GPS mode active");
}

void MainWindow::onGpsDisconnected()
{
    setGpsMode(false);
    m_gpsThread  = nullptr;
    m_gpsMonitor = nullptr;
    log("✓ GPS mode stopped", "#9dbfad");
    m_statusLabel->setText("GPS mode stopped");
}
