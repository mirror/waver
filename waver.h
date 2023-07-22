/*
    This file is part of Waver
    Copyright (C) 2021 Peter Papp
    Please visit https://launchpad.net/waver for details
*/

#ifndef WAVER_H
#define WAVER_H

#include <QtGlobal>
#include <QByteArray>
#include <QDateTime>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QList>
#include <QMutex>
#include <QObject>
#include <QQuickView>
#include <QRandomGenerator>
#include <QRegExp>
#include <QSettings>
#include <QStandardPaths>
#include <QString>
#include <QTextDocumentFragment>
#include <QtGlobal>
#include <QTimer>
#include <QUrl>
#include <QUuid>
#include <QVariant>
#include <QVariantMap>

#ifdef Q_OS_WIN
    #include "windows.h"
    #ifndef Q_OS_WINRT
        #include <taglib-1.12/taglib/fileref.h>
        #include <taglib-1.12/taglib/toolkit/tpropertymap.h>
        #include <taglib-1.12/taglib/toolkit/tstring.h>
    #endif
#endif

#ifdef Q_OS_LINUX
    #include <taglib/fileref.h>
    #include <taglib/tpropertymap.h>
    #include <taglib/tstring.h>
#endif

#ifdef QT_DEBUG
    #include <QDebug>
#endif

#include "ampacheserver.h"
#include "decodingcallback.h"
#include "filescanner.h"
#include "filesearcher.h"
#include "peakcallback.h"
#include "track.h"


class Waver : public QObject, PeakCallback, DecodingCallback
{
    Q_OBJECT

    public:

        explicit Waver();
        ~Waver();

        Track::TrackInfo getCurrentTrackInfo();
        Track::Status    getCurrentTrackStatus();
        long             getLastPositionMilliseconds();
        bool             isShutdownCompleted();

        void peakCallback(double lPeak, double rPeak, qint64 delayMicroseconds, void *trackPointer);
        void decodingCallback(double downloadPercent, double PCMPercent, void *trackPointer);


    private:

        struct LogItem {
            QDateTime dateTime;
            QString   id;
            QString   info;
            QString   error;
        };
        struct SearchActionData {
            QString query;
            int     nextExplorerResultIndex;
            int     doneCounter;
            int     filter;
            int     target;
        };

        static const QString UI_ID_PREFIX_SEARCH;
        static const QString UI_ID_PREFIX_SEARCHQUERY;
        static const QString UI_ID_PREFIX_SEARCHRESULT;
        static const QString UI_ID_PREFIX_SEARCHRESULT_ALBUM;
        static const QString UI_ID_PREFIX_SEARCHRESULT_ARTIST;
        static const QString UI_ID_PREFIX_SEARCHRESULT_PLAYLIST;
        static const QString UI_ID_PREFIX_LOCALDIR;
        static const QString UI_ID_PREFIX_LOCALDIR_SUBDIR;
        static const QString UI_ID_PREFIX_LOCALDIR_FILE;
        static const QString UI_ID_PREFIX_SERVER;
        static const QString UI_ID_PREFIX_SERVER_BROWSE;
        static const QString UI_ID_PREFIX_SERVER_BROWSEALPHABET;
        static const QString UI_ID_PREFIX_SERVER_BROWSEARTIST;
        static const QString UI_ID_PREFIX_SERVER_BROWSEALBUM;
        static const QString UI_ID_PREFIX_SERVER_BROWSESONG;
        static const QString UI_ID_PREFIX_SERVER_PLAYLISTS;
        static const QString UI_ID_PREFIX_SERVER_PLAYLISTSALPHABET;
        static const QString UI_ID_PREFIX_SERVER_PLAYLIST;
        static const QString UI_ID_PREFIX_SERVER_SMARTPLAYLISTS;
        static const QString UI_ID_PREFIX_SERVER_SMARTPLAYLISTSALPHABET;
        static const QString UI_ID_PREFIX_SERVER_SMARTPLAYLIST;
        static const QString UI_ID_PREFIX_SERVER_PLAYLIST_ITEM;
        static const QString UI_ID_PREFIX_SERVER_RADIOSTATIONS;
        static const QString UI_ID_PREFIX_SERVER_RADIOSTATIONSALPHABET;
        static const QString UI_ID_PREFIX_SERVER_RADIOSTATION;
        static const QString UI_ID_PREFIX_SERVER_GENRES;
        static const QString UI_ID_PREFIX_SERVER_GENRESALPHABET;
        static const QString UI_ID_PREFIX_SERVER_GENRE;
        static const QString UI_ID_PREFIX_SERVER_SHUFFLE;
        static const QString UI_ID_PREFIX_SERVER_SHUFFLE_FAVORITES;
        static const QString UI_ID_PREFIX_SERVER_SHUFFLE_NEVERPLAYED;
        static const QString UI_ID_PREFIX_SERVER_SHUFFLE_RECENTLYADDED;

        static const int SEARCH_MAX_QUERIES  = 9;
        static const int SEARCH_TARGET_SONGS = 0;
        static const int SEARCH_TARGET_REST  = 1;

        static const int AUTO_REFRESH_NO_ACTION_DELAY_MILLISEC = 60 * 1000;
        static const int AUTO_REFRESH_BROWSE                   = 0;
        static const int AUTO_REFRESH_PLAYLISTS                = 1;
        static const int AUTO_REFRESH_RAIOSTATIONS             = 2;
        static const int AUTO_REFRESH_GENRES                   = 3;

        enum ShuffleMode {
            None,
            Favorite,
            NeverPlayed,
            RecentlyAdded
        };

        QStringList           localDirs;
        QList<AmpacheServer*> servers;

        QList<FileScanner*>  fileScanners;
        QList<FileSearcher*> fileSearchers;

        QQuickView *globalConstantsView;
        QObject    *globalConstants;

        PeakCallbackInfo peakCallbackInfo;
        qint64           peakCallbackCount;
        qint64           peakFPS;
        QMutex           peakFPSMutex;
        qint64           peakFPSMax;
        int              peakUILagLastMS;
        int              peakLagCheckCount;
        qint64           peakLagCount;
        qint64           peakFPSNextIncrease;
        bool             peakDelayOn;
        qint64           peakDelayMilliseconds;

        DecodingCallbackInfo decodingCallbackInfo;

        Track                   *previousTrack;
        Track                   *currentTrack;
        QList<Track*>            playlist;
        Track::TracksInfo        history;
        long                     lastPositionMilliseconds;
        QStringList              crossfadeTags;
        bool                     crossfadeInProgress;

        QTimer *shuffleCountdownTimer;
        double  shuffleCountdownPercent;
        int     shuffleServerIndex;
        bool    shuffleFirstAfterStart;

        QStringList       playlistLoadSongIds;
        Track::TracksInfo playlistLoadTracks;

        bool   stopByShutdown;
        QMutex shutdownMutex;
        bool   shutdownCompleted;

        int                              searchQueriesCounter;
        QHash<QString, QString>          searchQueries;                  // query string => explorer id
        int                              searchOperationsCounter;
        int                              searchResultsCounter;
        QHash<QString, SearchActionData> searchActionItemsCounter;       // explorer id => action done items counter

        bool   autoRefresh;
        qint64 autoRefreshLastActionTimestamp;
        int    autoRefreshLastDay;
        int    autoRefreshLastServerIndex;
        int    autoRefreshLastItem;

        int      serverIndex(QString id);
        QVariant globalConstant(QString constName);

        void itemActionSearch(QString id, int action, QVariantMap extra);
        void itemActionSearchResult(QString id, int action, QVariantMap extra);
        void itemActionLocal(QString id, int action, QVariantMap extra);
        void itemActionServer(QString id, int action, QVariantMap extra);
        void itemActionServerItem(QString id, int action, QVariantMap extra);

        void actionPlay(Track::TrackInfo trackInfo);
        void actionPlay(Track *track);

        void startNextTrack();

        QChar            alphabetFromName(QString name);
        QString          formatMemoryValue(unsigned long bytes, bool padded = false);
        QString          formatFrequencyValue(double hertz);
        Track::TrackInfo trackInfoFromFilePath(QString filePath);
        Track::TrackInfo trackInfoFromIdExtra(QString id, QVariantMap extra);
        QUrl             trackURL(QString id);

        void connectTrackSignals(Track *track, bool newConnect = true);
        bool isCrossfade(Track *track1, Track *track2);
        void killPreviousTrack();

        void startShuffleCountdown();
        void stopShuffleCountdown();
        void startShuffleBatch(int srvIndex = -1, QString group = "", int artistId = 0, ShuffleMode mode = None, QString originalAction = "action_play", int shuffleTag = 0, int insertDestinationindex = 0, QString searchActionParentId = "");

        void explorerNetworkingUISignals(QString id, bool networking);

        void playlistSave();
        int  playlistLoad();

        bool    playlistAttributeSave(AmpacheServer *server, QString playlistId, QString attribute, QString value);
        QString playlistAttributeLoad(AmpacheServer *server, QString playlistId, QString attribute);

        bool playlistContains(QString id);

        void searchCaches(QString parent, QString criteria);
        void searchServers(QString parent, QString criteria);
        void searchLocalDirs(QString parent, QString criteria);

        void searchAction();


    public slots:

        void run();

        void addServer(QString host, QString user, QString psw);
        void deleteServer(QString id);
        void setServerPassword(QString id, QString psw);
        void explorerItemClicked(QString id, int action, QString extraJSON);
        void playlistItemClicked(int index, int action);
        void playlistItemDragDropped(int index, int destinationIndex);
        void playlistExplorerItemDragDropped(QString id, QString extraJSON, int destinationIndex);
        void positioned(double percent);

        void trackPlayPosition(QString id, bool decoderFinished, long knownDurationMilliseconds, long positionMilliseconds);
        void trackNetworkConnecting(QString id, bool busy);
        void trackReplayGainInfo(QString id, double current);
        void trackInfoUpdated(QString id);
        void trackFinished(QString id);
        void trackFadeoutStarted(QString id);
        void trackError(QString id, QString info, QString error);
        void trackInfo(QString id, QString info);
        void trackSessionExpired(QString trackId);
        void trackStatusChanged(QString id, Track::Status status, QString statusString);

        void previousButton(int index);
        void nextButton();
        void playButton();
        void pauseButton();
        void ppButton();
        void stopButton();
        void favoriteButton(bool fav);
        void raiseButton();

        void searchCriteriaEntered(QString criteria);
        void searchResult(QString parentId, QString id, QString extraJSON);
        void requestOptions();
        void updatedOptions(QString optionsJSON);
        void requestEQ(int eqChooser);

        void peakUILag(int lagMS);

        void shutdown();


    private slots:

        void fileScanFinished();
        void fileSearchFinished();
        void serverOperationFinished(AmpacheServer::OpCode opCode, AmpacheServer::OpData opData, AmpacheServer::OpResults opResults);
        void serverPasswordNeeded(QString id);
        void errorMessage(QString id, QString info, QString error);

        void startNextTrackUISignals();
        void playlistUpdateUISignals();
        void clearTrackUISignals();

        void shuffleCountdown();
        void autoRefreshNext();


    signals:

        void requestTrackBufferReplayGainInfo();

        void explorerAddItem(QVariant id, QVariant parent, QVariant title, QVariant image, QVariant extra, QVariant expandable, QVariant playable, QVariant selectable, QVariant selected);
        void explorerDisableQueueable(QVariant id);
        void explorerRemoveAboveLevel(QVariant id);
        void explorerRemoveChildren(QVariant id);
        void explorerRemoveItem(QVariant id);
        void explorerSetBusy(QVariant id, QVariant busy);
        void explorerSetFlagExtra(QVariant id, QVariant flag);
        void explorerSetError(QVariant id, QVariant isError, QVariant errorMessage);
        void explorerSetSelected(QVariant id, QVariant selected);
        void explorerSetTitle(QVariant id, QVariant title);
        void explorerSortChildren(QVariant id);
        void explorerToggleSelected(QVariant id);
        void explorerGetSearchResult(QVariant parent, QVariant index);

        void uiPromptServerPsw(QVariant id, QVariant formattedName);

        void uiSetTrackData(QVariant titleText, QVariant performerText, QVariant albumText, QVariant trackNumberText, QVariant yearText, QVariant artistSummaryText);
        void uiSetTitleCurlySpecial(QVariant tcs);
        void uiSetFavorite(QVariant favorite);
        void uiSetTrackBusy(QVariant busy);
        void uiSetTrackLength(QVariant lengthText);
        void uiSetTrackPosition(QVariant positionText, QVariant positionPercent);
        void uiSetTrackDecoding(QVariant downloadPercent, QVariant pcmPercent);
        void uiSetTrackTags(QVariant tagsText);
        void uiSetTrackAmpacheURL(QVariant ampacheURL);
        void uiSetPeakMeter(QVariant leftPercent, QVariant rightPercent, QVariant scheduledTimeMS);
        void uiSetPeakMeterReplayGain(QVariant gainPercent);
        void uiSetShuffleCountdown(QVariant percent);
        void uiSetFontSize(QVariant fontSize);

        void uiSetImage(QVariant image);
        void uiSetTempImage(QVariant image);

        void uiShowSearchCriteria();

        void playlistAddItem(QVariant title, QVariant artist, QVariant group, QVariant image, QVariant selected, QVariant ampacehUrl);
        void playlistClearItems();
        void playlistBufferData(QVariant index, QVariant memoryUsageText);
        void playlistBusy(QVariant index, QVariant busy);
        void playlistDecoding(QVariant index, QVariant downloadPercent, QVariant pcmPercent);
        void playlistBigBusy(QVariant busy);
        void playlistTotalTime(QVariant totalTime);
        void playlistSelected(QVariant index, QVariant busy);

        void optionsAsRequested(QVariant optionsObj);
        void eqAsRequested(QVariant eqObj);

        void uiSetStatusText(QVariant statusText);
        void uiSetStatusTempText(QVariant statusTempText);

        void uiHistoryAdd(QVariant title);
        void uiHistoryRemove(QVariant count);

        void notify(NotificationDataToSend dataToSend);
        void uiRaise();
        void uiSetIsSnap(QVariant isSnap);
};

#endif // WAVER_H
