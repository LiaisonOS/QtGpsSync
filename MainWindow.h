#pragma once

#include <QMainWindow>
#include <QLabel>
#include <QTextEdit>
#include <QPushButton>
#include <QComboBox>
#include <QTimer>
#include <QThread>
#include "GpsSyncer.h"
#include "GpsMonitor.h"
#include "NmeaParser.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onStartSync();
    void onStartGpsMode();
    void onStopGpsMode();
    void onAbort();
    void onSyncStatus(const QString &msg);
    void onSyncFinished(bool success, const RmcFix &fix, long deltaMs);
    void onVerifyResult(long delta);
    void onGpsStatus(const QString &msg);
    void onGpsConnected();
    void onGpsDisconnected();
    void updateClock();
    void refreshDevices();

private:
    void setupUi();
    void log(const QString &msg, const QString &color = "#9dbfad");
    void setRunning(bool running);
    void setGpsMode(bool active);
    QString realUserHome();
    void notifyDashboard(const RmcFix &fix, const QString &grid);

    QLabel      *m_clockLabel;
    QTextEdit   *m_logView;
    QPushButton *m_syncBtn;
    QPushButton *m_gpsModeBtn;
    QPushButton *m_abortBtn;
    QPushButton *m_refreshBtn;
    QComboBox   *m_deviceCombo;
    QLabel      *m_statusLabel;
    QLabel      *m_fixLabel;

    QTimer      *m_clockTimer;

    // Sync worker
    QThread     *m_thread    = nullptr;
    GpsSyncer   *m_syncer    = nullptr;

    // GPS mode worker
    QThread     *m_gpsThread   = nullptr;
    GpsMonitor  *m_gpsMonitor  = nullptr;
};
