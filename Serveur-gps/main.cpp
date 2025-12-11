#include <QCoreApplication>
#include <QSerialPort>
#include <QDateTime>
#include <QDebug>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <thread>

// Convertit "ddmm.mm" en degrés décimaux
double nmeaToDecimal(const QString& degMin) {
    bool ok = false;
    double val = degMin.toDouble(&ok);
    if (!ok || val == 0.0) return 0.0;
    int deg = int(val / 100);
    double min = val - deg * 100;
    return deg + min / 60.0;
}

// Nettoie une chaîne NMEA
QString cleanNmeaField(const QString& field) {
    QString cleaned;
    for (QChar c : field) {
        if (c.isDigit() || c == '.') {
            cleaned.append(c);
        }
    }
    return cleaned.trimmed();
}

// Parse une trame GPGLL
bool parseGpgll(const QByteArray& line, double& latitude, double& longitude, QDateTime& timestamp) {
    if (!line.startsWith("$GPGLL")) return false;

    QByteArray core = line;
    int star = core.indexOf('*');
    if (star != -1) core = core.left(star);

    QStringList parts = QString::fromUtf8(core).split(',', Qt::KeepEmptyParts);
    if (parts.size() < 7) return false;

    QString latStr = cleanNmeaField(parts[1]);
    QChar latDir = parts[2].isEmpty() ? QChar() : parts[2].at(0);
    QString lonStr = cleanNmeaField(parts[3]);
    QChar lonDir = parts[4].isEmpty() ? QChar() : parts[4].at(0);
    QString timeStr = cleanNmeaField(parts[5]);
    QChar status = parts[6].isEmpty() ? QChar() : parts[6].at(0);

    if (status != 'A') return false;

    latitude = nmeaToDecimal(latStr);
    if (latDir == 'S') latitude = -latitude;

    longitude = nmeaToDecimal(lonStr);
    if (lonDir == 'W') longitude = -longitude;

    if (timeStr.length() == 6) {
        int h = timeStr.mid(0, 2).toInt();
        int m = timeStr.mid(2, 2).toInt();
        int s = timeStr.mid(4, 2).toInt();
        timestamp = QDateTime::currentDateTimeUtc();
        timestamp.setTime(QTime(h, m, s));
    }
    else {
        timestamp = QDateTime::currentDateTimeUtc();
    }

    return true;
}

void bddconect(QSqlDatabase &db)
{
    
    if (!db.open()) {
        qCritical() << "Erreur DB:" << db.lastError().text();
    }
    else {
        qDebug("Connecter a la bdd");
    }
}

void portconnect(QSerialPort &port)
{
    port.setPortName("COM5");
    port.setBaudRate(4800);
    if (!port.open(QIODevice::ReadOnly)) {
        qCritical() << "Erreur port serie:" << port.errorString();
    }
    else {
        qDebug("Connecter au port serie");
    }
}

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    QSqlDatabase db = QSqlDatabase::addDatabase("QMYSQL");
    QSerialPort port;

        while (!db.isOpen()||!port.isOpen())
        {
            if (!db.isOpen())
            {
                qDebug("connexion a la bdd");
                bddconect(db);
            }

            if (!port.isOpen())
            {
                qDebug("Connection au port serie");
                portconnect(port);
            }
        }
            
            QObject::connect(&port, &QSerialPort::readyRead, [&]() {
                static QByteArray buffer;
                buffer.append(port.readAll());
                int idx;
                while ((idx = buffer.indexOf('\n')) != -1) {
                    if (!db.isOpen())
                    {
                        qDebug("connexion a la bdd");
                        bddconect(db);
                    }

                    if (!port.isOpen())
                    {
                        qDebug("Connection au port serie");
                        portconnect(port);
                    }

                    QByteArray line = buffer.left(idx).trimmed();
                    buffer.remove(0, idx + 1);

                    double lat = 0.0, lon = 0.0;
                    QDateTime dt;

                    if (parseGpgll(line, lat, lon, dt)) {
                        QSqlQuery q;
                        q.prepare("INSERT INTO GPS (latitude, longitude, Date) VALUES (?, ?, ?)");
                        q.addBindValue(lat * 100);
                        q.addBindValue(lon * 100);
                        q.addBindValue(dt.toString("yyyy-MM-dd HH:mm:ss"));

                        if (!q.exec()) {
                            qWarning() << "Erreur insertion:" << q.lastError().text();
                        }
                        else {
                            qDebug() << "Coordonnees inserees en BDD:"
                                << "Latitude:" << lat * 100
                                << "Longitude:" << lon * 100
                                << "Heure UTC:" << dt.toString("yyyy-MM-dd HH:mm:ss");
                        }
                    }
                }
                });
    
    return app.exec();
    
}
