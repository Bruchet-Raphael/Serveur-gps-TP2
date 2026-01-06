#include <QCoreApplication>
#include <QSerialPort>
#include <QDebug>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>

// Convertit "ddmm.mm" en degrés décimaux
double nmeaToDecimal(const QString& degMin, bool isLatitude) {
    bool ok = false;
    double val = degMin.toDouble(&ok);
    if (!ok || val == 0.0) return 0.0;

    int deg = int(val / 100);
    double min = val - deg * 100;
    double decimal = deg + min / 60.0;

    // Validation : latitude <= 90, longitude <= 180
    if (isLatitude && (decimal < -90.0 || decimal > 90.0)) return 0.0;
    if (!isLatitude && (decimal < -180.0 || decimal > 180.0)) return 0.0;

    return decimal;
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

// Parse une trame GPGGA (latitude/longitude uniquement)
bool parseGpgga(const QByteArray& line, double& latitude, double& longitude) {
    if (!line.startsWith("$GPGGA")) return false;

    QByteArray core = line;
    int star = core.indexOf('*');
    if (star != -1) core = core.left(star);

    QStringList parts = QString::fromUtf8(core).split(',', Qt::KeepEmptyParts);
    if (parts.size() < 6) return false;

    QString latStr = cleanNmeaField(parts[2]);
    QChar latDir = parts[3].isEmpty() ? QChar() : parts[3].at(0);
    QString lonStr = cleanNmeaField(parts[4]);
    QChar lonDir = parts[5].isEmpty() ? QChar() : parts[5].at(0);
    QString fixStr = cleanNmeaField(parts[6]);

    // Vérifie que le GPS a un FIX
    if (fixStr.toInt() == 0) return false;

    // Conversion latitude / longitude avec validation
    latitude = nmeaToDecimal(latStr, true);
    if (latDir == 'S') latitude = -latitude;

    longitude = nmeaToDecimal(lonStr, false);
    if (lonDir == 'W') longitude = -longitude;

    // Si coordonnées invalides, on rejette
    if (latitude == 0.0 || longitude == 0.0) return false;

    return true;
}

// Connexion à la base de données
void bddconect(QSqlDatabase& db) {
    db.setHostName("172.29.19.33");
    db.setPort(3306);
    db.setDatabaseName("TpProjet2");
    db.setUserName("BDDGPhess");
    db.setPassword("t(vj-sA2@sIhnlCB");
    if (!db.open()) {
        qCritical() << "Erreur DB:" << db.lastError().text();
    }
    else {
        qDebug("Connecté à la BDD");
    }
}

// Connexion au port série
void portconnect(QSerialPort& port) {
    port.setPortName("COM5");
    port.setBaudRate(9600);
    if (!port.open(QIODevice::ReadOnly)) {
        qCritical() << "Erreur port série:" << port.errorString();
    }
    else {
        qDebug("Connecté au port série");
    }
}

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    QSqlDatabase db = QSqlDatabase::addDatabase("QMYSQL");
    QSerialPort port;

    // Boucle de connexion
    while (!db.isOpen() || !port.isOpen()) {
        if (!db.isOpen()) {
            qDebug("Connexion à la BDD...");
            bddconect(db);
        }
        if (!port.isOpen()) {
            qDebug("Connexion au port série...");
            portconnect(port);
        }
    }

    // Lecture des trames
    QObject::connect(&port, &QSerialPort::readyRead, [&]() {
        static QByteArray buffer;
        buffer.append(port.readAll());
        int idx;
        while ((idx = buffer.indexOf('\n')) != -1) {
            QByteArray line = buffer.left(idx).trimmed();
            buffer.remove(0, idx + 1);

            double lat = 0.0, lon = 0.0;

            if (parseGpgga(line, lat, lon)) {
                QSqlQuery q;
                q.prepare("INSERT INTO GPS (latitude, longitude) VALUES (?, ?)");
                q.addBindValue(lat);
                q.addBindValue(lon);

                if (!q.exec()) {
                    qWarning() << "Erreur insertion:" << q.lastError().text();
                }
                else {
                    qDebug() << "Coordonnées insérées en BDD:"
                        << "Latitude:" << lat
                        << "Longitude:" << lon;
                }
            }
            else {
                qWarning() << "Trame ignorée (coordonnées invalides):" << line;
            }
        }
        });

    return app.exec();
}
