/*
    This file is part of Waver
    Copyright (C) 2021 Peter Papp
    Please visit https://launchpad.net/waver for details
*/


#include "ampacheserver.h"


AmpacheServer::AmpacheServer(QUrl host, QString user, QObject *parent) : QObject(parent)
{
    this->host = host;
    this->user = user;

    writeKeychainJob = nullptr;
    readKeychainJob  = nullptr;

    handshakeInProgress            = false;
    sessionExpiredHandshakeStarted = false;
    authKey                        = "";
    serverVersion                  = 0;
    songCount                      = 0;
    flaggedCount                   = 0;

    shuffled = 0;

    connect(&networkAccessManager, &QNetworkAccessManager::finished, this, &AmpacheServer::networkFinished);
}


long AmpacheServer::apiVersionFromString(QString apiVersionString)
{
    if (!apiVersionString.length()) {
        apiVersionString = "0";
    }

    bool OK         = false;
    long apiVersion = apiVersionString.toLong(&OK);

    if (OK) {
        if ((apiVersion >= 500000) && (apiVersion < 1000000)) {
            apiVersion *= 10;
        }
    }
    else {
        QStringList versionParts = apiVersionString.split(".");
        if (versionParts.size() == 3) {
            long majorApiVersion = 0;
            long minorApiVersion = 0;
            long patchApiVersion = 0;

            majorApiVersion = versionParts.at(0).toLong();
            minorApiVersion = versionParts.at(1).toLong();
            patchApiVersion = versionParts.at(2).toLong();

            apiVersion = majorApiVersion * 1000000 + minorApiVersion * 1000 + patchApiVersion;
        }
        if (apiVersion < 5000000) {
            apiVersion = 0;
        }
    }

    return apiVersion;
}


QNetworkRequest AmpacheServer::buildRequest(QUrlQuery query, QObject *extra)
{
    QUrl url(host);
    url.setPath("/server/xml.server.php");
    url.setQuery(query);

    QNetworkRequest request(url);
    request.setRawHeader("User-Agent", QGuiApplication::instance()->applicationName().toUtf8());
    request.setOriginatingObject(extra);

    return request;
}


QString AmpacheServer::formattedName()
{
    return QString("%1@%2").arg(user, host.host(QUrl::FullyDecoded));
}


QUrl AmpacheServer::getHost()
{
    return host;
}


QString AmpacheServer::getId()
{
    return id;
}


QUuid AmpacheServer::getSettingsId()
{
    return settingsId;
}


QString AmpacheServer::getUser()
{
    return user;
}


bool AmpacheServer::isShuffleTagSelected(int tagId)
{
    return shuffleTags.contains(tagId);
}


void AmpacheServer::networkFinished(QNetworkReply *reply)
{
    if (reply->error() != QNetworkReply::NoError) {
        emit errorMessage(id, tr("Network Error"), reply->errorString());
        reply->deleteLater();
        return;
    }
    if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute) != 200) {
        emit errorMessage(id, tr("Server Error"), tr("Status code %1 received").arg(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toString()));
        reply->deleteLater();
        return;
    }

    QUrl requestUrl = reply->request().url();
    if (!requestUrl.isValid()) {
        emit errorMessage(id, tr("Invalid Network Reply"), tr("Invalid request URL"));
        reply->deleteLater();
        return;
    }

    if (!requestUrl.hasQuery()) {
        emit errorMessage(id, tr("Invalid Network Reply"), tr("Request URL contains no query"));
        reply->deleteLater();
        return;
    }
    QUrlQuery requestQuery(requestUrl);

    if (!requestQuery.hasQueryItem("action")) {
        emit errorMessage(id, tr("Invalid Network Reply"), tr("Request URL query contains no 'action' item"));
        reply->deleteLater();
        return;
    }
    QString action = requestQuery.queryItemValue("action");

    QString limitString = requestQuery.hasQueryItem("limit") ? requestQuery.queryItemValue("limit") : "0";
    int     limit       = limitString.toInt();

    bool   isHandshake = action.compare("handshake") == 0;
    OpCode opCode;
    OpData opData;
    OpData extraData;

    opData.insert("serverId", id);

    QObject *extra = reply->request().originatingObject();
    if (extra != nullptr) {
        foreach (QByteArray name, extra->dynamicPropertyNames()) {
            QVariant value = extra->property(name);
            if (value.isValid()) {
                opData.insert(QString(name), value.toString());
                extraData.insert(QString(name), value.toString());
            }
        }
        delete extra;
    }

    if (isHandshake) {
        opCode = Unknown;
    }
    else {
        if (action.compare("advanced_search") == 0) {
            if (limit <= 0) {
                opCode = Search;
                opData.insert("criteria", requestQuery.queryItemValue("rule_1_input"));
            }
            else {
                opCode = Shuffle;
            }
        }
        else if (action.compare("artists") == 0) {
            opCode = BrowseRoot;
        }
        else if (action.compare("artist_albums") == 0) {
            opCode = BrowseArtist;
            opData.insert("artist", requestQuery.queryItemValue("filter"));
        }
        else if (action.compare("album_songs") == 0) {
            opCode = BrowseAlbum;
            opData.insert("album", requestQuery.queryItemValue("filter"));
        }
        else if (action.compare("get_indexes") == 0) {
            QString type = requestQuery.queryItemValue("type");
            if (type.compare("live_stream") == 0) {
                opCode = RadioStations;
            }
            else if (type.compare("playlist") == 0) {
                opCode = PlaylistRoot;
            }
            else {
                emit errorMessage(id, tr("Invalid Network Reply"), tr("Request URL query contains an unknown 'type' item"));
                reply->deleteLater();
                return;
            }
        }
        else if (action.compare("playlist_songs") == 0) {
            opCode = PlaylistSongs;
            opData.insert("playlist", requestQuery.queryItemValue("filter"));
        }
        else if (action.compare("playlist_generate") == 0) {
            if (opData.contains("count_flagged")) {
                opCode = CountFlagged;
                flaggedCount = 0;
            }
            else {
                opCode = Shuffle;
            }
        }
        else if ((action.compare("tags") == 0) || (action.compare("genres") == 0)) {
            opCode = Tags;
        }
        else if (action.compare("flag") == 0) {
            opCode = SetFlag;
        }
        else if (action.compare("song") == 0) {
            opCode = Song;
            opData.insert("song_id", requestQuery.queryItemValue("filter"));

        }
        else {
            emit errorMessage(id, tr("Invalid Network Reply"), tr("Request URL query contains an unknown 'action' item"));
            reply->deleteLater();
            return;
        }
    }

    QStringList wantedElements = {
        "album",
        "art",
        "artist",
        "flag",
        "name",
        "tag",
        "genre",
        "time",
        "title",
        "track",
        "url",
        "year"
    };
    QStringList multiElements = {
        "tag",
        "genre"
    };
    QHash<OpCode, QString> opElement = {
        { Search,        "song" },
        { BrowseRoot,    "artist" },
        { BrowseArtist,  "album" },
        { BrowseAlbum,   "song" },
        { CountFlagged,  "song" },
        { PlaylistRoot,  "playlist" },
        { PlaylistSongs, "song" },
        { RadioStations, "live_stream" },
        { Shuffle,       "song" },
        { Song,          "song" }
    };
    if (serverVersion >= 5000000) {
        opElement.insert(Tags, "genre");
    }
    else {
        opElement.insert(Tags, "tag");
    }

    QString                      currentElement = "";
    OpResults                    opResults;
    OpData                       opResult;
    QMultiHash<QString, QString> multiElementsValues;
    int                          errorCode = 0;
    QString                      errorMsg  = "";

    QXmlStreamReader xmlStreamReader(reply);
    while (!xmlStreamReader.atEnd()) {
        QXmlStreamReader::TokenType tokenType = xmlStreamReader.readNext();

        if (tokenType == QXmlStreamReader::StartElement) {
            currentElement = xmlStreamReader.name().toString();

            QXmlStreamAttributes attributes = xmlStreamReader.attributes();

            if (currentElement.compare("error") == 0) {
                if (attributes.hasAttribute("code")) {
                    errorCode = attributes.value("code").toInt();
                }
                else if (attributes.hasAttribute("errorCode")) {
                    errorCode = attributes.value("errorCode").toInt();
                }
            }

            if (opElement.value(opCode).compare(currentElement) == 0) {
                opResult.clear();
                multiElementsValues.clear();
                if (attributes.hasAttribute("id")) {
                    opResult.insert("id", attributes.value("id").toString());
                    if (opCode == CountFlagged) {
                        flaggedCount++;
                    }
                }
            }
        }

        if (tokenType == QXmlStreamReader::EndElement) {
            if ((opElement.value(opCode).compare(xmlStreamReader.name().toString()) == 0) && opResult.size()) {
                foreach(QString element, multiElements) {
                    if (multiElementsValues.contains(element)) {
                        opResult.insert(QString("%1s").arg(element), multiElementsValues.values(element).join("|"));
                    }
                }
                opResults.append(opResult);
            }
            currentElement = "";
        }

        if (tokenType == QXmlStreamReader::Characters) {
            if ((currentElement.compare("error") == 0) || (currentElement.compare("errorMessage") == 0)) {
                errorMsg = xmlStreamReader.text().toString().trimmed();
            }

            if (isHandshake) {
                if (currentElement.compare("auth") == 0) {
                    authKey = xmlStreamReader.text().toString();
                }
                else if (currentElement.compare("api") == 0) {
                    serverVersion = apiVersionFromString(xmlStreamReader.text().toString());
                }
                else if (currentElement.compare("songs") == 0) {
                    songCount = xmlStreamReader.text().toInt();
                }
            }
            else {
                if (wantedElements.contains(currentElement)) {
                    if (multiElements.contains(currentElement)) {
                        multiElementsValues.insert(currentElement, xmlStreamReader.text().toString());
                    }
                    else {
                        opResult.insert(currentElement, xmlStreamReader.text().toString());
                    }
                }
            }
        }
    }

    reply->deleteLater();

    if (xmlStreamReader.hasError()) {
        emit errorMessage(id, tr("Failed to parse XML response"), xmlStreamReader.errorString());
        return;
    }
    if (errorCode || errorMsg.length()) {
        if (!isHandshake && (errorMsg.compare("Session Expired", Qt::CaseInsensitive) == 0)) {
            extra = nullptr;
            if (extraData.size()) {
                extra = new QObject;
                foreach(QString name, extraData.keys()) {
                    extra->setProperty(name.toLatin1().data(), extraData.value(name));
                    opData.remove(name);
                }
            }

            opQueue.prepend({ opCode, opData, extra });

            if (!handshakeInProgress && !sessionExpiredHandshakeStarted) {
                emit errorMessage(id, tr("Renewing handshake"), QString("%1 %2").arg(errorCode).arg(errorMsg));
                sessionExpiredHandshakeStarted = true;
                QTimer::singleShot(50, this, &AmpacheServer::startHandshake);
            }
            return;
        }

        handshakeInProgress            = false;
        sessionExpiredHandshakeStarted = false;

        emit errorMessage(id, tr("Server responded with error message"), QString("%1 %2").arg(errorCode).arg(errorMsg));
        return;
    }

    if (serverVersion >= 5000000) {
        for (int i = 0; i < opResults.size(); i++) {
            if (opResults[i].contains("genres") && !opResults[i].contains("tags")) {
                opResults[i].insert("tags", opResults[i].value("genres"));
            }
        }
    }

    if (isHandshake) {
        if (serverVersion < SERVER_API_VERSION_MIN) {
            emit errorMessage(id, tr("Server version does not satisfy minimum requirement"), QString("%1").arg(serverVersion));
            return;
        }

        handshakeInProgress            = false;
        sessionExpiredHandshakeStarted = false;

        QObject *opExtra = new QObject();
        opExtra->setProperty("count_flagged", "count_flagged");

        opQueue.prepend({ CountFlagged, OpData(), opExtra });
        QTimer::singleShot(50, this, &AmpacheServer::startOperations);
        return;
    }
    else if (opCode == Shuffle) {
        if (opData.contains("favorite")) {
            shuffleFavorites.append(opResults);
            shuffleFavoritesCompleted = true;
        }
        else {
            shuffleRegular.append(opResults);
            shuffleRegularCompleted = true;
        }

        if (shuffleRegularCompleted && shuffleFavoritesCompleted) {
            opResults.clear();

            QSettings settings;
            int favoriteFrequency = settings.value("options/shuffle_favorite_frequency", DEFAULT_SHUFFLE_FAVORITE_FREQUENCY).toInt();

            int limit = opData.value("shuffle_limit", "0").toInt();
            if (limit <= 0) {
                limit = settings.value("options/shuffle_count", DEFAULT_SHUFFLE_COUNT).toInt();
            }

            int regularIndex  = 0;
            int favoriteIndex = 0;

            while (limit > 0) {
                shuffled++;
                if ((shuffled % favoriteFrequency == 0) && shuffleFavorites.count()) {
                    opResults.append(shuffleFavorites.at(favoriteIndex));

                    favoriteIndex++;
                    if (favoriteIndex >= shuffleFavorites.count()) {
                        favoriteIndex = 0;
                    }
                }
                else if (shuffleRegular.count()) {
                    opResults.append(shuffleRegular.at(regularIndex));

                    regularIndex++;
                    if (regularIndex >= shuffleRegular.count()) {
                        regularIndex = 0;
                    }
                }
                limit--;
            }
            emit operationFinished(opCode, opData, opResults);
            QTimer::singleShot(500, this, &AmpacheServer::startOperations);
        }
    }
    else {
        emit operationFinished(opCode, opData, opResults);
        QTimer::singleShot(500, this, &AmpacheServer::startOperations);
    }
}


void AmpacheServer::readKeychainJobFinished(QKeychain::Job* job)
{
    if (job->error()) {
        emit errorMessage(id, tr("Cannot read password from keychain"), job->errorString());
        emit passwordNeeded(id);
    }
    else {
        this->psw = readKeychainJob->textData();
    }

    disconnect(readKeychainJob, &ReadPasswordJob::finished, this, &AmpacheServer::readKeychainJobFinished);
    delete readKeychainJob;

    readKeychainJob = nullptr;
}


void AmpacheServer::retreivePassword()
{
    if (readKeychainJob != nullptr) {
        return;
    }

    readKeychainJob = new ReadPasswordJob(QGuiApplication::instance()->applicationName());

    connect(readKeychainJob, &ReadPasswordJob::finished, this, &AmpacheServer::readKeychainJobFinished);

    readKeychainJob->setAutoDelete(false);
    readKeychainJob->setKey(this->formattedName());
    readKeychainJob->start();
}


void AmpacheServer::setId(QString id)
{
    this->id = id;
}


void AmpacheServer::setPassword(QString psw)
{
    this->psw = psw;

    if (writeKeychainJob != nullptr) {
        return;
    }

    writeKeychainJob = new WritePasswordJob(QGuiApplication::instance()->applicationName());

    connect(writeKeychainJob, &WritePasswordJob::finished, this, &AmpacheServer::writeKeychainJobFinished);

    writeKeychainJob->setAutoDelete(false);
    writeKeychainJob->setKey(this->formattedName());
    writeKeychainJob->setTextData(psw);
    writeKeychainJob->start();
}


void AmpacheServer::setSettingsId(QUuid settingsId)
{
    this->settingsId = settingsId;
    shuffleTags.clear();

    QSettings settings;
    QString   cacheKey = QString("%1/shuffleTags").arg(settingsId.toString());

    if (settings.contains(QString("%1/1/id").arg(cacheKey))) {
        int browseSize = settings.beginReadArray(cacheKey);
        for (int i = 0; i < browseSize; i++) {
            settings.setArrayIndex(i);
            shuffleTags.append(settings.value("id").toInt());
        }
        settings.endArray();
    }
}


void AmpacheServer::setShuffleTag(int tagId, bool selected)
{
    if (selected && !shuffleTags.contains(tagId)) {
        shuffleTags.append(tagId);
    }
    if (!selected && shuffleTags.contains(tagId)) {
        shuffleTags.removeAll(tagId);
    }

    QSettings settings;
    QString   cacheKey = QString("%1/shuffleTags").arg(settingsId.toString());

    settings.remove(cacheKey);

    settings.beginWriteArray(cacheKey);
    for(int i = 0; i < shuffleTags.size(); i++) {
       settings.setArrayIndex(i);
       settings.setValue("id", shuffleTags.at(i));
    }
    settings.endArray();
    settings.sync();
}


QStringList AmpacheServer::shuffleTagsToStringList()
{
    QStringList returnValue;

    QSettings settings;
    QString   cacheKey = QString("%1/tags").arg(settingsId.toString());

    int tagsSize = settings.beginReadArray(cacheKey);
    for (int i = 0; i < tagsSize; i++) {
        settings.setArrayIndex(i);
        if (shuffleTags.contains(settings.value("id").toInt())) {
            returnValue.append(settings.value("name").toString());
        }
    }
    settings.endArray();

    return returnValue;
}


void AmpacheServer::startHandshake()
{
    if (handshakeInProgress) {
        return;
    }
    handshakeInProgress = true;

    authKey = "";

    QString    timeStr  = QString("%1").arg(QDateTime::currentSecsSinceEpoch());
    QByteArray pswHash  = QCryptographicHash::hash(psw.toLatin1(), QCryptographicHash::Sha256);
    QByteArray authHash = QCryptographicHash::hash(QString(timeStr + QString(pswHash.toHex())).toLatin1(), QCryptographicHash::Sha256);

    QUrlQuery query;
    query.addQueryItem("action", "handshake");
    query.addQueryItem("auth", authHash.toHex());
    query.addQueryItem("timestamp", timeStr);
    query.addQueryItem("version", QString("%1").arg(SERVER_API_VERSION_MIN));
    query.addQueryItem("user", user);

    networkAccessManager.get(buildRequest(query, nullptr));
}


void AmpacheServer::startOperation(OpCode opCode, OpData opData, QObject *extra)
{
    opQueue.append({ opCode, opData, extra });
    QTimer::singleShot(50, this, &AmpacheServer::startOperations);
}


void AmpacheServer::startOperations()
{
    if (handshakeInProgress) {
        return;
    }
    if (!authKey.length()) {
        QTimer::singleShot(50, this, &AmpacheServer::startHandshake);
        return;
    }

    while (opQueue.count()) {
        Operation operation = opQueue.first();

        QUrlQuery query;
        query.addQueryItem("auth", authKey);

        if (operation.opCode == Search) {
            query.addQueryItem("action", "advanced_search");
            query.addQueryItem("rule_1", "title");
            query.addQueryItem("rule_1_operator", "0");
            query.addQueryItem("rule_1_input", operation.opData.value("criteria"));
        }
        else if (operation.opCode == BrowseRoot) {
            query.addQueryItem("action", "artists");
        }
        else if (operation.opCode == BrowseArtist) {
            query.addQueryItem("action", "artist_albums");
            query.addQueryItem("filter", operation.opData.value("artist"));
        }
        else if (operation.opCode == BrowseAlbum) {
            query.addQueryItem("action", "album_songs");
            query.addQueryItem("filter", operation.opData.value("album"));
        }
        else if (operation.opCode == PlaylistRoot) {
            query.addQueryItem("action", "get_indexes");
            query.addQueryItem("type", "playlist");
        }
        else if (operation.opCode == PlaylistSongs) {
            query.addQueryItem("action", "playlist_songs");
            query.addQueryItem("filter", operation.opData.value("playlist"));
        }
         else if (operation.opCode == RadioStations) {
            query.addQueryItem("action", "get_indexes");
            query.addQueryItem("type", "live_stream");
        }
        else if (operation.opCode == Shuffle) {
            QSettings settings;

            int randomListCount = settings.value("options/random_lists_count", DEFAULT_RANDOM_LISTS_COUNT).toInt();

            if (operation.opData.contains("favorite")) {
                query.addQueryItem("action", "playlist_generate");
                query.addQueryItem("mode", "random");
                query.addQueryItem("flag", "1");

                int limit = operation.opData.value("limit", "0").toInt();
                if (limit <= 0) {
                    limit = 1;
                }
                query.addQueryItem("limit", QString("%1").arg(limit));
            }
            else if (operation.opData.contains("artist")) {
                shuffleRegular.clear();
                shuffleRegularCompleted = false;
                shuffleFavorites.clear();
                shuffleFavoritesCompleted = true;

                query.addQueryItem("action", "playlist_generate");
                query.addQueryItem("artist", operation.opData.value("artist"));
                query.addQueryItem("limit", QString("%1").arg(randomListCount));

                if (operation.extra == nullptr) {
                    operation.extra = new QObject();
                }
                operation.extra->setProperty("shuffle_limit", randomListCount);
            }
            else if (operation.opData.contains("favorites")) {
                shuffleRegular.clear();
                shuffleRegularCompleted = false;
                shuffleFavorites.clear();
                shuffleFavoritesCompleted = true;

                query.addQueryItem("action", "playlist_generate");
                query.addQueryItem("mode", "random");
                query.addQueryItem("flag", "1");
                query.addQueryItem("limit", QString("%1").arg(randomListCount));

                if (operation.extra == nullptr) {
                    operation.extra = new QObject();
                }
                operation.extra->setProperty("shuffle_limit", randomListCount);
            }
            else if (operation.opData.contains("never_played")) {
                shuffleRegular.clear();
                shuffleRegularCompleted = false;
                shuffleFavorites.clear();
                shuffleFavoritesCompleted = true;

                query.addQueryItem("action", "playlist_generate");
                query.addQueryItem("mode", "unplayed");
                query.addQueryItem("limit", QString("%1").arg(randomListCount));

                if (operation.extra == nullptr) {
                    operation.extra = new QObject();
                }
                operation.extra->setProperty("shuffle_limit", randomListCount);
            }
            else if (operation.opData.contains("recently_added")) {
                shuffleRegular.clear();
                shuffleRegularCompleted = false;
                shuffleFavorites.clear();
                shuffleFavoritesCompleted = true;

                query.addQueryItem("action", "advanced_search");
                query.addQueryItem("type", "song");
                query.addQueryItem("random", "1");
                query.addQueryItem("rule_1", "recent_added");
                query.addQueryItem("rule_1_operator", "0");
                query.addQueryItem("rule_1_input", "100");
                query.addQueryItem("limit", QString("%1").arg(randomListCount));

                if (operation.extra == nullptr) {
                    operation.extra = new QObject();
                }
                operation.extra->setProperty("shuffle_limit", randomListCount);
            }
            else {
                shuffleRegular.clear();
                shuffleRegularCompleted = false;
                shuffleFavorites.clear();
                shuffleFavoritesCompleted = false;

                if (shuffleTags.size()) {
                    query.addQueryItem("action", "advanced_search");
                    query.addQueryItem("random", "1");
                    query.addQueryItem("operator", settings.value("options/shuffle_operator", DEFAULT_SHUFFLE_OPERATOR).toString());

                    QStringList shuffleTagStrings = shuffleTagsToStringList();
                    for (int i = 0; i < shuffleTagStrings.size(); i++) {
                        query.addQueryItem(QString("rule_%1").arg(i + 1), "tag");
                        query.addQueryItem(QString("rule_%1_operator").arg(i + 1), "4");
                        query.addQueryItem(QString("rule_%1_input").arg(i + 1), shuffleTagStrings.at(i));
                    }
                }
                else {
                    query.addQueryItem("action", "playlist_generate");
                }

                int favoriteFrequency = settings.value("options/shuffle_favorite_frequency", DEFAULT_SHUFFLE_FAVORITE_FREQUENCY).toInt();
                int limit             = settings.value("options/shuffle_count", DEFAULT_SHUFFLE_COUNT).toInt();

                if (flaggedCount > 0) {
                    int limitRegular  = 0;
                    int limitFavorite = 0;
                    for (int i = shuffled; i < shuffled + limit; i++) {
                        if ((i + 1) % favoriteFrequency == 0) {
                            limitFavorite++;
                            continue;
                        }
                        limitRegular++;
                    }

                    if (limitFavorite > 0) {
                        QObject *opExtra = new QObject();
                        opExtra->setProperty("favorite", "favorite");
                        opQueue.append({ Shuffle, {{ "favorite", "favorite" }, { "limit", QString("%1").arg(limitFavorite) }}, opExtra });
                    }
                    else {
                        shuffleFavoritesCompleted = true;
                    }

                    limit = limitRegular;
                }

                query.addQueryItem("limit", QString("%1").arg(limit));
            }
        }
        else if (operation.opCode == Tags) {
            if (serverVersion >= 5000000) {
                query.addQueryItem("action", "genres");
            }
            else {
                query.addQueryItem("action", "tags");
            }
        }
        else if (operation.opCode == SetFlag) {
            query.removeAllQueryItems("limit");
            query.addQueryItem("action", "flag");
            query.addQueryItem("type", "song");
            query.addQueryItem("id", operation.opData.value("song_id"));
            query.addQueryItem("flag", operation.opData.value("flag"));

            if (operation.opData.value("flag").compare("0")) {
                flaggedCount++;
            }
            else {
                flaggedCount--;
            }
        }
        else if (operation.opCode == CountFlagged) {
            query.removeAllQueryItems("limit");
            query.addQueryItem("action", "playlist_generate");
            query.addQueryItem("flag", "1");
            query.addQueryItem("format", "index");
        }
        else if (operation.opCode == Song) {
            query.removeAllQueryItems("limit");
            query.addQueryItem("action", "song");
            query.addQueryItem("filter", operation.opData.value("song_id"));
        }

        if ((serverVersion != 424000) && (serverVersion != 425000) && !query.hasQueryItem("limit")) {
            query.addQueryItem("limit", "none");
        }

        networkAccessManager.get(buildRequest(query, operation.extra));

        opQueue.removeFirst();

        if (operation.opCode == CountFlagged) {
            break;
        }
    }
}


void AmpacheServer::writeKeychainJobFinished(QKeychain::Job* job)
{
    if (job->error()) {
        QString info = tr("Cannot save password to keychain");
        if (QGuiApplication::instance()->applicationDirPath().contains("snap", Qt::CaseInsensitive)) {
            info.append(". ").append(tr("Please check the application's permissions."));
        }

        emit errorMessage(id, info, job->errorString());
    }

    disconnect(writeKeychainJob, &WritePasswordJob::finished, this, &AmpacheServer::writeKeychainJobFinished);
    delete writeKeychainJob;

    writeKeychainJob = nullptr;
}

