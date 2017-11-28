#ifndef ESIBASE_H
#define ESIBASE_H

#include <QObject>
#include <QString>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QUrlQuery>
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QByteArray>

class EsiRequest : public QObject {
    Q_OBJECT
public:
    EsiRequest(QObject *parent = Q_NULLPTR, QNetworkReply *re = Q_NULLPTR);
    QNetworkReply *rep;

signals:
    void sendDocument(QJsonDocument doc);

public slots:
    void processReply();
};

class EsiBase : public QObject
{
    Q_OBJECT
public:
    explicit EsiBase(QObject *parent = Q_NULLPTR);
    QNetworkAccessManager *manager;
    QString baseUrl, branch, agent, datasource, module;

    void processHeaders(QNetworkRequest *req);
    QString fullUrl();

    QNetworkReply* getRequest(QUrl url, QUrlQuery qurl);

    QNetworkReply* getMarketsPrices();
    QNetworkReply* getMarketsGroups();
    QNetworkReply* getCharactersOrders(qint64 character_id);
    QNetworkReply* getMarketsGroup(qint64 market_id);
    QNetworkReply* getMarketsStructure(qint64 stru_id, int page);
    QNetworkReply* getMarketsRegionHistory(qint64 reg_id);
    QNetworkReply* getMarketsRegionOrders(qint64 reg_id, int page);
    QNetworkReply* getMarketRegionTypes(qint64 reg_id);
    QNetworkReply* getUniverseStructures();
    QNetworkReply* getUniverseStructure(qint64 structure_id);

signals:

public slots:
};

#endif // ESIBASE_H
