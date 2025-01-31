/*
    This file is part of Waver
    Copyright (C) 2021 Peter Papp
    Please visit https://launchpad.net/waver for details
*/


#ifndef GLOBALS_H
#define GLOBALS_H

#include <QAudioBuffer>
#include <QByteArray>
#include <QVector>

static const QString DEFAULT_SHUFFLE_OPERATOR           = "or";
static const int     DEFAULT_SHUFFLE_COUNT              = 5;
static const int     DEFAULT_RANDOM_LISTS_COUNT         = 11;
static const int     DEFAULT_SHUFFLE_DELAY_SECONDS      = 10;
static const int     DEFAULT_SHUFFLE_FAVORITE_FREQUENCY = 4;
static const int     DEFAULT_SHUFFLE_RECENT_FREQUENCY   = 4;
static const bool    DEFAULT_SHUFFLE_AUTOSTART          = true;

static const int     DEFAULT_FONT_SIZE            = 12;
static const int     DEFAULT_ALPHABET_LIMIT       = 49;
static const int     DEFAULT_RECENTLY_ADDED_COUNT = 100;
static const int     DEFAULT_RECENTLY_ADDED_DAYS  = 14;
static const bool    DEFAULT_AUTO_REFRESH         = false;

static const int DEFAULT_SEARCH_COUNT_MAX        = 0;
static const int DEFAULT_SEARCH_ACTION           = 2;
static const int DEFAULT_SEARCH_ACTION_FILTER    = 3;
static const int DEFAULT_SEARCH_ACTION_COUNT_MAX = 11;

static const bool DEFAULT_HIDE_DOT_PLAYLIST    = true;
static const bool DEFAULT_TITLE_CURLY_SPECIAL  = true;
static const bool DEFAULT_STARTING_INDEX_APPLY = true;
static const int  DEFAULT_STARTING_INDEX_DAYS  = 30;

static const QString DEFAULT_FADE_TAGS      = "live,medley,nonstop";
static const QString DEFAULT_CROSSFADE_TAGS = "live";
static const int     DEFAULT_FADE_SECONDS   = 7;

static const int    DEFAULT_MAX_PEAK_FPS  = 25;
static const bool   DEFAULT_PEAK_DELAY_ON = false;
static const qint64 DEFAULT_PEAK_DELAY_MS = 333;

static const bool   DEFAULT_EQON   = true;
static const double DEFAULT_PREAMP = 0.0;
static const double DEFAULT_EQ1    = 6.0;
static const double DEFAULT_EQ2    = 3.0;
static const double DEFAULT_EQ3    = 1.5;
static const double DEFAULT_EQ4    = 0.0;
static const double DEFAULT_EQ5    = -1.5;
static const double DEFAULT_EQ6    = 0.0;
static const double DEFAULT_EQ7    = 3.0;
static const double DEFAULT_EQ8    = 6.0;
static const double DEFAULT_EQ9    = 9.0;
static const double DEFAULT_EQ10   = 12.0;

static const int  DEFAULT_WIDE_STEREO_DELAY_MILLISEC = 0;
static const bool DEFAULT_SKIP_LONG_SILENCE          = true;
static const int  DEFAULT_SKIP_LONG_SILENCE_SECONDS  = 4;

static const double SILENCE_THRESHOLD_DB = -25;

struct TimedChunk {
    QByteArray *chunkPointer;
    qint64      startMicroseconds;
};

typedef QVector<QAudioBuffer *> BufferQueue;
typedef QVector<QByteArray *>   ChunkQueue;
typedef QVector<TimedChunk>     TimedChunkQueue;

enum NotificationDataToSend {
    All,
    MetaData,
    PlaybackStatus
};

#endif // GLOBALS_H
