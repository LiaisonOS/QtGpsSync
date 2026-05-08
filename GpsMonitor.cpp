#include "GpsMonitor.h"
#include <QThread>
#include <QMap>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <cstring>

#define RFCOMM_DEV  "/dev/rfcomm0"
#define ET_GPS_LINK "/dev/et-gps"
#define GPSD_WRAPPER "/opt/emcomm-tools/sbin/wrapper-gpsd.sh"

GpsMonitor::GpsMonitor(const QString &mac, QObject *parent)
    : QObject(parent), m_mac(mac)
{
}

int GpsMonitor::sdpFindChannel()
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
            emit status(QString("SDP: found '%1' on channel %2").arg(name).arg(found[name]));
            return found[name];
        }
    }
    return -1;
}

bool GpsMonitor::bindRfcomm(int channel)
{
    QProcess proc;
    proc.start("sudo", {"rfcomm", "bind", "0", m_mac, QString::number(channel)});
    proc.waitForFinished(5000);
    return proc.exitCode() == 0;
}

void GpsMonitor::releaseRfcomm()
{
    QProcess proc;
    proc.start("sudo", {"rfcomm", "release", "0"});
    proc.waitForFinished(3000);
}

void GpsMonitor::createSymlink()
{
    QProcess::execute("sudo", {"ln", "-sf", RFCOMM_DEV, ET_GPS_LINK});
}

void GpsMonitor::removeSymlink()
{
    QProcess::execute("sudo", {"rm", "-f", ET_GPS_LINK});
}

void GpsMonitor::startGpsd()
{
    QProcess::startDetached(GPSD_WRAPPER, {"start"});
}

void GpsMonitor::stopGpsd()
{
    QProcess::execute(GPSD_WRAPPER, {"stop"});
}

bool GpsMonitor::rfcommConnected()
{
    QProcess proc;
    proc.start("rfcomm", {"show", "0"});
    proc.waitForFinished(3000);
    return proc.readAllStandardOutput().toLower().contains("connected");
}

void GpsMonitor::start()
{
    m_stop = false;

    // SDP lookup
    emit status("Looking up SDP channel...");
    int channel = sdpFindChannel();
    if (channel < 0 || m_stop) {
        emit status("ERROR: Could not find GPS service via SDP");
        emit finished();
        return;
    }

    // Bind rfcomm
    emit status(QString("Binding rfcomm0 to %1 channel %2...").arg(m_mac).arg(channel));
    releaseRfcomm();
    if (!bindRfcomm(channel) || m_stop) {
        emit status("ERROR: rfcomm bind failed");
        emit finished();
        return;
    }

    // Symlink + gpsd
    emit status("Creating /dev/et-gps symlink...");
    createSymlink();
    emit status("Starting gpsd...");
    startGpsd();

    // Wait for rfcomm to become active (up to 30s)
    emit status("Waiting for GPS connection...");
    for (int i = 0; i < 30 && !m_stop; i++) {
        if (rfcommConnected()) break;
        QThread::sleep(1);
    }

    if (m_stop) {
        cleanup();
        return;
    }

    m_running = true;
    emit status("GPS mode active — connected to " + m_mac);
    notify("gps:start");
    notify("gps:running");
    emit connected();

    // Monitor loop — check connection every 2s, send pulse every 15s
    int pulseCounter = 0;
    while (!m_stop) {
        QThread::sleep(2);
        pulseCounter += 2;

        if (!rfcommConnected()) {
            emit status("GPS disconnected — Android closed connection");
            break;
        }

        if (pulseCounter >= 15) {
            // Check if gpsd has a fresh fix via gpspipe
            QProcess gp;
            gp.start("gpspipe", {"-w", "-n", "10"});
            bool hasFix = false;
            if (gp.waitForFinished(9000)) {
                QString out = gp.readAllStandardOutput();
                for (const QString &line : out.split('\n')) {
                    if (line.contains("\"class\":\"TPV\"") && line.contains("\"time\"")) {
                        hasFix = true;
                        break;
                    }
                }
            }
            if (hasFix) {
                notify("gps:running");
                emit status("GPS: fix active");
            } else {
                notify("gps:warn");
                emit status("GPS: no fix — signal lost");
            }
            pulseCounter = 0;
        }
    }

    notify("gps:stop");
    cleanup();
}

void GpsMonitor::stop()
{
    m_stop = true;
}

void GpsMonitor::notify(const QString &msg)
{
    // Send UDP datagram to et-dashboard GPS notify socket
    int fd = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (fd < 0) return;

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, "/tmp/et-gps-notify.sock", sizeof(addr.sun_path) - 1);

    QByteArray data = msg.toUtf8();
    sendto(fd, data.constData(), data.size(), 0,
           (struct sockaddr *)&addr, sizeof(addr));
    close(fd);
}

void GpsMonitor::cleanup()
{
    emit status("Stopping gpsd...");
    stopGpsd();
    emit status("Removing /dev/et-gps symlink...");
    removeSymlink();
    emit status("Releasing rfcomm0...");
    releaseRfcomm();
    m_running = false;
    emit disconnected();
    emit finished();
}
