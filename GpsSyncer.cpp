#include "GpsSyncer.h"
#include <QThread>
#include <QDateTime>
#include <QProcess>
#include <QMap>
#include <sys/time.h>
#include <ctime>

GpsSyncer::GpsSyncer(const QString &mac, QObject *parent)
    : QObject(parent), m_mac(mac)
{
}

void GpsSyncer::abort()
{
    m_abort = true;
}

int GpsSyncer::sdpFindChannel()
{
    QProcess proc;
    proc.start("sdptool", {"browse", m_mac});
    if (!proc.waitForFinished(15000)) return -1;

    QString output = proc.readAllStandardOutput();
    QStringList serviceNames = {"LiaisonGPS", "Serial Port"};
    QMap<QString, int> found;
    QString inService;

    for (const QString &line : output.split('\n')) {
        for (const QString &name : serviceNames) {
            if (line.contains(name)) inService = name;
        }
        if (!inService.isEmpty() && line.contains("Channel:")) {
            int ch = line.trimmed().split(' ').last().toInt();
            if (!found.contains(inService)) found[inService] = ch;
            inService.clear();
        }
    }

    for (const QString &name : serviceNames) {
        if (found.contains(name)) {
            emit status(QString("SDP: found service '%1' on channel %2").arg(name).arg(found[name]));
            return found[name];
        }
    }
    return -1;
}

bool GpsSyncer::bindRfcomm(int channel)
{
    QProcess proc;
    proc.start("sudo", {"rfcomm", "bind", "0", m_mac, QString::number(channel)});
    proc.waitForFinished(5000);
    return proc.exitCode() == 0;
}

void GpsSyncer::releaseRfcomm()
{
    QProcess proc;
    proc.start("sudo", {"rfcomm", "release", "0"});
    proc.waitForFinished(3000);
}

QString GpsSyncer::readLine(int timeoutMs)
{
    QByteArray buf;
    while (!m_abort) {
        if (!m_serial->waitForReadyRead(timeoutMs))
            return QString();
        buf += m_serial->readAll();
        int nl = buf.indexOf('\n');
        if (nl >= 0) {
            QString line = QString::fromLatin1(buf.left(nl)).trimmed();
            buf = buf.mid(nl + 1);
            return line;
        }
    }
    return QString();
}

bool GpsSyncer::setSystemClock(time_t epoch, long subsecUs)
{
    struct timeval tv;
    tv.tv_sec  = epoch;
    tv.tv_usec = subsecUs;
    return settimeofday(&tv, nullptr) == 0;
}

void GpsSyncer::run()
{
    // ---- Step 1: SDP lookup ----
    emit status("Looking up SDP channel...");
    int channel = sdpFindChannel();
    if (channel < 0 || m_abort) {
        emit status("ERROR: Could not find GPS service via SDP");
        emit finished(false, RmcFix{}, 0);
        return;
    }

    // ---- Step 2: bind rfcomm0 ----
    emit status(QString("Binding rfcomm0 to %1 channel %2...").arg(m_mac).arg(channel));
    releaseRfcomm();   // release any stale binding first
    if (!bindRfcomm(channel) || m_abort) {
        emit status("ERROR: rfcomm bind failed");
        emit finished(false, RmcFix{}, 0);
        return;
    }

    // ---- Step 3: open serial port ----
    QSerialPort serial;
    m_serial = &serial;
    serial.setPortName("/dev/rfcomm0");
    serial.setBaudRate(QSerialPort::Baud9600);
    serial.setDataBits(QSerialPort::Data8);
    serial.setParity(QSerialPort::NoParity);
    serial.setStopBits(QSerialPort::OneStop);
    serial.setFlowControl(QSerialPort::NoFlowControl);

    if (!serial.open(QIODevice::ReadOnly)) {
        emit status(QString("ERROR: Cannot open rfcomm0 — %1").arg(serial.errorString()));
        m_serial = nullptr;
        releaseRfcomm();
        emit finished(false, RmcFix{}, 0);
        return;
    }

    emit status("Connected — waiting for GPS fix...");

    // ---- Phase 1: get first valid RMC fix ----
    RmcFix first;
    int attempts = 0;
    while (!m_abort && attempts < 200) {
        QString line = readLine(3000);
        if (line.isEmpty()) { attempts++; continue; }
        if (NmeaParser::parseRmc(line, first)) break;
        attempts++;
    }

    if (!first.valid || m_abort) {
        serial.close();
        m_serial = nullptr;
        releaseRfcomm();
        emit status("ERROR: Timeout waiting for GPS fix");
        emit finished(false, RmcFix{}, 0);
        return;
    }

    emit status(QString("GPS fix: %1/%2/%3 %4:%5:%6 UTC  Lat:%7 Lon:%8  Grid:%9")
        .arg(first.year).arg(first.mon,2,10,QChar('0')).arg(first.day,2,10,QChar('0'))
        .arg(first.hour,2,10,QChar('0')).arg(first.min,2,10,QChar('0')).arg(first.sec,2,10,QChar('0'))
        .arg(first.lat, 0, 'f', 5).arg(first.lon, 0, 'f', 5)
        .arg(NmeaParser::toGrid(first.lat, first.lon)));

    emit status("Waiting for second boundary (draining BT buffer)...");

    // ---- Phase 2: detect second boundary, drain BT buffer ----
    RmcFix prev = first;
    RmcFix current;
    bool   found = false;
    qint64 deadlineMs = QDateTime::currentMSecsSinceEpoch() + 30000;

    while (!m_abort && QDateTime::currentMSecsSinceEpoch() < deadlineMs) {
        QString line = readLine(2000);
        if (line.isEmpty()) continue;
        if (!NmeaParser::parseRmc(line, current)) continue;

        time_t tPrev    = NmeaParser::toEpoch(prev);
        time_t tCurrent = NmeaParser::toEpoch(current);

        if (tCurrent != tPrev) {
            qint64 wallMs  = QDateTime::currentMSecsSinceEpoch();
            long   wallSec = (long)(wallMs / 1000);
            long   lag     = wallSec - (long)tCurrent;

            emit status(QString("Transition: GPS=%1  wall=%2  lag=%3s")
                .arg(tCurrent).arg(wallSec).arg(lag));

            if (lag <= 1) {
                long subsecUs = (wallMs % 1000) * 1000L;
                found = true;
                emit status("BT buffer drained — setting clock...");

                if (!setSystemClock(tCurrent, subsecUs)) {
                    serial.close();
                    m_serial = nullptr;
                    releaseRfcomm();
                    emit status("ERROR: settimeofday failed — need root");
                    emit finished(false, current, 0);
                    return;
                }

                system("hwclock --systohc 2>/dev/null");

                emit status(QString("Clock set: %1-%2-%3 %4:%5:%6 UTC")
                    .arg(current.year).arg(current.mon,2,10,QChar('0')).arg(current.day,2,10,QChar('0'))
                    .arg(current.hour,2,10,QChar('0')).arg(current.min,2,10,QChar('0')).arg(current.sec,2,10,QChar('0')));
                break;
            }

            emit status(QString("Still in BT buffer (%1s behind), continuing...").arg(lag));
        }
        prev = current;
    }

    if (!found || m_abort) {
        serial.close();
        m_serial = nullptr;
        releaseRfcomm();
        emit status("ERROR: Timeout waiting for second boundary");
        emit finished(false, RmcFix{}, 0);
        return;
    }

    // ---- Phase 3: verification pass ----
    emit status("Verifying clock accuracy...");

    RmcFix vprev = current;
    RmcFix vcurrent;
    qint64 verifyDeadline = QDateTime::currentMSecsSinceEpoch() + 10000;

    while (!m_abort && QDateTime::currentMSecsSinceEpoch() < verifyDeadline) {
        QString line = readLine(2000);
        if (line.isEmpty()) continue;
        if (!NmeaParser::parseRmc(line, vcurrent)) continue;

        time_t vtPrev    = NmeaParser::toEpoch(vprev);
        time_t vtCurrent = NmeaParser::toEpoch(vcurrent);

        if (vtCurrent != vtPrev) {
            qint64 wallMs  = QDateTime::currentMSecsSinceEpoch();
            long   wallSec = (long)(wallMs / 1000);
            long   delta   = wallSec - (long)vtCurrent;

            emit verifyResult(delta);

            if (delta != 0) {
                emit status(QString("Correction applied: %1%2s").arg(delta > 0 ? "+" : "").arg(delta));
                setSystemClock(vtCurrent, (wallMs % 1000) * 1000L);
                system("hwclock --systohc 2>/dev/null");
            } else {
                emit status("Verification OK — clock accurate (0s delta)");
            }
            break;
        }
        vprev = vcurrent;
    }

    serial.close();
    m_serial = nullptr;
    releaseRfcomm();
    emit finished(true, current, 0);
}
