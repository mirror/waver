#ifndef ANALYZER_H
#define ANALYZER_H

#include "wp_acoustid_global.h"

#include <chromaprint.h>
#include <QAudioBuffer>
#include <QAudioFormat>
#include <QByteArray>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QStringList>

#ifdef QT_DEBUG
    #include <QDebug>
#endif

#include "../waver/API/plugindsppre_006.h"

class WP_ACOUSTID_EXPORT Analyzer : PluginDspPre_006 {
        Q_OBJECT

    public:

        int     pluginType()                                                       override;
        QString pluginName()                                                       override;
        int     pluginVersion()                                                    override;
        QString waverVersionAPICompatibility()                                     override;
        QUuid   persistentUniqueId()                                               override;
        bool    hasUI()                                                            override;
        int     priority()                                                         override;
        void    setBufferQueue(BufferQueue *bufferQueue, QMutex *bufferQueueMutex) override;
        void    setCast(bool cast)                                                 override;

        Analyzer();
        ~Analyzer();


    private:

        QUuid id;

        bool cast;

        bool enabled;
        bool globalEnabled;

        BufferQueue *bufferQueue;
        QMutex      *bufferQueueMutex;

        bool    firstBuffer;
        bool    analyzeOK;
        bool    decoderStarted;
        bool    decoderFinished;
        QString userAgent;
        bool    sendDiagnostics;
        QString chromaprint;
        qint64  duration;

        ChromaprintContext *chromaprintContext;

        void sendChromaprint();

        void sendDiagnosticsData();


    public slots:

        void run() override;

        void loadedConfiguration(QUuid uniqueId, QJsonDocument configuration)       override;
        void loadedGlobalConfiguration(QUuid uniqueId, QJsonDocument configuration) override;

        void sqlResults(QUuid persistentUniqueId, bool temporary, QString clientIdentifier, int clientSqlIdentifier, SqlResults results)       override;
        void globalSqlResults(QUuid persistentUniqueId, bool temporary, QString clientIdentifier, int clientSqlIdentifier, SqlResults results) override;
        void sqlError(QUuid persistentUniqueId, bool temporary, QString clientIdentifier, int clientSqlIdentifier, QString error)              override;

        void getUiQml(QUuid uniqueId)                         override;
        void uiResults(QUuid uniqueId, QJsonDocument results) override;

        void messageFromPlugin(QUuid uniqueId, QUuid sourceUniqueId, int messageId, QVariant value) override;

        void startDiagnostics(QUuid uniqueId) override;
        void stopDiagnostics(QUuid uniqueId)  override;

        void bufferAvailable(QUuid uniqueId) override;
        void decoderDone(QUuid uniqueId)     override;
};

#endif // ANALYZER_H
