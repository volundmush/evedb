#include "evedatabase.h"

EveStructure::EveStructure(qint64 stru_id, QString stru_name, qint64 stru_loc, EveDBWorker *wor)
{
    esi = new EsiBase(this);
    structure_id = stru_id;
    name = stru_name;
    location_id = stru_loc;
    worker = wor;
    timer = new QTimer(this);
    startUpdate();
    connect(this, &EveStructure::allFinished, &EveStructure::finishUpdate);
    connect(this, &EveStructure::restart, this, &EveStructure::restartUpdate);
    connect(timer, &QTimer::timeout, this, &EveStructure::startUpdate);
}

EveStructure::~EveStructure()
{
    for(auto r : reps.keys())
    {
        r->abort();
    }
}

void EveStructure::startUpdate()
{
    finished = 0;
    qDebug() << "EveStructure<" << QString::number(structure_id) << ", " << name << "> Starting Update: "  << QDateTime::currentDateTime().toString();
    QNetworkReply *rep = esi->getMarketsStructure(structure_id, 1);
    reps.insert(rep, 1);
    connect(rep, &QNetworkReply::finished, this, &EveStructure::firstFinished);
    connect(rep, static_cast<void(QNetworkReply::*)(QNetworkReply::NetworkError)>(&QNetworkReply::error),[=](QNetworkReply::NetworkError code){this->firstFailed(code);});
    connect(rep, &QNetworkReply::finished, this, &EveStructure::downloadFinished);
    connect(rep, static_cast<void(QNetworkReply::*)(QNetworkReply::NetworkError)>(&QNetworkReply::error),[=](QNetworkReply::NetworkError code){this->downloadFailed(code);});

}

void EveStructure::firstFinished()
{
    auto *rep = dynamic_cast<QNetworkReply*>(sender());
    QByteArray ph("x-pages"), ex("expires");
    QString datestring(rep->rawHeader(ex));
    exp = QDateTime::fromString(datestring.left(25),"ddd, dd MMM yyyy hh:mm:ss");
    exp.setTimeSpec(Qt::UTC);
    qint64 nextsecs = exp.currentDateTimeUtc().msecsTo(exp);
    if(nextsecs<25000)
    {
        qDebug() << "EveStructure<" << QString::number(structure_id) << ", " << name << "> First Response get. Expires in: " << QString::number(nextsecs) << " milliseconds. " << exp.toLocalTime() << " Received: " << QDateTime::currentDateTime().toString() << " Restarting!";
        restartUpdate();
        return;
    }
    //timer->start(nextsecs + 30000);
    qDebug() << "EveStructure<" << QString::number(structure_id) << ", " << name << "> First Response get. Expires in: " << QString::number(nextsecs) << " milliseconds. " << exp.toLocalTime() << " Received: " << QDateTime::currentDateTime().toString();

    if(rep->hasRawHeader(ph))
    {
        QString pstr(rep->rawHeader(ph));
        int num_pages = pstr.toInt();
        pages = num_pages;
        qDebug() << "EveStructure<" << QString::number(structure_id) << ", " << name << "> First Response get. Pages: " << QString::number(num_pages);
        if(num_pages>1)
        {
            for (int i = 2; i <= num_pages; i++)
            {
                QNetworkReply *rep2 = esi->getMarketsStructure(structure_id, i);
                reps.insert(rep2, i);
                connect(rep2, &QNetworkReply::finished, this, &EveStructure::downloadFinished);
                connect(rep2, static_cast<void(QNetworkReply::*)(QNetworkReply::NetworkError)>(&QNetworkReply::error),[=](QNetworkReply::NetworkError code){this->downloadFailed(code);});
            }
        }
    }
}

void EveStructure::firstFailed(QNetworkReply::NetworkError code)
{
    //qDebug() << "EveStructure<" << QString::number(structure_id) << ", " << name << "> Failed Page " << QString::number(page) << " of " << QString::number(pages) << "- Retrying - " << QDateTime::currentDateTime().toString();
    startUpdate();
}

void EveStructure::restartUpdate()
{
    for (auto rep: reps.keys())
    {
        disconnect(rep, 0, this, 0);
        rep->abort();
    }
    reps.clear();
    results.clear();
    timer->start(25000);
}

void EveStructure::downloadFailed(QNetworkReply::NetworkError code)
{
    auto *rep = dynamic_cast<QNetworkReply*>(sender());
    int page = reps.value(rep);
    if(page==1)
    {
        qDebug() << "EveStructure<" << QString::number(structure_id) << ", " << name << "> Failed Page " << QString::number(page) << " of " << QString::number(pages) << "- Restarting - " << QDateTime::currentDateTime().toString();
        return;
    }
    QUrl url = rep->url();
    QNetworkRequest req(url);
    QNetworkReply *new_rep = esi->manager->get(req);
    reps.remove(rep);
    reps.insert(new_rep, page);
    qDebug() << "EveStructure<" << QString::number(structure_id) << ", " << name << "> Failed Page " << QString::number(page) << " of " << QString::number(pages) << "- Retrying - " << QDateTime::currentDateTime().toString();
}

void EveStructure::downloadFinished()
{
    auto *rep = dynamic_cast<QNetworkReply*>(sender());
    int page = reps.value(rep);
    QByteArray ph("x-pages"), ex("expires");
    QString datestring(rep->rawHeader(ex));
    QDateTime checkdate = QDateTime::fromString(datestring.left(25),"ddd, dd MMM yyyy hh:mm:ss");
    checkdate.setTimeSpec(Qt::UTC);
    if(exp != checkdate)
    {
        qDebug() << "EveStructure<" << QString::number(structure_id) << ", " << name << "> Received Page " << QString::number(page) << " of " << QString::number(pages) << " Expiry Passed, restarting - " << QDateTime::currentDateTime().toString();
        restartUpdate();
        return;
    }

    results.insert(page, rep->readAll());
    qDebug() << "EveStructure<" << QString::number(structure_id) << ", " << name << "> Received Page " << QString::number(page) << " of " << QString::number(pages) << " - " << QDateTime::currentDateTime().toString();
    finished++;
    if(finished==pages)
    {
        emit allFinished();
    }
}

void EveStructure::finishUpdate()
{
    qDebug() << "EveStructure<" << QString::number(structure_id) << ", " << name << "> Downloaded All Pages. Processing..." << QDateTime::currentDateTime().toString();
    finished = 0;
    QVariantList orderIDs, typeIDs, location_ids, volume_totals, volume_remains, volume_minimums, prices, is_buy_orders, durations, issueds, sale_ranges;

    for (auto result : results.values())
    {
        auto jsa = QJsonDocument::fromJson(result).array();

        for (auto order : jsa)
        {
            auto ord = order.toObject().toVariantMap();
            orderIDs.append(ord.value("order_id"));
            typeIDs.append(ord.value("type_id"));
            location_ids.append(ord.value("location_id"));
            volume_totals.append(ord.value("volume_total"));
            volume_remains.append(ord.value("volume_remain"));
            volume_minimums.append(ord.value("min_volume"));
            prices.append(ord.value("price"));
            is_buy_orders.append(ord.value("is_buy_order"));
            durations.append(ord.value("duration"));
            issueds.append(ord.value("issued").toDateTime());
            sale_ranges.append(ord.value("range"));
        }
    }
    qDebug() << "EveStructure<" << QString::number(structure_id) << ", " << name << "> Downloaded All Pages. Orders: " << QString::number(orderIDs.size()) << " - " << QDateTime::currentDateTime().toString();
    reps.clear();
    results.clear();
    emit sendRegion(structure_id, name, cycle_count, orderIDs, typeIDs, location_ids, volume_totals, volume_remains, volume_minimums, prices, is_buy_orders, durations, issueds, sale_ranges);
    cycle_count++;
}

EveStructureManager::EveStructureManager()
{
    esi = new EsiBase(this);
    timer = new QTimer(this);
    connect(this, &EveStructureManager::deleteStructures, this, &EveStructureManager::receiveDelete);
    connect(this, &EveStructureManager::allFinished, this, &EveStructureManager::finishUpdate);
    connect(timer, &QTimer::timeout, this, &EveStructureManager::startUpdate);
}

EveStructureManager::~EveStructureManager()
{
    for(auto r : reps.keys())
    {
        r->abort();
        delete r;
    }
    reps.clear();

    for(auto i : db_threads.values())
    {
        i->quit();
        i->wait();
        delete i;
    }
    db_threads.clear();

    for(auto i : web_threads.values())
    {
        i->quit();
        i->wait();
        delete i;
    }
    web_threads.clear();

    timer->stop();
    delete timer;
    delete esi;

}

void EveStructureManager::startUpdate()
{
    qDebug() << "EveStructureManager: Starting Update";
    QNetworkReply *rep = esi->getUniverseStructures();
    connect(rep, &QNetworkReply::finished, this, &EveStructureManager::firstFinished);
    connect(rep, static_cast<void(QNetworkReply::*)(QNetworkReply::NetworkError)>(&QNetworkReply::error),[=](QNetworkReply::NetworkError code){this->firstFailed(code);});
}

void EveStructureManager::firstFinished()
{
    auto *rep = dynamic_cast<QNetworkReply*>(sender());
    old_structure_ids = structure_ids;
    auto jresp = QJsonDocument::fromJson(rep->readAll()).array().toVariantList();
    structure_ids.clear();
    for(auto jr: jresp)
    {
        structure_ids.insert(jr.toLongLong());
    }
    LongIDSet gone_structure_ids = old_structure_ids - structure_ids;
    emit deleteStructures(gone_structure_ids);

    qDebug() << "EveStructureManager: " << QString::number(structure_ids.size()) << "Structures detected. Starting Update: "  << QDateTime::currentDateTime().toString();
    QByteArray ph("x-pages"), ex("expires");
    QString datestring(rep->rawHeader(ex));
    exp = QDateTime::fromString(datestring.left(25),"ddd, dd MMM yyyy hh:mm:ss");
    exp.setTimeSpec(Qt::UTC);
    qint64 nextsecs = exp.currentDateTimeUtc().msecsTo(exp);
    if(nextsecs<25000)
    {
        nextsecs = 25000;
    }
    timer->start(nextsecs);

    finished = 0;

    for (auto i: structure_ids)
    {
        QNetworkReply *rep2 = esi->getUniverseStructure(i);
        reps.insert(rep2, i);
        connect(rep2, &QNetworkReply::finished, this, &EveStructureManager::downloadFinished);
        connect(rep2, static_cast<void(QNetworkReply::*)(QNetworkReply::NetworkError)>(&QNetworkReply::error),[=](QNetworkReply::NetworkError code){this->downloadFailed(code);});
    }

}

void EveStructureManager::downloadFinished()
{
    auto *rep = dynamic_cast<QNetworkReply*>(sender());
    auto jresp = QJsonDocument::fromBinaryData(rep->readAll()).object().toVariantMap();
    qint64 st_id = reps.value(rep);
    QString st_name = jresp.value("name").toString();
    qint64 loc_id = jresp.value("solar_system_id").toLongLong();

    EveStructure *found_cit;
    EveDBWorker *work;
    QThread *found_thread;

    if(citadels.contains(st_id))
    {
        found_cit = citadels.value(st_id);
        if(st_name != found_cit->name)
        {
            found_cit->name = st_name;
            found_cit->do_update;
        }
    }
    else
    {
        work = new EveDBWorker(st_id);
        db_workers.insert(st_id, work);

        found_cit = new EveStructure(st_id, st_name, loc_id, work);
        citadels.insert(st_id, found_cit);

        found_thread = new QThread();
        db_threads.insert(st_id, found_thread);
        found_thread->start();
        work->moveToThread(found_thread);

        found_thread = new QThread();
        found_thread->start();
        web_threads.insert(st_id, found_thread);

        found_cit->moveToThread(found_thread);
        connect(found_cit, &EveStructure::sendRegion, work, &EveDBWorker::receiveRegion);
    }
    finished++;
    qDebug() << "EveStructureManager: Received Structure " << QString::number(finished) << " of " << QString::number(structure_ids.size()) << QDateTime::currentDateTime().toString();
    if(finished == structure_ids.size())
    {
        emit allFinished();
    }
}

void EveStructureManager::finishUpdate()
{
    QVariantList structure_ids2, structure_names, structure_locations;

    for(auto st : citadels.values())
    {
        if(st->do_update)
        {
            structure_ids2.append(st->structure_id);
            structure_names.append(st->name);
            structure_locations.append(st->location_id);
        }
    }
    emit sendStructures(structure_ids2, structure_names, structure_locations);
}

void EveStructureManager::receiveDelete(LongIDSet to_delete)
{
    for(auto i : to_delete)
    {
        delete citadels.value(i);

        delete db_workers.value(i);

        db_threads.value(i)->quit();
        db_threads.value(i)->wait();
        delete db_threads.value(i);

        web_threads.value(i)->quit();
        web_threads.value(i)->wait();
        delete web_threads.value(i);


    }
}

void EveStructureManager::firstFailed(QNetworkReply::NetworkError code)
{
    //pass
}

void EveStructureManager::downloadFailed(QNetworkReply::NetworkError code)
{
    auto *rep = dynamic_cast<QNetworkReply*>(sender());
    int page = reps.value(rep);
    QUrl url = rep->url();
    QNetworkRequest req(url);
    QNetworkReply *new_rep = esi->manager->get(req);
    reps.remove(rep);
    reps.insert(new_rep, page);
}

EveMarketRegion::EveMarketRegion(EsiBase *esi_b, int reg_id, QString reg_name)
{
    region_id = reg_id;
    region_name = reg_name;
    esi = new EsiBase(this);
    timer = new QTimer(this);
    timer->setSingleShot(true);
    connect(this, &EveMarketRegion::allFinished, this, &EveMarketRegion::finishUpdate);
    connect(this, &EveMarketRegion::restart, this, &EveMarketRegion::startUpdate);
    connect(timer, &QTimer::timeout, this, &EveMarketRegion::startUpdate);

}

void EveMarketRegion::startUpdate()
{
    finished = 0;
    qDebug() << "EveMarketRegion<" << QString::number(region_id) << ", " << region_name << "> Starting Update: "  << QDateTime::currentDateTime().toString();
    QNetworkReply *rep = esi->getMarketsRegionOrders(region_id, 1);
    reps.insert(rep, 1);
    connect(rep, &QNetworkReply::finished, this, &EveMarketRegion::firstFinished);
    connect(rep, static_cast<void(QNetworkReply::*)(QNetworkReply::NetworkError)>(&QNetworkReply::error),[=](QNetworkReply::NetworkError code){this->firstFailed(code);});
    connect(rep, &QNetworkReply::finished, this, &EveMarketRegion::downloadFinished);
    connect(rep, static_cast<void(QNetworkReply::*)(QNetworkReply::NetworkError)>(&QNetworkReply::error),[=](QNetworkReply::NetworkError code){this->downloadFailed(code);});

}

void EveMarketRegion::restartUpdate()
{
    for (auto rep: reps.keys())
    {
        disconnect(rep, 0, this, 0);
        rep->abort();
    }
    reps.clear();
    results.clear();
    timer->start(25000);
}

void EveMarketRegion::firstFinished()
{
    auto *rep = dynamic_cast<QNetworkReply*>(sender());
    QByteArray ph("x-pages"), ex("expires");
    QString datestring(rep->rawHeader(ex));
    exp = QDateTime::fromString(datestring.left(25),"ddd, dd MMM yyyy hh:mm:ss");
    exp.setTimeSpec(Qt::UTC);
    qint64 nextsecs = exp.currentDateTimeUtc().msecsTo(exp);
    if(nextsecs<25000)
    {
        qDebug() << "EveMarketRegion<" << QString::number(region_id) << ", " << region_name << "> First Response get. Expires in: " << QString::number(nextsecs) << " milliseconds. " << exp.toLocalTime() << " Received: " << QDateTime::currentDateTime().toString() << " Restarting!";
        restartUpdate();
        return;
    }
    timer->start(nextsecs + 30000);
    qDebug() << "EveMarketRegion<" << QString::number(region_id) << ", " << region_name << "> First Response get. Expires in: " << QString::number(nextsecs) << " milliseconds. " << exp.toLocalTime() << " Received: " << QDateTime::currentDateTime().toString();

    if(rep->hasRawHeader(ph))
    {
        QString pstr(rep->rawHeader(ph));
        int num_pages = pstr.toInt();
        pages = num_pages;
        qDebug() << "EveMarketRegion<" << QString::number(region_id) << ", " << region_name << "> First Response get. Pages: " << QString::number(num_pages);
        if(num_pages>1)
        {
            for (int i = 2; i <= num_pages; i++)
            {
                QNetworkReply *rep2 = esi->getMarketsRegionOrders(region_id, i);
                reps.insert(rep2, i);
                connect(rep2, &QNetworkReply::finished, this, &EveMarketRegion::downloadFinished);
                connect(rep2, static_cast<void(QNetworkReply::*)(QNetworkReply::NetworkError)>(&QNetworkReply::error),[=](QNetworkReply::NetworkError code){this->downloadFailed(code);});
            }
        }
    }
}

void EveMarketRegion::downloadFinished()
{
    auto *rep = dynamic_cast<QNetworkReply*>(sender());
    int page = reps.value(rep);
    QByteArray ph("x-pages"), ex("expires");
    QString datestring(rep->rawHeader(ex));
    QDateTime checkdate = QDateTime::fromString(datestring.left(25),"ddd, dd MMM yyyy hh:mm:ss");
    checkdate.setTimeSpec(Qt::UTC);
    if(exp != checkdate)
    {
        qDebug() << "EveMarketRegion<" << QString::number(region_id) << ", " << region_name << "> Received Page " << QString::number(page) << " of " << QString::number(pages) << " Expiry Passed, restarting - " << QDateTime::currentDateTime().toString();
        restartUpdate();
        return;
    }

    results.insert(page, rep->readAll());
    qDebug() << "EveMarketRegion<" << QString::number(region_id) << ", " << region_name << "> Received Page " << QString::number(page) << " of " << QString::number(pages) << " - " << QDateTime::currentDateTime().toString();
    finished++;
    if(finished==pages)
    {
        emit allFinished();
    }
}

void EveMarketRegion::downloadFailed(QNetworkReply::NetworkError code)
{
    auto *rep = dynamic_cast<QNetworkReply*>(sender());
    int page = reps.value(rep);
    if(page==1)
    {
        qDebug() << "EveMarketRegion<" << QString::number(region_id) << ", " << region_name << "> Failed Page " << QString::number(page) << " of " << QString::number(pages) << "- Restarting - " << QDateTime::currentDateTime().toString();
        return;
    }
    QUrl url = rep->url();
    QNetworkRequest req(url);
    QNetworkReply *new_rep = esi->manager->get(req);
    reps.remove(rep);
    reps.insert(new_rep, page);
    qDebug() << "EveMarketRegion<" << QString::number(region_id) << ", " << region_name << "> Failed Page " << QString::number(page) << " of " << QString::number(pages) << "- Retrying - " << QDateTime::currentDateTime().toString();
}

void EveMarketRegion::firstFailed(QNetworkReply::NetworkError code)
{
    //qDebug() << "EveMarketRegion<" << QString::number(region_id) << ", " << region_name << "> Failed Page " << QString::number(page) << " of " << QString::number(pages) << "- Retrying - " << QDateTime::currentDateTime().toString();
    startUpdate();
}

void EveMarketRegion::finishUpdate()
{
    qDebug() << "EveMarketRegion<" << QString::number(region_id) << ", " << region_name << "> Downloaded All Pages. Processing..." << QDateTime::currentDateTime().toString();
    finished = 0;
    QVariantList orderIDs, typeIDs, location_ids, volume_totals, volume_remains, volume_minimums, prices, is_buy_orders, durations, issueds, sale_ranges;

    for (auto result : results.values())
    {
        auto jsa = QJsonDocument::fromJson(result).array();

        for (auto order : jsa)
        {
            auto ord = order.toObject().toVariantMap();
            orderIDs.append(ord.value("order_id"));
            typeIDs.append(ord.value("type_id"));
            location_ids.append(ord.value("location_id"));
            volume_totals.append(ord.value("volume_total"));
            volume_remains.append(ord.value("volume_remain"));
            volume_minimums.append(ord.value("min_volume"));
            prices.append(ord.value("price"));
            is_buy_orders.append(ord.value("is_buy_order"));
            durations.append(ord.value("duration"));
            issueds.append(ord.value("issued").toDateTime());
            sale_ranges.append(ord.value("range"));
        }
    }
    qDebug() << "EveMarketRegion<" << QString::number(region_id) << ", " << region_name << "> Downloaded All Pages. Orders: " << QString::number(orderIDs.size()) << " - " << QDateTime::currentDateTime().toString();
    reps.clear();
    results.clear();
    emit sendRegion(region_id, region_name, cycle_count, orderIDs, typeIDs, location_ids, volume_totals, volume_remains, volume_minimums, prices, is_buy_orders, durations, issueds, sale_ranges);
    cycle_count++;
}

EveDBWorker::EveDBWorker(qint64 w_id)
{
    worker_id = w_id;

}

void EveDBWorker::setup()
{
    if(is_setup)
        return;
    db = QSqlDatabase::addDatabase("QMYSQL", "worker_" + QString::number(worker_id));
    db.setHostName("localhost");
    db.setDatabaseName("evecalc");
    db.setUserName("root");
    db.setPassword("Draculina7");
    is_setup = true;
}

EveDB::EveDB()
{

    db = QSqlDatabase::addDatabase("QMYSQL");
    db.setHostName("localhost");
    db.setDatabaseName("evecalc");
    db.setUserName("root");
    db.setPassword("Draculina7");
    db.open();

    esi = new EsiBase(this);

    QSqlQuery q(db);
    db.transaction();
    q.exec("DELETE from MarketOrders");
    q.exec("DELETE FROM Structures");
    q.exec("DELETE FROM DockableMarkets");

    q.exec("INSERT INTO MarketRegions (regionID,regionName) SELECT regionID,regionName FROM evesde.mapRegions WHERE regionID<11000001");

    q.exec("INSERT INTO MarketConstellations (constellationID,constellationName,regionID) SELECT constellationID,constellationName,regionID FROM evesde.mapConstellations WHERE regionID IN (SELECT regionID FROM MarketRegions)");

    q.exec("INSERT INTO MarketSystems (solarSystemID,solarSystemName,security,constellationID) SELECT solarSystemID,solarSystemName,security,constellationID FROM evesde.mapSolarSystems WHERE constellationID IN (SELECT constellationID FROM MarketConstellations)");

    q.exec("INSERT INTO DockableMarkets (locationID,stationName,solarSystemID,source_type) SELECT stationID,stationName,solarSystemID,0 FROM v_MarketLocations");

    db.commit();


    EveDBWorker *new_worker;
    QThread *new_thread;
    for(int i = 0; i <= db_threadpool_size; i++)
    {
        new_thread = new QThread(this);
        db_threads.append(new_thread);
        new_worker = new EveDBWorker(i);
        db_workers.append(new_worker);
        new_worker->moveToThread(new_thread);
        new_thread->start();
        connect(new_worker, &EveDBWorker::finishedRegion, this, &EveDB::regionDone);
    }

    for(int i = 0; i <= req_threadpool_size; i++)
    {
        new_thread = new QThread(this);
        req_threads.append(new_thread);
        new_thread->start();
    }

    /*
    st_thread = new QThread(this);
    st_thread->start();
    st_worker = new EveDBWorker(0);
    st_manager = new EveStructureManager();
    st_worker->moveToThread(st_thread);
    stb_thread = new QThread(this);
    stb_thread->start();
    st_manager->moveToThread(stb_thread);
    connect(st_manager, &EveStructureManager::sendStructures, st_worker, &EveDBWorker::receiveStructures);
    connect(this, &EveDB::updateAllRegions, st_manager, &EveStructureManager::startUpdate);
    */
    loadRegions();
}

EveDB::~EveDB()
{
    for (auto thr : db_threads)
    {
        thr->quit();
    }
    for (auto thr : req_threads)
    {
        thr->quit();
    }
}

void EveDB::loadRegions ()
{
    QSqlQuery q(db);
    db.transaction();
    q.exec("SELECT regionID,regionName FROM evesde.mapRegions WHERE regionID<11000001");
    //q.exec("SELECT regionID,regionName FROM evesde.mapRegions WHERE regionID=10000002");
    unsigned int db_thread_id = 0, req_thread_id = 0;
    EveDBWorker *found_worker;
    QThread *found_thread;

    QStringList tablenames;

    while (q.next()) {
        QSqlRecord rec = q.record();
        qint64 reg_id = rec.value(0).toLongLong();
        QString reg_name = rec.value(1).toString();
        QString my_table = "MarketOrders_" + QString::number(reg_id);
        tablenames.append(my_table);

        EveMarketRegion *new_reg = new EveMarketRegion(esi, reg_id, reg_name);
        regions.append(new_reg);
        found_thread = req_threads.at(req_thread_id);
        found_worker = db_workers.at(db_thread_id);
        new_reg->moveToThread(found_thread);

        connect(this, &EveDB::updateAllRegions, new_reg, &EveMarketRegion::startUpdate);
        connect(new_reg, &EveMarketRegion::sendRegion, found_worker, &EveDBWorker::receiveRegion);
        db_thread_id++, req_thread_id++;
        if(db_thread_id>db_threadpool_size)
        {
            db_thread_id = 0;
        }
        if(req_thread_id>req_threadpool_size)
        {
            req_thread_id = 0;
        }
    }

    for(auto tab : tablenames)
    {
        q.exec("INSERT INTO " + tab + " (orderID,typeID,location_id,volume_total,volume_remain,volume_minimum,price,is_buy_order,duration,issued,sale_range) VALUES (:orderID,:typeID,:location_id,:volume_total,:volume_remain,:volume_minimum,:price,:is_buy_order,:duration,:issued,:sale_range) ON DUPLICATE KEY UPDATE price=VALUES(price),issued=VALUES(issued)");
    }

    db.commit();
    db.close();
    emit updateAllRegions();
}

void EveDB::regionDone()
{
    finished++;
    qDebug() << "EvDB: Updated Regions: " << QString::number(finished);
}


void EveDBWorker::receiveStructures(QVariantList structure_ids, QVariantList structure_names, QVariantList structure_locations)
{
    db.open();
    qDebug() << "EveDB: Updating Structures. Beginning transaction: " << QString::number(structure_ids.size()) << " orders. DB IS Open: " << db.isOpen();
    QSqlQuery q(db);
    db.transaction();

    if(structure_ids.size() == 0)
    {
        // It would be kind of silly to do all the below code if there were no orders.
        // If 0 orders, clear the Market and call it good.
        db.transaction();
        q.prepare("DELETE FROM Structures");
        q.exec();
        db.commit();
        db.close();
        emit finishedStructures();
        return;
    }

    q.exec("CREATE TEMPORARY TABLE Structures_Temp structureID BIGINT UNSIGNED NOT NULL,structureName VARCHAR(100),solarSystemID BIGINT UNSIGNED NOT NULL,PRIMARY KEY(structureID)) ENGINE=MEMORY DEFAULT CHARSET=utf8 COLLATE=utf8_unicode_ci");

    q.prepare("INSERT INTO Structures_Temp (structureID,structureName,solarSystemID) VALUES (:structureID,:structureName,:solarSystemID) ON DUPLICATE KEY UPDATE structureName=VALUES(structureName)");

    q.bindValue(":structureID", structure_ids);
    q.bindValue(":structureName", structure_names);
    q.bindValue(":solarSystemID", structure_locations);

    qDebug() << "EveDB: Insert Lists bound: " << QDateTime::currentDateTime().toString() << " Size: " << structure_ids.size();

    if (!q.execBatch())
        qDebug() << q.lastError();

    qDebug() << "EveDB: Inserts finished: " << QDateTime::currentDateTime().toString();

    db.commit();
    qDebug() << "EvDB: Updating Structures Transaction to microtable finished.";

    db.transaction();
    q.prepare("DELETE FROM Structures WHERE structureID NOT IN (SELECT structureID FROM Structures_Temp)");
    q.exec();

    q.prepare("INSERT INTO Structures (structureID,structureName,solarSystemID) SELECT structureID,structureName as newstructureName,solarSystemID FROM Structures_Temp ON DUPLICATE KEY UPDATE structureName=newstructureName");
    q.exec();
    q.exec("DROP TEMPORARY TABLE Structures_Temp");
    db.commit();

    //finished++;
    //qDebug() << "EvDB: Updated Structure: " << QString::number(finished);
    emit finishedStructures();
    db.close();

}


void EveDBWorker::receiveRegion(qint64 reg_id, QString reg_name, qint64 cycle_count, QVariantList orderIDs,  QVariantList typeIDs,  QVariantList location_ids,  QVariantList volume_totals,  QVariantList volume_remains,  QVariantList volume_minimums,  QVariantList prices,  QVariantList is_buy_orders,  QVariantList durations,  QVariantList issueds,  QVariantList sale_ranges)
{
    if(!is_setup)
    {
        setup();
    }
    db.open();
    qDebug() << "EvDB: Updating Region <" << QString::number(reg_id) << ", " << reg_name << "> Beginning transaction: " << QString::number(orderIDs.size()) << " orders. DB IS Open: " << db.isOpen();
    QSqlQuery q(db);
    db.transaction();


    if(orderIDs.size() == 0)
    {
        // It would be kind of silly to do all the below code if there were no orders.
        // If 0 orders, clear the Market and call it good.
        db.transaction();
        q.prepare("DELETE FROM MarketOrders WHERE regionID=:regionID AND cycle_count=:cycle_count");
        q.bindValue(":regionID", reg_id);
        q.bindValue(":cycle_count", cycle_count - 1);
        q.exec();
        db.commit();
        db.close();
        emit finishedRegion();
        return;
    }

    QString my_table = "MarketOrders_" + QString::number(reg_id);

    q.exec("CREATE TEMPORARY TABLE " + my_table + "(orderID BIGINT UNSIGNED NOT NULL,typeID SMALLINT UNSIGNED NOT NULL,location_id BIGINT UNSIGNED NOT NULL,volume_total INT UNSIGNED NOT NULL,volume_remain INT UNSIGNED NOT NULL,volume_minimum INT UNSIGNED NOT NULL,price DECIMAL(16,2) NOT NULL,is_buy_order BOOL NOT NULL,duration SMALLINT UNSIGNED NOT NULL,issued DATETIME NOT NULL,sale_range VARCHAR(11) NOT NULL,PRIMARY KEY(orderID),INDEX(typeID,location_id,price,is_buy_order)) ENGINE=MEMORY DEFAULT CHARSET=utf8 COLLATE=utf8_unicode_ci");

    q.prepare("INSERT INTO " + my_table + " (orderID,typeID,location_id,volume_total,volume_remain,volume_minimum,price,is_buy_order,duration,issued,sale_range) VALUES (:orderID,:typeID,:location_id,:volume_total,:volume_remain,:volume_minimum,:price,:is_buy_order,:duration,:issued,:sale_range) ON DUPLICATE KEY UPDATE price=VALUES(price),issued=VALUES(issued),volume_remain=VALUES(volume_remain)");

    q.bindValue(":orderID", orderIDs);
    q.bindValue(":typeID", typeIDs);
    q.bindValue(":location_id", location_ids);
    q.bindValue(":volume_total", volume_totals);
    q.bindValue(":volume_remain", volume_remains);
    q.bindValue(":volume_minimum", volume_minimums);
    q.bindValue(":price", prices);
    q.bindValue(":is_buy_order", is_buy_orders);
    q.bindValue(":duration", durations);
    q.bindValue(":issued", issueds);
    q.bindValue(":sale_range", sale_ranges);

    qDebug() << "EveDB: Insert Lists bound: " << QDateTime::currentDateTime().toString() << " Size: " << orderIDs.size();

    if (!q.execBatch())
        qDebug() << q.lastError();

    qDebug() << "EveDB: Inserts finished: " << QDateTime::currentDateTime().toString();


    qDebug() << "EvDB: Updating Region <" << QString::number(reg_id) << ", " << reg_name << "> Transaction to microtable finished.";

    q.prepare("DELETE FROM MarketOrders WHERE regionID=:regionID AND cycle_count=:cycle_count");
    q.bindValue(":regionID", reg_id);
    q.bindValue(":cycle_count", cycle_count - 1);
    if(!q.exec())
            qDebug() << q.lastError();

    q.prepare("INSERT INTO MarketOrders (orderID,typeID,location_id,volume_total,volume_remain,volume_minimum,price,is_buy_order,duration,issued,sale_range,cycle_count,regionID) SELECT orderID,typeID,location_id,volume_total,volume_remain,volume_minimum,price,is_buy_order,duration,issued,sale_range,:cycle_count,:regionID FROM " + my_table);
    q.bindValue(":regionID", reg_id);
    q.bindValue(":cycle_count", cycle_count);

    if(!q.exec())
            qDebug() << q.lastError();

    if(!q.exec("DROP TEMPORARY TABLE " + my_table))
            qDebug() << q.lastError();
    db.commit();
    cycle_count++;

    //finished++;
    //qDebug() << "EvDB: Updated Regions: " << QString::number(finished);
    emit finishedRegion();
    db.close();
}
