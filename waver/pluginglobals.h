#ifndef PLUGINGLOBALS_H
#define PLUGINGLOBALS_H

#include <QAudioBuffer>
#include <QHash>
#include <QUrl>
#include <QUuid>
#include <QVariant>
#include <QVariantHash>
#include <QVector>

typedef QVector<QAudioBuffer *> BufferQueue;

struct TrackAction {
    QUuid   pluginId;
    int     id;
    QString label;
};
struct TrackInfo {
    QUrl                 url;
    bool                 cast;
    QVector<QUrl>        pictures;
    QString              title;
    QString              performer;
    QString              album;
    int                  year;
    int                  track;
    QVector<TrackAction> actions;
};
typedef QVector<TrackInfo> TracksInfo;

typedef QHash<QUrl, QVariantHash> ExtraInfo;

struct OpenTrack {
    QString id;
    QString label;
    bool    hasChildren;
    bool    selectable;
};
typedef QVector<OpenTrack> OpenTracks;

typedef QVector<QVariantHash> SqlResults;

struct DiagnosticItem {
    QString label;
    QString message;
};
typedef QVector<DiagnosticItem> DiagnosticData;

static const int PLUGIN_TYPE_SOURCE  = 1;
static const int PLUGIN_TYPE_DECODER = 2;
static const int PLUGIN_TYPE_DSP_PRE = 4;
static const int PLUGIN_TYPE_DSP     = 8;
static const int PLUGIN_TYPE_OUTPUT  = 16;
static const int PLUGIN_TYPE_INFO    = 32;
static const int PLUGIN_TYPE_ALL     =
    PLUGIN_TYPE_SOURCE  |
    PLUGIN_TYPE_DECODER |
    PLUGIN_TYPE_DSP_PRE |
    PLUGIN_TYPE_DSP     |
    PLUGIN_TYPE_OUTPUT  |
    PLUGIN_TYPE_INFO;

static const int CACHE_BUFFER_COUNT = 3;

static const int PLAYLIST_MODE_NORMAL        = 0;
static const int PLAYLIST_MODE_LOVED         = 1;
static const int PLAYLIST_MODE_LOVED_SIMILAR = 2;

static const int RESERVED_ACTION_TRACKINFOUPDATED = 1250000;

#endif // PLUGINGLOBALS_H
