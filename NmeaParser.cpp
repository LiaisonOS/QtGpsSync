#include "NmeaParser.h"
#include <QStringList>
#include <cstring>
#include <cmath>

bool NmeaParser::parseRmc(const QString &line, RmcFix &fix)
{
    // Accept $GPRMC and $GNRMC
    if (!line.startsWith("$GPRMC") && !line.startsWith("$GNRMC"))
        return false;

    // Strip checksum (*XX)
    QString stripped = line;
    int star = stripped.lastIndexOf('*');
    if (star >= 0)
        stripped = stripped.left(star);

    QStringList f = stripped.split(',');
    // $GNRMC,HHMMSS.ss,A,LLLL.LL,a,YYYYY.YY,a,x.x,x.x,DDMMYY,...
    if (f.size() < 10)
        return false;

    // Status must be A (active)
    if (f[2].trimmed() != "A")
        return false;

    // Time: HHMMSS or HHMMSS.ss
    QString timeStr = f[1];
    if (timeStr.length() < 6)
        return false;
    fix.hour = timeStr.mid(0, 2).toInt();
    fix.min  = timeStr.mid(2, 2).toInt();
    fix.sec  = timeStr.mid(4, 2).toInt();

    // Date: DDMMYY
    QString dateStr = f[9];
    if (dateStr.length() < 6)
        return false;
    fix.day  = dateStr.mid(0, 2).toInt();
    fix.mon  = dateStr.mid(2, 2).toInt();
    int yr2  = dateStr.mid(4, 2).toInt();
    fix.year = 2000 + yr2;

    // Latitude: DDMM.MMMM + N/S
    QString latStr = f[3];
    QString latDir = f[4];
    if (!latStr.isEmpty() && latStr.length() >= 4) {
        double rawLat = latStr.toDouble();
        int    degLat = (int)(rawLat / 100);
        double minLat = rawLat - degLat * 100;
        fix.lat = degLat + minLat / 60.0;
        if (latDir == "S") fix.lat = -fix.lat;
    }

    // Longitude: DDDMM.MMMM + E/W
    QString lonStr = f[5];
    QString lonDir = f[6];
    if (!lonStr.isEmpty() && lonStr.length() >= 5) {
        double rawLon = lonStr.toDouble();
        int    degLon = (int)(rawLon / 100);
        double minLon = rawLon - degLon * 100;
        fix.lon = degLon + minLon / 60.0;
        if (lonDir == "W") fix.lon = -fix.lon;
    }

    fix.valid = true;
    return true;
}

time_t NmeaParser::toEpoch(const RmcFix &fix)
{
    struct tm t;
    memset(&t, 0, sizeof(t));
    t.tm_year = fix.year - 1900;
    t.tm_mon  = fix.mon  - 1;
    t.tm_mday = fix.day;
    t.tm_hour = fix.hour;
    t.tm_min  = fix.min;
    t.tm_sec  = fix.sec;
    t.tm_isdst = 0;
    return timegm(&t);
}

QString NmeaParser::toGrid(double lat, double lon)
{
    // Maidenhead locator (6-char)
    lon += 180.0;
    lat += 90.0;

    int fieldLon = (int)(lon / 20);
    int fieldLat = (int)(lat / 10);
    int squareLon = (int)((lon - fieldLon * 20) / 2);
    int squareLat = (int)((lat - fieldLat * 10) / 1);
    int subLon = (int)((lon - fieldLon * 20 - squareLon * 2) * 12);
    int subLat = (int)((lat - fieldLat * 10 - squareLat)     * 24);

    QString grid;
    grid += QChar('A' + fieldLon);
    grid += QChar('A' + fieldLat);
    grid += QChar('0' + squareLon);
    grid += QChar('0' + squareLat);
    grid += QChar('a' + subLon);
    grid += QChar('a' + subLat);
    return grid;
}
