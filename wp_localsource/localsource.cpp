/*
    This file is part of Waver

    Copyright (C) 2017 Peter Papp <peter.papp.p@gmail.com>

    Please visit https://launchpad.net/waver for details

    Waver is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Waver is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    (GPL.TXT) along with Waver. If not, see <http://www.gnu.org/licenses/>.

*/


#include "localsource.h"

// plugin factory
void wp_plugin_factory(int pluginTypesMask, PluginFactoryResults *retVal)
{
    if (pluginTypesMask & PluginBase::PLUGIN_TYPE_SOURCE) {
        retVal->append((PluginBase*) new LocalSource());
    }
}


// constructor
LocalSource::LocalSource()
{
    id           = QUuid("{187C9046-4801-4DB2-976C-128761F25BD8}");
    readyEmitted = false;

    variationSetting           = "Medium";
    variationSetCountSinceHigh = 0;
    variationSetCountSinceLow  = 0;
    variationSetCurrentRemainingDir();
}


// destructor
LocalSource::~LocalSource()
{
    // interrupt running scanners (usually there will be none at this point, but just in case)
    foreach(FileScanner *scanner, scanners) {
        scanner->requestInterruption();
    }

    // this is a bit ugly, but have to wait until scanners interrupt (see scannerFinished)
    while (scanners.count() > 0) {
        QThread::currentThread()->msleep(25);
        QCoreApplication::processEvents();
    }
}


// overrided virtual function
int LocalSource::pluginType()
{
    return PLUGIN_TYPE_SOURCE;
}


// overrided virtual function
QString LocalSource::pluginName()
{
    return "Local Files";
}


// overrided virtual function
int LocalSource::pluginVersion()
{
    return 2;
}


// overrided virtual function
bool LocalSource::hasUI()
{
    return true;
}


// overrided virtual function
QUuid LocalSource::persistentUniqueId()
{
    return id;
}


// thread entry
void LocalSource::run()
{
    qsrand(QDateTime::currentDateTime().toTime_t());

    emit loadConfiguration(id);
}


// slot receiving configuration
void LocalSource::loadedConfiguration(QUuid uniqueId, QJsonDocument configuration)
{
    if (uniqueId != id) {
        return;
    }

    bool doScan = false;
    if (configuration.isEmpty()) {
        if (directories.isEmpty()) {
            // use default locations
            directories.append(QStandardPaths::standardLocations(QStandardPaths::MusicLocation));

            // must save updated configuration
            emit saveConfiguration(id, configToJson());
            doScan = true;
        }
    }
    else {
        jsonToConfig(configuration);
        doScan = true;
    }

    if (doScan) {
        // just to be on the safe side
        foreach(FileScanner *scanner, scanners) {
            scanner->requestInterruption();
        }

        // make sure no duplicates
        mutex.lock();
        trackFileNames.clear();
        mutex.unlock();

        // start new scans
        foreach (QString directory, directories) {
            scanDir(directory);
        }
    }
}


// client wants to display this plugin's configuration dialog
void LocalSource::getUiQml(QUuid uniqueId)
{
    if (uniqueId != id) {
        return;
    }

    QFile settingsFile("://LS_DirsSettings.qml");
    settingsFile.open(QFile::ReadOnly);
    QString settings = settingsFile.readAll();
    settingsFile.close();

    QString modelElements;
    foreach(QString directory, directories) {
        modelElements.append(QString("ListElement { dirpath: \"%1\" } ").arg(directory));
    }

    settings.replace("ListModel { id: dirsModel }", QString("ListModel { id: dirsModel; %1 }").arg(modelElements));
    settings.replace("9999", QString("%1").arg(variationSettingId()));

    emit uiQml(id, settings);
}


// slot receiving configuration dialog results
void LocalSource::uiResults(QUuid uniqueId, QJsonDocument results)
{
    if (uniqueId != id) {
        return;
    }

    loadedConfiguration(id, results);
    variationSetCurrentRemainingDir();

    emit requestRemoveTracks(id);

    emit saveConfiguration(id, configToJson());
}


// must be a broken file
void LocalSource::unableToStart(QUuid uniqueId, QUrl url)
{
    // TODO implement blacklist
}


// request for playlist entries
void LocalSource::getPlaylist(QUuid uniqueId, int maxCount)
{
    if (uniqueId != id) {
        return;
    }

    TracksInfo returnValue;

    // can't do anything if there aren't any files
    if (trackFileNames.count() < 1) {
        // let everybody know
        emit unready(id);
        readyEmitted = false;

        if (alreadyPlayedTrackFileNames.count() > 0) {
            // all tracks have been played, start over
            mutex.lock();
            alreadyPlayedTrackFileNames.clear();
            mutex.unlock();

            // must save updated configuration
            emit saveConfiguration(id, configToJson());

            // start new scans if none in progress
            if (scanners.count() == 0) {
                foreach (QString directory, directories) {
                    scanDir(directory);
                }
            }
        }

        return;
    }

    // random select some tracks according to variation
    int count = (qrand() % maxCount) + 1;
    for (int i = 1; i <= count; i++) {
        // make sure we didn't run out of tracks
        if (trackFileNames.count() < 1) {
            break;
        }

        // set variation if done
        if (variationRemaining == 0) {
            variationSetCurrentRemainingDir();
        }

        // make scanners wait so they don't mess up the arrays while we're working with them
        mutex.lock();

        // see what we can select form
        QStringList variationSelection;

        // first try a simple filter
        foreach (QString trackFileName, trackFileNames) {
            // this will append everything is variationDir is empty
            if (trackFileName.startsWith(variationDir)) {
                variationSelection.append(trackFileName);
            }
        }

        // if there are no more tracks in current variation directory and variation is Low, then we have to try parent dir
        if ((variationSelection.count() < 1) && (variationCurrent == 0)) {
            QString currentVariationDir = variationDir;

            // find the base directory
            currentVariationDir.append("/");
            QString baseDirectory;
            foreach (QString directory, directories) {
                if (currentVariationDir.startsWith(directory) && (directory.length() > baseDirectory.length())) {
                    baseDirectory = directory;
                }
            }
            if (baseDirectory.endsWith("/")) {
                baseDirectory = baseDirectory.left(baseDirectory.length() - 1);
            }

            // get parent directory path
            while (currentVariationDir.endsWith("/")) {
                currentVariationDir = currentVariationDir.left(currentVariationDir.length() - 1);
            }
            currentVariationDir = (currentVariationDir.contains("/") ? currentVariationDir.left(currentVariationDir.lastIndexOf("/")) : "");

            // make sure there is a parent directory
            if ((currentVariationDir.length() > 0) && (currentVariationDir.compare(baseDirectory) != 0)) {
                // set current variation directory to parent
                variationDir = currentVariationDir;

                // filter again
                foreach (QString trackFileName, trackFileNames) {
                    if (trackFileName.startsWith(variationDir)) {
                        variationSelection.append(trackFileName);
                    }
                }
            }
        }

        // there are no more tracks in variation directory (or its parent if variation is Low)
        if (variationSelection.count() < 1) {
            // set a new variation (directory will be empty, don't have to filter on it)
            variationSetCurrentRemainingDir();

            // copy without filter
            foreach (QString trackFileName, trackFileNames) {
                variationSelection.append(trackFileName);
            }
        }

        // this should never happen
        if (variationSelection.count() < 1) {
            return;
        }

        // select random track
        int     variationIndex = qrand() % variationSelection.count();
        QString trackFileName  = variationSelection.at(variationIndex);
        int     trackIndex     = trackFileNames.indexOf(trackFileName);

        // so that it won't be played again
        trackFileNames.removeAt(trackIndex);
        alreadyPlayedTrackFileNames.append(trackFileName);

        // scanners can continue
        mutex.unlock();

        // return value
        TrackInfo trackInfo = trackInfoFromFilePath(trackFileName);
        returnValue.append(trackInfo);

        // remember variation directory if it was just selected and variation is not not on High
        if ((variationCurrent < 2) && (variationDir.length() < 1)) {
            variationDir = trackFileName.left(trackFileName.lastIndexOf("/"));
        }

        // adjust variation track counter
        variationRemaining--;
    }

    // reset variation if not on Low
    if (variationCurrent != 0) {
        variationRemaining = 0;
    }

    // send them back to the server
    emit playlist(id, returnValue);

    // must save updated configuration
    emit saveConfiguration(id, configToJson());
}


// client wants to dispaly open dialog
void LocalSource::getOpenTracks(QUuid uniqueId, QString parentId)
{
    if (uniqueId != id) {
        return;
    }

    OpenTracks openTracks;

    // top level
    if (parentId.length() < 1) {
        foreach(QString directory, directories) {
            OpenTrack openTrack;
            openTrack.hasChildren = true;
            openTrack.id = directory;
            openTrack.label = directory;
            openTrack.selectable = false;
            openTracks.append(openTrack);
        }
        emit openTracksResults(id, openTracks);
        return;
    }

    // parent always must be a directory
    QDir directory(parentId);
    if (!directory.exists()) {
        return;
    }

    QFileInfoList entries = directory.entryInfoList();
    foreach (QFileInfo entry, entries) {
        if ((entry.fileName().compare(".") == 0) || (entry.fileName().compare("..") == 0)) {
            continue;
        }

        OpenTrack openTrack;

        if (entry.isDir()) {
            openTrack.hasChildren = true;
            openTrack.id = entry.absoluteFilePath();
            openTrack.label = entry.fileName();
            openTrack.selectable = true;
            openTracks.append(openTrack);
        }

        if (isTrackFile(entry)) {
            openTrack.hasChildren = false;
            openTrack.id = entry.absoluteFilePath();
            openTrack.label = entry.fileName();
            openTrack.selectable = true;
            openTracks.append(openTrack);
        }
    }
    emit openTracksResults(id, openTracks);
}


// turn open dialog selections to playlist entries
void LocalSource::resolveOpenTracks(QUuid uniqueId, QStringList selectedTracks)
{
    if (uniqueId != id) {
        return;
    }

    QStringList fileList;

    foreach (QString selectedTrack, selectedTracks) {
        QFileInfo fileInfo(selectedTrack);

        if (fileInfo.isDir()) {
            // not doing recursive stuff here, except one level down but only if no tracks found in this directory
            QFileInfoList entries = QDir(fileInfo.absoluteFilePath()).entryInfoList();

            QVector<QDir> dirs;
            bool fileFound = false;
            foreach (QFileInfo entry, entries) {
                if (entry.isDir()) {
                    dirs.append(QDir(entry.absoluteFilePath()));
                }
                if (isTrackFile(entry)) {
                    fileList.append(entry.absoluteFilePath());
                    fileFound = true;
                }
            }
            if (!fileFound) {
                foreach(QDir dir, dirs) {
                    QFileInfoList subEntries = dir.entryInfoList();
                    foreach (QFileInfo entry, subEntries) {
                        if (isTrackFile(entry)) {
                            fileList.append(entry.absoluteFilePath());
                        }
                    }
                }
            }
            continue;
        }

        if (isTrackFile(fileInfo)) {
            fileList.append(fileInfo.absoluteFilePath());
        }
    }

    // this doesn't count in the already played tracks, so this is simple here
    TracksInfo returnValue;
    foreach (QString file, fileList) {
        returnValue.append(trackInfoFromFilePath(file));
    }

    emit playlist(id, returnValue);
}


// request for playlist entries based on search criteria
void LocalSource::search(QUuid uniqueId, QString criteria)
{
    if (uniqueId != id) {
        return;
    }

    criteria.replace("_", " ").replace(" ", "");

    QStringList matches;

    // TODO change this to be real filesystem seach (add some parameter to filescanner and use that)

    mutex.lock();

    foreach (QString trackFileName, alreadyPlayedTrackFileNames) {
        QString modifying(trackFileName);
        modifying.replace("_", " ").replace(" ", "");

        if (modifying.contains(criteria, Qt::CaseInsensitive)) {
            matches.append(trackFileName);
        }
    }

    foreach (QString trackFileName, trackFileNames) {
        QString modifying(trackFileName);
        modifying.replace("_", " ").replace(" ", "");

        if (modifying.contains(criteria, Qt::CaseInsensitive)) {
            matches.append(trackFileName);
        }
    }

    qSort(matches);

    OpenTracks openTracks;
    foreach (QString match, matches) {
        OpenTrack openTrack;

        openTrack.hasChildren = false;
        openTrack.id = match;
        openTrack.label = match;
        openTrack.selectable = true;
        openTracks.append(openTrack);
    }

    mutex.unlock();

    emit searchResults(id, openTracks);
}


// user clicked action that was included in track info
void LocalSource::action(QUuid uniqueId, int actionKey)
{
    if (uniqueId != id) {
        return;
    }
}


// scan a dir
void LocalSource::scanDir(QString dir)
{
    FileScanner *fileScanner = new FileScanner((QObject*)this, dir, &trackFileNames, &alreadyPlayedTrackFileNames, &mutex);
    connect(fileScanner, SIGNAL(foundFirst()), this, SLOT(scannerFoundFirst()));
    connect(fileScanner, SIGNAL(finished()),   this, SLOT(scannerFinished()));
    scanners.append(fileScanner);
    fileScanner->start();
}


// scanner signal handler
void LocalSource::scannerFoundFirst()
{
    if (!readyEmitted) {
        // there must be a little delay to give a chance for the scanner to get more tracks
        QTimer::singleShot(500, this, SLOT(readyTimer()));
        readyEmitted = true;
    }
}


// timer signal handler
void LocalSource::readyTimer()
{
    emit ready(id);
}


// scanner signal handler
void LocalSource::scannerFinished()
{
    // get the scanner that sent this signal
    FileScanner *sender = (FileScanner*) QObject::sender();

    // find it in our scanners
    int senderIndex = -1;
    int i = 0;
    while ((i < scanners.count()) && (senderIndex < 0)) {
        if (scanners.at(i) == sender) {
            senderIndex = i;
        }
        i++;
    }
    if (senderIndex < 0) {
        // this should never happen
        return;
    }

    // housekeeping
    scanners.remove(senderIndex);
    delete sender;

    // maybe all tracks have been played already?
    int trackFileNamesCount;
    int alreadyPlayedTrackFileNamesCount;
    mutex.lock();
    trackFileNamesCount              = trackFileNames.count();
    alreadyPlayedTrackFileNamesCount = alreadyPlayedTrackFileNames.count();
    mutex.unlock();
    if ((trackFileNamesCount < 1) && (alreadyPlayedTrackFileNamesCount > 0) && (scanners.count() == 0)) {
        // start over
        mutex.lock();
        alreadyPlayedTrackFileNames.clear();
        mutex.unlock();

        // must save updated configuration
        emit saveConfiguration(id, configToJson());

        // start new scans
        foreach (QString directory, directories) {
            scanDir(directory);
        }
    }
}


// configuration conversion
QJsonDocument LocalSource::configToJson()
{
    QJsonObject jsonObject;

    jsonObject.insert("directories", QJsonValue(QJsonArray::fromStringList(directories)));
    mutex.lock();
    jsonObject.insert("alreadyPlayedTrackFileNames", QJsonValue(QJsonArray::fromStringList(alreadyPlayedTrackFileNames)));
    mutex.unlock();
    jsonObject.insert("variation", variationSetting);

    QJsonDocument returnValue;
    returnValue.setObject(jsonObject);

    return returnValue;
}


// configuration conversion
void LocalSource::jsonToConfig(QJsonDocument jsonDocument)
{
    mutex.lock();

    if (jsonDocument.object().contains("directories")) {
        directories.clear();
        foreach (QJsonValue jsonValue, jsonDocument.object().value("directories").toArray()) {
            directories.append(jsonValue.toString());
        }
    }

    if (jsonDocument.object().contains("alreadyPlayedTrackFileNames")) {
        alreadyPlayedTrackFileNames.clear();
        foreach (QJsonValue jsonValue, jsonDocument.object().value("alreadyPlayedTrackFileNames").toArray()) {
            alreadyPlayedTrackFileNames.append(jsonValue.toString());
        }
    }

    if (jsonDocument.object().contains("variation")) {
        variationSetting = jsonDocument.object().value("variation").toString();
        variationSetCountSinceLow  = (variationSettingId() == 3 ? 3 : 0);
        variationSetCountSinceHigh = 0;
    }

    mutex.unlock();
}


// helper
bool LocalSource::isTrackFile(QFileInfo fileInfo)
{
    return (fileInfo.exists() && fileInfo.isFile() && !fileInfo.isSymLink() && (fileInfo.fileName().endsWith(".mp3", Qt::CaseInsensitive) || mimeDatabase.mimeTypeForFile(fileInfo).name().startsWith("audio", Qt::CaseInsensitive)));
}


// helper
PluginSource::TrackInfo LocalSource::trackInfoFromFilePath(QString filePath)
{
    // find track's base directory for track info discovery
    QString trackDirectory;
    foreach (QString directory, directories) {
        if (filePath.startsWith(directory) && (directory.length() > trackDirectory.length())) {
            trackDirectory = directory;
        }
    }

    // track info discovery
    QString title     = "";
    QString performer = "";
    QString album     = "";
    QStringList trackRelative = QString(filePath).remove(trackDirectory).split("/");
    if (trackRelative.at(0).isEmpty()) {
        trackRelative.removeFirst();
    }
    if (trackRelative.count() > 0) {
        title = trackRelative.last().replace(".mp3", "", Qt::CaseInsensitive);
        trackRelative.removeLast();
    }
    if (trackRelative.count() > 0) {
        performer = trackRelative.first();
        trackRelative.removeFirst();
    }
    if (trackRelative.count() > 0) {
        album = trackRelative.join(" - ");
    }

    // search for pictures
    QVector<QUrl> pictures;
    QFileInfoList entries = QDir(filePath.left(filePath.lastIndexOf("/"))).entryInfoList();
    foreach (QFileInfo entry, entries) {
        if (entry.exists() && entry.isFile() && !entry.isSymLink() && (entry.fileName().endsWith(".jpg", Qt::CaseInsensitive) || entry.fileName().endsWith(".png", Qt::CaseInsensitive))) {
            pictures.append(QUrl::fromLocalFile(entry.absoluteFilePath()));
        }
    }

    // add to playlist
    TrackInfo trackInfo;
    trackInfo.url       = QUrl::fromLocalFile(filePath);
    trackInfo.cast      = false;
    trackInfo.title     = title;
    trackInfo.performer = performer;
    trackInfo.album     = album;
    trackInfo.year      = 0;
    trackInfo.track     = 0;
    trackInfo.pictures.append(pictures);
    trackInfo.actions.insert(0, "Ban");

    return trackInfo;
}


// helper
int LocalSource::variationSettingId()
{
    QStringList variations({"Low", "Medium", "High", "Random" });
    return variations.indexOf(variationSetting);
}


// helper
void LocalSource::variationSetCurrentRemainingDir()
{
    if (variationSettingId() == 3) {
        if (variationSetCountSinceHigh >= 4) {
            variationCurrent = 2;
        }
        else if (variationSetCountSinceLow >= 4) {
            variationCurrent = qrand() % 3;
        }
        else {
            variationCurrent = (qrand() % 2) + 1;
        }
    }
    else {
        variationCurrent = variationSettingId();
    }

    switch (variationCurrent) {
    case 0:
        variationRemaining = (qrand() % 3) + 4;
        variationSetCountSinceHigh++;
        variationSetCountSinceLow = 0;
        break;
    case 1:
        variationRemaining = (qrand() % 2) + 2;
        variationSetCountSinceHigh++;
        variationSetCountSinceLow++;
        break;
    case 2:
        variationRemaining = -1;
        variationSetCountSinceHigh = 0;
        variationSetCountSinceLow++;
     }

    variationDir = "";
}
