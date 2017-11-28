#include "esibase.h"

EsiRequest::EsiRequest(QObject *parent, QNetworkReply *re) : QObject(parent) {
    rep = re;
    connect(rep, &QNetworkReply::finished, this, &EsiRequest::processReply);
}

void EsiRequest::processReply() {
    QByteArray raw(rep->readAll());
    QJsonDocument doc = QJsonDocument::fromJson(raw);
    emit sendDocument(doc);
    this->deleteLater();
}

EsiBase::EsiBase(QObject *parent) : QObject(parent)
{
    manager = new QNetworkAccessManager(this);
    baseUrl = "https://esi.tech.ccp.is";
    branch = "latest";
    agent = "EveCalc";
    datasource = "tranquility";
    module = "";
}

void EsiBase::processHeaders(QNetworkRequest *req) {
    req->setHeader(QNetworkRequest::UserAgentHeader, agent);
    req->setRawHeader(QByteArray("Accept"), QByteArray("application/json"));
}

QString EsiBase::fullUrl()
{
    QString full_url = baseUrl + "/" + branch + "/";
    return full_url;
}



QNetworkReply* EsiBase::getRequest(QUrl url, QUrlQuery qurl) {
    qurl.addQueryItem("user_agent", agent);
    qurl.addQueryItem("datasource", datasource);
    url.setQuery(qurl);
    QNetworkRequest req(url);
    processHeaders(&req);
    QNetworkReply *rep = manager->get(req);
    return rep;
}


QNetworkReply* EsiBase::getMarketsPrices() {
    //return getRequest("markets/prices", empty_head, empty_head);
}

QNetworkReply* EsiBase::getMarketsGroups() {
    //return getRequest("markets/groups", empty_head, empty_head);
}

QNetworkReply* EsiBase::getCharactersOrders(qint64 character_id) {

}

QNetworkReply* EsiBase::getMarketsGroup(qint64 market_id) {

}

QNetworkReply* EsiBase::getMarketsStructure(qint64 stru_id, int page)
{
    QVariant num(stru_id);
    QString nm = num.toString();
    QString path = fullUrl() + "markets/structures/" + nm + "/";
    QUrl url(path);
    QUrlQuery qurl;
    qurl.addQueryItem("page", QString::number(page));
    return getRequest(url, qurl);
}

QNetworkReply* EsiBase::getMarketsRegionHistory(qint64 reg_id) {

}

QNetworkReply* EsiBase::getMarketsRegionOrders(qint64 reg_id, int page)
{
    QVariant num(reg_id);
    QString nm = num.toString();
    QString path = fullUrl() + "markets/" + nm + "/orders/";
    QUrl url(path);
    QUrlQuery qurl;
    qurl.addQueryItem("page", QString::number(page));
    return getRequest(url, qurl);
}

QNetworkReply* EsiBase::getMarketRegionTypes(qint64 reg_id) {

}

QNetworkReply* EsiBase::getUniverseStructures()
{
    QString path = fullUrl() + "universe/structures/";
    QUrl url(path);
    QUrlQuery qurl;
    return getRequest(url, qurl);
}

QNetworkReply* EsiBase::getUniverseStructure(qint64 structure_id)
{
    QVariant num(structure_id);
    QString nm = num.toString();
    QString path = fullUrl() + "universe/structures/" + nm + "/";
    QUrl url(path);
    QUrlQuery qurl;
    return getRequest(url, qurl);
}
