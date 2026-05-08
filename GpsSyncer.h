#pragma once

#include <QObject>
#include <QSerialPort>
#include "NmeaParser.h"

class GpsSyncer : public QObject
{
    Q_OBJECT

public:
    explicit GpsSyncer(const QString &mac, QObject *parent = nullptr);

public slots:
    void run();
    void abort();

signals:
    void status(const QString &msg);
    void finished(bool success, const RmcFix &fix, long deltaMs);
    void verifyResult(long delta);

private:
    QString      m_mac;
    bool         m_abort  = false;
    QSerialPort *m_serial = nullptr;

    bool    bindRfcomm(int channel);
    void    releaseRfcomm();
    int     sdpFindChannel();
    QString readLine(int timeoutMs = 2000);
    bool    setSystemClock(time_t epoch, long subsecUs);
};
