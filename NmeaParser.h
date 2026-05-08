#pragma once

#include <QString>
#include <QMetaType>
#include <ctime>

struct RmcFix {
    bool    valid    = false;
    int     year     = 0;
    int     mon      = 0;
    int     day      = 0;
    int     hour     = 0;
    int     min      = 0;
    int     sec      = 0;
    double  lat      = 0.0;
    double  lon      = 0.0;
};
Q_DECLARE_METATYPE(RmcFix)

class NmeaParser
{
public:
    // Returns true and fills fix if line is a valid $GPRMC/$GNRMC with status A
    static bool parseRmc(const QString &line, RmcFix &fix);

    // Convert RmcFix to UTC epoch
    static time_t toEpoch(const RmcFix &fix);

    // Maidenhead grid from lat/lon
    static QString toGrid(double lat, double lon);
};
