#include <QCoreApplication>
#include <QSerialPort>
#include <QDateTime>
#include <QDebug>

// Convertit "ddmm.mm" en degrés décimaux
double nmeaToDecimal(const QString& degMin) {
    bool ok = false;
    double val = degMin.toDouble(&ok);
    if (!ok) return 0.0;
    int deg = int(val / 100);
    double min = val - deg * 100;
    return deg + min / 60.0;
}

// Parse une trame GPGLL
bool parseGpgll(const QByteArray& line, double& latitude, double& longitude, QDateTime& timestamp) {
    if (!line.startsWith("$GPGLL")) return false;

    QByteArray core = line;
    int star = core.indexOf('*');
    if (star != -1) core = core.left(star);

    QStringList parts = QString::fromUtf8(core).split(',', Qt::KeepEmptyParts);
    if (parts.size() < 7) return false;

    QString latStr = parts[1];
    QChar latDir = parts[2].isEmpty() ? QChar() : parts[2].at(0);
    QString lonStr = parts[3];
    QChar lonDir = parts[4].isEmpty() ? QChar() : parts[4].at(0);
    QString timeStr = parts[5];
    QChar status = parts[6].isEmpty() ? QChar() : parts[6].at(0);

    if (status != 'A') return false;

    latitude = nmeaToDecimal(latStr);
    if (latDir == 'S') latitude = -latitude;

    longitude = nmeaToDecimal(lonStr);
    if (lonDir == 'W') longitude = -longitude;

    if (timeStr.length() != 6) return false;
    int h = timeStr.mid(0, 2).toInt();
    int m = timeStr.mid(2, 2).toInt();
    int s = timeStr.mid(4, 2).toInt();
    timestamp = QDateTime::currentDateTimeUtc();
    timestamp.setTime(QTime(h, m, s));

    return true;
}

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);

    // Port série
    QSerialPort port;
    port.setPortName("COM3");
    port.setBaudRate(4800);
    if (!port.open(QIODevice::ReadOnly)) {
        qCritical() << "Erreur port serie:" << port.errorString();
        return 1;
    }

    QObject::connect(&port, &QSerialPort::readyRead, [&]() {
        static QByteArray buffer;
        buffer.append(port.readAll());

        int idx;
        while ((idx = buffer.indexOf('\n')) != -1) {
            QByteArray line = buffer.left(idx).trimmed();
            buffer.remove(0, idx + 1);

            double lat = 0.0, lon = 0.0;
            QDateTime dt;

            if (parseGpgll(line, lat, lon, dt)) {
                qDebug() << "Coordonnees recues:"
                    << "Latitude:" << lat
                    << "Longitude:" << lon
                    << "Heure UTC:" << dt.toString("yyyy-MM-dd HH:mm:ss");
            }
        }
        });

    return app.exec();
}
