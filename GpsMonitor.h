#pragma once

#include <QObject>
#include <QProcess>

class GpsMonitor : public QObject
{
    Q_OBJECT

public:
    explicit GpsMonitor(const QString &mac, QObject *parent = nullptr);

public slots:
    void start();
    void stop();

signals:
    void status(const QString &msg);
    void connected();
    void disconnected();
    void finished();

private:
    QString  m_mac;
    bool     m_stop     = false;
    bool     m_running  = false;

    int     sdpFindChannel();
    bool    bindRfcomm(int channel);
    void    releaseRfcomm();
    void    createSymlink();
    void    removeSymlink();
    void    startGpsd();
    void    stopGpsd();
    bool    rfcommConnected();
    void    cleanup();
    void    notify(const QString &msg);
};
