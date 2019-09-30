#include "LoginRequest.h"

#include "Helper.h"

#include <QUrlQuery>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>

void LoginRequest::send(QNetworkAccessManager& nam, QString username, QString password) {
    QNetworkRequest request(QUrl(METAVERSE_API_DOMAIN + "/oauth/token"));

    request.setHeader(QNetworkRequest::UserAgentHeader, getHTTPUserAgent());
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");

    QUrlQuery query;
    query.addQueryItem("grant_type", "password");
    query.addQueryItem("username", username);
    query.addQueryItem("password", password);
    query.addQueryItem("scope", "owner");

    auto reply = nam.post(request, query.toString().toUtf8());
    QObject::connect(reply, &QNetworkReply::finished, this, &LoginRequest::receivedResponse);
}

void LoginRequest::receivedResponse() {
    _state = State::Finished;

    auto reply = static_cast<QNetworkReply*>(sender());

    if (reply->error()) {
        qDebug() << "Error logging in: " << reply->readAll();
        _error = Error::Unknown;
        emit finished();
        //setApplicationState(ApplicationState::UnexpectedError);
        return;
    }

    auto data = reply->readAll();
    QJsonParseError parseError;
    auto doc = QJsonDocument::fromJson(data, &parseError);

    if (parseError.error != QJsonParseError::NoError) {
        qDebug() << "Error parsing response for login" << data;
        _error = Error::BadResponse;
        emit finished();
        //setApplicationStateError("Failed to login");
        return;
    }

    auto root = doc.object();

    if (!root.contains("access_token")
        || !root.contains("token_type")
        || !root.contains("expires_in")
        || !root.contains("refresh_token")
        || !root.contains("scope")
        || !root.contains("created_at")) {

        _error = Error::BadUsernameOrPassword;
        emit finished();
        return;
    }

    _token.accessToken = root["access_token"].toString();
    _token.refreshToken = root["refresh_token"].toString();
    _token.tokenType = root["token_type"].toString();

    qDebug() << "Got response for login: " << data;
    _rawLoginToken = data;

    emit finished();
}
