#ifndef EVEDATABASE_H
#define EVEDATABASE_H

#include <QString>
#include <QSqlRecord>
#include <QList>
#include <QMap>
#include <QHash>
#include <QSqlDatabase>
#include <QVariant>
#include <QVariantMap>
#include <QVector>
#include <QSqlQuery>
#include <QIcon>
#include <QObject>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QVariantHash>
#include <QSqlError>
#include <QDateTime>
#include <QTimer>
#include <QThread>
#include <QMutex>
#include <QMetaType>

#include "esibase.h"

typedef QSet<qint64> LongIDSet;
Q_DECLARE_METATYPE(LongIDSet)

class EveDB;
class EveDBWorker;
class EveMarketRegion;
class EveStructureManager;
class EveStructureMarket;

class EveStructure : public QObject
{
    Q_OBJECT
public:
    EveStructure(qint64 stru_id, QString stru_name, qint64 stru_loc, EveDBWorker* wor);
    ~EveStructure();
    QMap<QNetworkReply*, qint64> reps;
    QTimer *timer;
    EveDBWorker* worker;
    EsiBase *esi;
    QString name;
    qint64 finished = 0, pages = 0;
    QMap<int, QByteArray> results;
    qint64 location_id, structure_id, cycle_count = 1;
    bool do_update = true;
    QDateTime exp;

    void startUpdate();
    void restartUpdate();
    void downloadFinished();
    void downloadFailed(QNetworkReply::NetworkError code);
    void firstFinished();
    void firstFailed(QNetworkReply::NetworkError code);
    void finishUpdate();

signals:
    void sendRegion(qint64 reg_id, QString reg_name, qint64 cycle_count, QVariantList orderIDs,  QVariantList typeIDs,  QVariantList location_ids,  QVariantList volume_totals,  QVariantList volume_remains,  QVariantList volume_minimums,  QVariantList prices,  QVariantList is_buy_orders,  QVariantList durations,  QVariantList issueds,  QVariantList sale_ranges);
    void restart();
    void allFinished();

};

class EveStructureManager : public QObject
{
    Q_OBJECT
public:
    EveStructureManager();
    ~EveStructureManager();
    EsiBase *esi;
    QString my_table;
    LongIDSet old_structure_ids, structure_ids, upd_structure_ids;
    QMap<QNetworkReply*, qint64> reps;
    QMap<qint64, EveStructure*> citadels;
    QMap<qint64, EveDBWorker*> db_workers;
    QMap<qint64, QThread*> db_threads, web_threads;
    QTimer *timer;
    qint64 finished = 0;
    QDateTime exp;
public slots:
    void startUpdate();
    void downloadFailed(QNetworkReply::NetworkError code);
    void downloadFinished();
    void firstFinished();
    void firstFailed(QNetworkReply::NetworkError code);
    void receiveDelete(LongIDSet to_delete);
    void finishUpdate();

signals:
    void deleteStructures(LongIDSet to_delete);
    void allFinished();
    void updateAllStructures();
    void sendStructures(QVariantList structure_ids, QVariantList structure_names, QVariantList structure_locations);
};

class EveMarketRegion : public QObject
{
    Q_OBJECT
public:
    EveMarketRegion(EsiBase *esi_b, int reg_id, QString reg_name);
    qint64 region_id, cycle_count = 1;
    unsigned int finished, pages;
    QString region_name;
    QDateTime exp;
    EsiBase *esi;
    QMap<int, QByteArray> results;
    QHash<QNetworkReply*, int> reps;
    QTimer *timer;

public slots:
    void startUpdate();
    void restartUpdate();
    void downloadFinished();
    void downloadFailed(QNetworkReply::NetworkError code);
    void firstFinished();
    void firstFailed(QNetworkReply::NetworkError code);
    void finishUpdate();

signals:
    void sendRegion(qint64 reg_id, QString reg_name, qint64 cycle_count, QVariantList orderIDs,  QVariantList typeIDs,  QVariantList location_ids,  QVariantList volume_totals,  QVariantList volume_remains,  QVariantList volume_minimums,  QVariantList prices,  QVariantList is_buy_orders,  QVariantList durations,  QVariantList issueds,  QVariantList sale_ranges);
    void restart();
    void allFinished();

};

class EveDBWorker : public QObject
{
    Q_OBJECT
public:
    EveDBWorker(qint64 w_id = 0);
    qint64 worker_id;

public slots:
    void receiveRegion(qint64 reg_id, QString reg_name, qint64 cycle_count, QVariantList orderIDs,  QVariantList typeIDs,  QVariantList location_ids,  QVariantList volume_totals,  QVariantList volume_remains,  QVariantList volume_minimums,  QVariantList prices,  QVariantList is_buy_orders,  QVariantList durations,  QVariantList issueds,  QVariantList sale_ranges);
    void receiveStructures(QVariantList structure_ids, QVariantList structure_names, QVariantList structure_locations);

signals:
    void finishedRegion();
    void finishedStructures();

private:
    bool is_setup = 0;
    void setup();
    QSqlDatabase db;
};

class EveDB : public QObject {
    Q_OBJECT
public:
    EveDB();
    ~EveDB();
    QList<QThread*> db_threads, req_threads;
    QList<EveDBWorker*> db_workers;
    QSqlDatabase db;
    EsiBase *esi;
    QThread *st_thread, *stb_thread;
    EveStructureManager *st_manager;
    EveDBWorker *st_worker;
    unsigned int req_threadpool_size = 67, db_threadpool_size = 67;
    QList<EveMarketRegion*> regions;
    unsigned int finished = 0;
    void loadRegions();


public slots:
    void regionDone();

signals:
    void updateAllRegions();
};

#endif // EVEDATABASE_H
