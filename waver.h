/*
    This file is part of Waver
    Copyright (C) 2021 Peter Papp
    Please visit https://launchpad.net/waver for details
*/

#ifndef WAVER_H
#define WAVER_H

#include <QByteArray>
#include <QDateTime>
#include <QFileInfo>
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
#include <QTimer>
#include <QUrl>
#include <QUuid>
#include <QVariant>
#include <QVariantMap>
#include <taglib/fileref.h>
#include <taglib/tpropertymap.h>
#include <taglib/tstring.h>


#ifdef QT_DEBUG
    #include <QDebug>
#endif

#include "ampacheserver.h"
#include "filescanner.h"
#include "peakcallback.h"
#include "track.h"


class Waver : public QObject, PeakCallback
{
    Q_OBJECT

    public:

        struct ErrorItem {
            QDateTime dateTime;
            QString   id;
            QString   info;
            QString   error;
        };

        explicit Waver();
        ~Waver();

        QList<ErrorItem> getErrorLog();
        bool isShutdownCompleted();

        void peakCallback(double lPeak, double rPeak, qint64 delayMicroseconds, void *trackPointer);


    private:

        static const QString UI_ID_PREFIX_LOCALDIR;
        static const QString UI_ID_PREFIX_LOCALDIR_SUBDIR;
        static const QString UI_ID_PREFIX_LOCALDIR_FILE;
        static const QString UI_ID_PREFIX_SERVER;
        static const QString UI_ID_PREFIX_SERVER_SEARCH;
        static const QString UI_ID_PREFIX_SERVER_SEARCHRESULT;
        static const QString UI_ID_PREFIX_SERVER_BROWSE;
        static const QString UI_ID_PREFIX_SERVER_BROWSEALPHABET;
        static const QString UI_ID_PREFIX_SERVER_BROWSEARTIST;
        static const QString UI_ID_PREFIX_SERVER_BROWSEALBUM;
        static const QString UI_ID_PREFIX_SERVER_BROWSESONG;
        static const QString UI_ID_PREFIX_SERVER_PLAYLISTS;
        static const QString UI_ID_PREFIX_SERVER_PLAYLIST;
        static const QString UI_ID_PREFIX_SERVER_SMARTPLAYLISTS;
        static const QString UI_ID_PREFIX_SERVER_SMARTPLAYLIST;
        static const QString UI_ID_PREFIX_SERVER_PLAYLIST_ITEM;
        static const QString UI_ID_PREFIX_SERVER_RADIOSTATIONS;
        static const QString UI_ID_PREFIX_SERVER_RADIOSTATION;
        static const QString UI_ID_PREFIX_SERVER_SHUFFLE;
        static const QString UI_ID_PREFIX_SERVER_SHUFFLETAG;
        static const QString UI_ID_PREFIX_SERVER_SHUFFLE_FAVORITES;
        static const QString UI_ID_PREFIX_SERVER_SHUFFLE_NEVERPLAYED;

        static const quint32 RANDOM_MAX = std::numeric_limits<quint32>::max();

        enum ShuffleMode {
            None,
            Favorite,
            NeverPlayed
        };

        QList<ErrorItem> errorLog;

        QStringList           localDirs;
        QList<AmpacheServer*> servers;

        QList<FileScanner*> fileScanners;

        QQuickView *globalConstantsView;
        QObject    *globalConstants;

        PeakCallbackInfo peakCallbackInfo;

        Track                   *previousTrack;
        Track                   *currentTrack;
        QList<Track*>            playlist;
        QList<Track::TrackInfo>  history;
        QStringList              crossfadeTags;
        bool                     crossfadeInProgress;

        QTimer *shuffleCountdownTimer;
        double  shuffleCountdownPercent;
        int     shuffleServerIndex;

        int      serverIndex(QString id);
        QVariant globalConstant(QString constName);

        void itemActionLocal(QString id, int action, QVariantMap extra);
        void itemActionServer(QString id, int action, QVariantMap extra);
        void itemActionServerItem(QString id, int action, QVariantMap extra);

        void actionPlay(Track::TrackInfo trackInfo);
        void actionPlay(Track *track);

        void startNextTrack();

        QChar            alphabetFromName(QString name);
        QString          formatMemoryValue(unsigned long bytes);
        QString          formatFrequencyValue(double hertz);
        Track::TrackInfo trackInfoFromFilePath(QString filePath);
        Track::TrackInfo trackInfoFromIdExtra(QString id, QVariantMap extra);

        void connectTrackSignals(Track *track, bool newConnect = true);
        bool isCrossfade(Track *track1, Track *track2);
        bool killPreviousTrack();

        void startShuffleCountdown();
        void stopShuffleCountdown();
        void startShuffleBatch(int srvIndex = 0, int artistId = 0, ShuffleMode mode = None);

        QMutex shutdownMutex;
        bool   shutdownCompleted;


    public slots:

        void run();

        void addServer(QString host, QString user, QString psw);
        void deleteServer(QString id);
        void explorerItemClicked(QString id, int action, QString extraJSON);
        void playlistItemClicked(int index, int action);
        void playlistItemDragDropped(int index, int destinationIndex);
        void positioned(double percent);

        void trackPlayPosition(QString id, bool decoderFinished, long knownDurationMilliseconds, long positionMilliseconds, long decodedMilliseconds);
        void trackBufferInfo(QString id, bool rawIsFile, unsigned long rawSize, bool pmcIsFile, unsigned long pmcSize);
        void trackNetworkConnecting(QString id, bool busy);
        void trackReplayGainInfo(QString id, double target, double current);
        void trackInfoUpdated(QString id);
        void trackFinished(QString id);
        void trackFadeoutStarted(QString id);
        void trackError(QString id, QString info, QString error);
        void trackStatusChanged(QString id, Track::Status status, QString statusString);

        void previousButton(int index);
        void nextButton();
        void playButton();
        void pauseButton();
        void stopButton();
        void favoriteButton(bool fav);

        void requestOptions();
        void updatedOptions(QString optionsJSON);

        void shutdown();


    private slots:

        void fileScanFinished();
        void serverOperationFinished(AmpacheServer::OpCode opCode, AmpacheServer::OpData opData, AmpacheServer::OpResults opResults);
        void errorMessage(QString id, QString info, QString error);

        void startNextTrackUISignals();
        void playlistUpdateUISignals();
        void clearTrackUISignals();

        void shuffleCountdown();


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
        void explorerToggleSelected(QVariant id);

        void uiSetTrackData(QVariant titleText, QVariant performerText, QVariant albumText, QVariant trackNumberText, QVariant yearText);
        void uiSetFavorite(QVariant favorite);
        void uiSetTrackBusy(QVariant busy);
        void uiSetTrackLength(QVariant lengthText);
        void uiSetTrackPosition(QVariant positionText, QVariant positionPercent, QVariant decodedPercent);
        void uiSetTrackTags(QVariant tagsText);
        void uiSetTrackBufferData(QVariant memoryUsageText);
        void uiSetTrackReplayGain(QVariant target, QVariant current);
        void uiSetPeakMeter(QVariant left, QVariant right);
        void uiSetShuffleCountdown(QVariant percent);

        void uiSetImage(QVariant image);
        void uiSetTempImage(QVariant image);

        void playlistAddItem(QVariant title, QVariant artist, QVariant group, QVariant image, QVariant selected);
        void playlistClearItems();
        void playlistBufferData(QVariant index, QVariant memoryUsageText);
        void playlistBusy(QVariant index, QVariant busy);
        void playlistSelected(QVariant index, QVariant busy);

        void optionsAsRequested(QVariant optionsObj);

        void uiSetStatusText(QVariant statusText);
        void uiSetStatusTempText(QVariant statusTempText);

        void uiHistoryAdd(QVariant title);
};

#endif // WAVER_H
