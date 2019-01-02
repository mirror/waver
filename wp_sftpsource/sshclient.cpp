/*
    This file is part of Waver

    Copyright (C) 2018 Peter Papp <peter.papp.p@gmail.com>

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


#include "sshclient.h"


const QString SSHClient::AUTH_METHOD_PUBLICKEY      = "publickey";
const QString SSHClient::AUTH_METHOD_PASSWORD       = "password";
const QString SSHClient::AUTH_METHOD_KB_INTERACTIVE = "keyboard-interactive";


//constructor
SSHClient::SSHClient(SSHClientConfig config, QObject *parent) : QObject(parent)
{
    this->config = config;

    connectAttempt = 0;
    state          = Idle;
    loadingBytes   = 0;

    socket      = NULL;
    session     = NULL;
    sftpSession = NULL;

    mutex = new QMutex();

    extensions.append(".3gp");
    extensions.append(".aa");
    extensions.append(".aac");
    extensions.append(".aax");
    extensions.append(".act");
    extensions.append(".aiff");
    extensions.append(".amr");
    extensions.append(".ape");
    extensions.append(".au");
    extensions.append(".awb");
    extensions.append(".dct");
    extensions.append(".dss");
    extensions.append(".dvf");
    extensions.append(".flac");
    extensions.append(".gsm");
    extensions.append(".iklax");
    extensions.append(".ivs");
    extensions.append(".m4a");
    extensions.append(".m4b");
    extensions.append(".m4p");
    extensions.append(".mmf");
    extensions.append(".mp3");
    extensions.append(".mpc");
    extensions.append(".msv");
    extensions.append(".nsf");
    extensions.append(".ogg,");
    extensions.append(".opus");
    extensions.append(".ra,");
    extensions.append(".raw");
    extensions.append(".sln");
    extensions.append(".tta");
    extensions.append(".vox");
    extensions.append(".wav");
    extensions.append(".wma");
    extensions.append(".wv");
    extensions.append(".webm");
    extensions.append(".8svx");

    qRegisterMetaType<SSHClient::DirListItem>("SSHClient::DirListItem");
    qRegisterMetaType<SSHClient::DirList>("SSHClient::DirList");
}


// destructor
SSHClient::~SSHClient()
{
    downloadList.clear();

    disconnectSSH(config.id);

    if (socket != NULL) {
        delete socket;
    }

    delete mutex;
}


// thread entry point
void SSHClient::run()
{
    // check if there are files already cached
    updateState(CheckingCache);
    QStringList foundCached;
    findCachedAudio("", &foundCached);
    updateState(Idle);
    emit audioList(config.id, foundCached, true);

    // instantiate socket (must be in this thread)
    socket = new QTcpSocket();

    // socket signals
    connect(socket, SIGNAL(connected()),                                this, SLOT(socketConnected()));
    connect(socket, SIGNAL(error(QAbstractSocket::SocketError)),        this, SLOT(socketError(QAbstractSocket::SocketError)));
    connect(socket, SIGNAL(stateChanged(QAbstractSocket::SocketState)), this, SLOT(socketStateChanged(QAbstractSocket::SocketState)));

    // auto-connect after delay
    QTimer::singleShot(10000, this, SLOT(autoConnect()));
}


// timer slot
void SSHClient::autoConnect()
{
    connectSSH(config.id);
}

// public method
SSHClient::SSHClientConfig SSHClient::getConfig()
{
    return config;
}


// public method
void SSHClient::setCacheDir(QString cacheDir)
{
    if (config.cacheDir.isEmpty()) {
        config.cacheDir = cacheDir;
    }
}


// public method
QString SSHClient::formatUserHost()
{
    return QString("%1@%2").arg(config.user).arg(config.host);
}


// public method
bool SSHClient::isConnected()
{
    if (socket == NULL) {
        return false;
    }
    return (socket->state() == QTcpSocket::ConnectedState);
}


// public method
QString SSHClient::localToRemote(QString local)
{
    QString cacheDir = config.cacheDir;
    QString dir      = config.dir;

    if (!cacheDir.endsWith("/")) {
        cacheDir.append("/");
    }
    if (!dir.endsWith("/")) {
        dir.append("/");
    }

    return local.replace(QRegExp(QString("^%1").arg(cacheDir)), dir);
}


// public method
QString SSHClient::remoteToLocal(QString remote)
{
    QString cacheDir = config.cacheDir;
    QString dir      = config.dir;

    if (!cacheDir.endsWith("/")) {
        cacheDir.append("/");
    }
    if (!dir.endsWith("/")) {
        dir.append("/");
    }

    return remote.replace(QRegExp(QString("^%1").arg(dir)), cacheDir);
}


// public slot
void SSHClient::connectSSH(int id)
{
    if (id != config.id) {
        return;
    }

    // host validation, can be host only, or host:port
    QStringList host = config.host.split(":");
    if ((host.count() < 1) || (host.count() > 2)) {
        emit error(config.id, "Invalid configuration");
        return;
    }

    // determine port
    int port = 22;
    if (host.count() > 1) {
        bool OK = false;
        port = QString(host.at(1)).toInt(&OK);
        if (!OK || (port < 1)) {
            emit error(config.id, "Invalid configuration");
            return;
        }
    }

    // user validation
    if (config.user.length() < 1) {
        emit error(config.id, "Invalid configuration");
        return;
    }

    updateState(Connecting);

    // continued in socketConnected
    socket->connectToHost(host.at(0), (quint16)port);
}


// public slot
void SSHClient::disconnectSSH(int id)
{
    if (id != config.id) {
        return;
    }

    disconnectSSH(id, "");
}

// public slot
void SSHClient::disconnectSSH(int id, QString errorMessage)
{
    if (id != config.id) {
        return;
    }

    if (!errorMessage.isEmpty()) {
        QString sshErrorMessage = getErrorMessageSSH();
        if (!sshErrorMessage.isEmpty()) {
            errorMessage = errorMessage + " - " + sshErrorMessage;
        }
    }

    connectAttempt = 3;

    updateState(Disconnecting);

    if (sftpSession != NULL) {
        libssh2_sftp_shutdown(sftpSession);
        sftpSession = NULL;
    }
    if (session != NULL) {
        libssh2_session_disconnect(session, "Waver disconnecting");
        libssh2_session_free(session);
        session = NULL;
    }
    if (socket != NULL) {
        socket->close();
    }

    updateState(Idle);

    if (!errorMessage.isEmpty()) {
        emit error(config.id, errorMessage);
    }

    emit disconnected(config.id);
}


// private slot
void SSHClient::socketConnected()
{
    // start ssh session
    session = libssh2_session_init();
    if (!session) {
        disconnectSSH(config.id, "Unable to initialize ssh session");
        return;
    }

    // shake hands
    if (libssh2_session_handshake(session, socket->socketDescriptor())) {
        disconnectSSH(config.id, "Unable to perform ssh handshake");
        return;
    }

    // config stuff
    libssh2_session_set_blocking(session, 1);
    libssh2_session_set_timeout(session, TIMEOUT_MS);

    // banner just for the fun of it
    const char *bannerRaw = libssh2_session_banner_get(session);
    if (bannerRaw) {
        emit info(config.id, QString(bannerRaw));
    }

    // get available authentication methods
    QByteArray userRaw = config.user.toUtf8();
    char *userAuthListRaw = libssh2_userauth_list(session, userRaw.constData(), userRaw.count());
    if (!userAuthListRaw) {
        if (libssh2_userauth_authenticated(session)) {
            // just in case server accepted "none" authentication, which is pretty unlikely
            sftpSession = libssh2_sftp_init(session);
            if (!sftpSession) {
                disconnectSSH(config.id, "Could not open sftp session");
                return;
            }
            connectCheckDir();
            return;
        }
        disconnectSSH(config.id, "Could not get the list of available authentication methods");
        return;
    }
    userAuthList = QString(userAuthListRaw).split(",");

    // get fingerprint
    const char *fingerprintRaw = libssh2_hostkey_hash(session, LIBSSH2_HOSTKEY_HASH_SHA1);
    if (!fingerprintRaw) {
        disconnectSSH(config.id, "Could not get fingerprint");
        return;
    }
    QString fingerprint = QString(QByteArray(fingerprintRaw, HOSTKEY_HASH_SHA1_LENGTH).toHex());

    // decide what authentication mode to use
    QFileInfo privateKeyFileInfo(config.privateKeyFile);
    QFileInfo publicKeyFileInfo(config.publicKeyFile);
    QString authMethod = AUTH_METHOD_PUBLICKEY;
    if (!userAuthList.contains(authMethod) || !privateKeyFileInfo.exists() || !publicKeyFileInfo.exists() || (config.fingerprint.compare(fingerprint) != 0)) {
        authMethod = AUTH_METHOD_PASSWORD;
    }
    if (!userAuthList.contains(authMethod)) {
        // TODO support "keyboard-interactive" method
        disconnectSSH(config.id, "No supported authentication mode");
        return;
    }

    // authenticate
    if (authMethod.compare(AUTH_METHOD_PUBLICKEY) == 0) {
        connectAuthPublicKey();
    }
    else if (authMethod.compare(AUTH_METHOD_PASSWORD) == 0) {
        connectAuthPassword(fingerprint);
    }
}


// private method
void SSHClient::connectAuthPublicKey()
{
    if (libssh2_userauth_publickey_fromfile(session, config.user.toUtf8().constData(), config.publicKeyFile.toUtf8().constData(), config.privateKeyFile.toUtf8().constData(), NULL)) {
        // TODO try password authentication
        disconnectSSH(config.id, "Authentication failed");
        return;
    }

    emit info(config.id, "Authentication successful");

    // open SFTP session
    sftpSession = libssh2_sftp_init(session);
    if (!sftpSession) {
        disconnectSSH(config.id, "Could not open sftp session");
        return;
    }

    // figure out user's home dir
    setHomeDir();
    if (homeDir.isEmpty()) {
        disconnectSSH(config.id, "Could not determine user's home directory");
    }

    connectCheckDir();
}


// private method
void SSHClient::connectAuthPassword(QString fingerprint)
{
    // get password from user if not yet got
    if (password.isEmpty()) {
        QString explanation = "On this computer, Waver doesn't have passwordless public key authentication set up for this server yet.";
        if (!userAuthList.contains(AUTH_METHOD_PUBLICKEY)) {
            explanation = "This server does not support passwordless public key authentication.";
        }
        if (config.fingerprint.isEmpty()) {
            explanation = "Seems like this is the first time you're connecting to this server from Waver on this computer";
        }
        else if (config.fingerprint.compare(fingerprint) != 0) {
            explanation = "SERVER FINGERPRINT MISMATCH!\n\nThis might be an indication of your connection being hijacked by a virus or a hacker. However, this can also happen if the server was recently swapped or rebuilt. It is recommended that you contact the server's maintainer to check.\n\nDo not enter your password unless you're absolutely sure that it's safe to do so.\n\nKnown fingerprint: " + config.fingerprint;
        }

        // display password entry
        emit showPasswordEntry(config.id, formatUserHost(), fingerprint, explanation);
        return;
    }

    // authentication
    if (libssh2_userauth_password(session, config.user.toUtf8().constData(), password.toUtf8().constData())) {
        disconnectSSH(config.id, "Authentication failed");
        password.clear();
        return;
    }

    emit info(config.id, "Authentication successful");

    // maybe fingerprint needs to be updated
    if (config.fingerprint.compare(fingerprint) != 0) {
        config.fingerprint = fingerprint;
        emit updateConfig(config.id);

        // just to be on the safe side
        // TODO this might not be the best time to do this, see first TODO note in connectCreateKeys
        QFile(config.privateKeyFile).remove();
        QFile(config.publicKeyFile).remove();
    }

    // open SFTP session
    sftpSession = libssh2_sftp_init(session);
    if (!sftpSession) {
        disconnectSSH(config.id, "Could not open sftp session");
        return;
    }

    // figure out user's home dir
    setHomeDir();
    if (homeDir.isEmpty()) {
        disconnectSSH(config.id, "Could not determine user's home directory");
    }

    // offer creating keys
    if (userAuthList.contains(AUTH_METHOD_PUBLICKEY)) {
        emit showKeySetupQuestion(config.id, formatUserHost());
        return;
    }

    connectCheckDir();
}


// private method
void SSHClient::connectCreateKeys()
{
    // TODO compare server's authorized list with existing public keys to se maybe sevrer is not really new but was mover or different dir selected

    QString tempFileName = QString("waver_key_%1").arg(QDateTime::currentMSecsSinceEpoch());

    // keys are generated on the server, then downloaded, it's just a series of remote commands

    emit info(config.id, "Generating keys");
    QString command = QString("ssh-keygen -t rsa -f %1/%2 -N '' -q").arg(homeDir).arg(tempFileName);
    if (!executeSSH(command) || ((stdErrSSH.count() > 0) && !stdErrSSH.at(0).isEmpty())) {
        emit error(config.id, stdErrSSH.join(" "));
        connectCheckDir();
        return;
    }

    emit info(config.id, "Creating remote directory");
    command = QString("mkdir -p %1/.ssh").arg(homeDir);
    if (!executeSSH(command) || ((stdErrSSH.count() > 0) && !stdErrSSH.at(0).isEmpty())) {
        emit error(config.id, stdErrSSH.join(" "));
        connectCheckDir();
        return;
    }

    emit info(config.id, "Adding new public key to remote");
    command = QString("cat %1/%2.pub >> %1/.ssh/authorized_keys").arg(homeDir).arg(tempFileName);
    if (!executeSSH(command) || ((stdErrSSH.count() > 0) && !stdErrSSH.at(0).isEmpty())) {
        emit error(config.id, stdErrSSH.join(" "));
        connectCheckDir();
        return;
    }

    emit info(config.id, "Downloading keys");

    bool copyOK = download(QString("%1/%2.pub").arg(homeDir).arg(tempFileName), config.publicKeyFile);
    copyOK = copyOK && download(QString("%1/%2").arg(homeDir).arg(tempFileName), config.privateKeyFile);
    if (!copyOK) {
        emit error(config.id, "Could not copy key files");
    }

    emit info(config.id, "Deleting temporary files");
    command = QString("unlink %1/%2.pub").arg(homeDir).arg(tempFileName);
    executeSSH(command);
    command = QString("unlink %1/%2").arg(homeDir).arg(tempFileName);
    executeSSH(command);

    if (copyOK) {
        emit updateConfig(config.id);
        emit info(config.id, "Passwordless public key authentication is ready");
    }

    connectCheckDir();
}


// private method
void SSHClient::connectCheckDir()
{
    DirList dirContents;

    if (!config.dir.isEmpty()) {
        // make sure dir still exists
        if (!dirList(config.dir, &dirContents)) {
            config.dir.clear();
        }
    }

    // if doesn't exist anymore or was never set yet
    if (config.dir.isEmpty()) {
        if (!dirList(homeDir, &dirContents)) {
            disconnectSSH(config.id, "Could not get directory listing");
            return;
        }
        emit showDirSelector(config.id, formatUserHost(), homeDir, dirContents);
        return;
    }

    // all OK
    updateState(Idle);
    emit connected(config.id);
}


// private method
void SSHClient::setHomeDir()
{
    // HOME environment variable always set on Linux
    homeDir.clear();
    if (executeSSH("env")) {
        QRegExp homeRegExp("^HOME=(.+)$");
        if (stdOutSSH.indexOf(homeRegExp) >= 0) {
            homeDir = homeRegExp.capturedTexts().at(1);
        }
    }
}


// private method
void SSHClient::findCachedAudio(QString localDir, QStringList *results)
{
    if (localDir.isEmpty()) {
        localDir = config.cacheDir;
    }
    if (!localDir.startsWith(config.cacheDir)) {
        return;
    }

    QFileInfoList entries = QDir(localDir).entryInfoList();

    foreach (QFileInfo entry, entries) {
        if (entry.fileName().startsWith(".")) {
            continue;
        }
        if (entry.isDir()) {
            findCachedAudio(entry.absoluteFilePath(), results);
            continue;
        }
        if (extensions.contains("." + entry.suffix())) {
            results->append(localToRemote(entry.absoluteFilePath()));
        }
    }
}


// private method
bool SSHClient::executeSSH(QString command)
{
    SSHClientState prevState = state;
    updateState(ExecutingSSH);

    // clear output buffers from previous execution
    stdOutSSH.clear();
    stdErrSSH.clear();

    // need a channel for each command
    LIBSSH2_CHANNEL *channel = libssh2_channel_open_session(session);
    if (!channel) {
        stdErrSSH.append(getErrorMessageSSH());
        updateState(prevState);
        return false;
    }

    // do the deed
    #ifdef QT_DEBUG
    qint64 start = QDateTime::currentMSecsSinceEpoch();
    #endif
    if (libssh2_channel_exec(channel, command.toUtf8().constData())) {
        stdErrSSH.append(getErrorMessageSSH());
        libssh2_channel_close(channel);
        libssh2_channel_wait_closed(channel);
        libssh2_channel_free(channel);
        updateState(prevState);
        return false;
    }
    #ifdef QT_DEBUG
    qint64 duration = QDateTime::currentMSecsSinceEpoch() - start;
    qWarning() << QString("%1 %2ms").arg(command.left(command.indexOf(" "))).arg(duration);
    #endif

    // read outputs

    QString tmp;
    char    bufferRaw[CHUNK_SIZE];

    ssize_t readCount = libssh2_channel_read(channel, bufferRaw, sizeof(bufferRaw));
    while ((readCount > 0) && !QThread::currentThread()->isInterruptionRequested()) {
        tmp.append(QByteArray(bufferRaw, readCount));
        readCount = libssh2_channel_read(channel, bufferRaw, sizeof(bufferRaw));
    }
    if (QThread::currentThread()->isInterruptionRequested()) {
        tmp = "";
    }
    stdOutSSH = tmp.split("\n");

    tmp.clear();

    readCount = libssh2_channel_read_stderr(channel, bufferRaw, sizeof(bufferRaw));
    while ((readCount > 0) && !QThread::currentThread()->isInterruptionRequested()) {
        tmp.append(QByteArray(bufferRaw, readCount));
        readCount = libssh2_channel_read_stderr(channel, bufferRaw, sizeof(bufferRaw));
    }
    if (QThread::currentThread()->isInterruptionRequested()) {
        tmp = "";
    }
    stdErrSSH = tmp.split("\n");

    // housekeeping
    libssh2_channel_close(channel);
    libssh2_channel_wait_closed(channel);
    libssh2_channel_free(channel);

    if (QThread::currentThread()->isInterruptionRequested()) {
        updateState(prevState);
        return false;
    };

    updateState(prevState);
    return true;
}


// private method
bool SSHClient::dirList(QString dir, DirList *contents)
{
    SSHClientState prevState = state;
    updateState(GettingDirList);

    dir.replace(QRegExp("/$"), "");

    // clear buffer from previous listing
    contents->clear();

    // open remote dir
    LIBSSH2_SFTP_HANDLE *sftpDirHandle = libssh2_sftp_opendir(sftpSession, dir.toUtf8().constData());
    if (!sftpDirHandle) {
        updateState(prevState);
        return false;
    }

    // get contents
    char                    buffer[256];
    LIBSSH2_SFTP_ATTRIBUTES attrs;
    int size = libssh2_sftp_readdir(sftpDirHandle, buffer, sizeof(buffer), &attrs);
    while (size && !QThread::currentThread()->isInterruptionRequested()) {
        DirListItem dirListItem;

        dirListItem.isDir    = LIBSSH2_SFTP_S_ISDIR(attrs.permissions);
        dirListItem.name     = QString(buffer);
        dirListItem.fullPath = QString("%1/%2%3").arg(dir).arg(dirListItem.name).arg(dirListItem.isDir ? "/" : "");

        contents->append(dirListItem);

        size = libssh2_sftp_readdir(sftpDirHandle, buffer, sizeof(buffer), &attrs);
    }

    // housekeeping
    libssh2_sftp_closedir(sftpDirHandle);

    if (QThread::currentThread()->isInterruptionRequested()) {
        contents->clear();
        updateState(prevState);
        return false;
    }

    updateState(prevState);
    return true;
}


// private method
bool SSHClient::download(QString source, QString destination)
{
    SSHClientState prevState = state;
    updateState(Downloading);

    // local destination
    QFile localFile(destination);

    // open local destination for writing
    if (!localFile.open(QIODevice::WriteOnly)) {
        updateState(prevState);
        return false;
    }

    // pair local destination with data stream
    QDataStream localOutput(&localFile);

    // open remote source for reading
    LIBSSH2_SFTP_HANDLE *sftpHandle = libssh2_sftp_open(sftpSession, source.toUtf8().constData(), LIBSSH2_FXF_READ, 0);
    if (!sftpHandle) {
        localFile.close();
        localFile.remove();
        updateState(prevState);
        return false;
    }

    // read source and write destination
    char    bufferRaw[CHUNK_SIZE];
    bool    wasError  = false;
    qint64  timeStart = QDateTime::currentMSecsSinceEpoch();
    ssize_t readCount = libssh2_sftp_read(sftpHandle, bufferRaw, sizeof(bufferRaw));
    while ((readCount > 0) && !QThread::currentThread()->isInterruptionRequested()) {
        mutex->lock();
        loadingBytes += readCount;
        mutex->unlock();
        emit stateChanged(config.id);

        if (localOutput.writeRawData(bufferRaw, readCount) != readCount) {
            wasError = true;
            break;
        }
        timeStart = QDateTime::currentMSecsSinceEpoch();
        readCount = libssh2_sftp_read(sftpHandle, bufferRaw, sizeof(bufferRaw));
    }
    mutex->lock();
    loadingBytes = 0;
    mutex->unlock();
    emit stateChanged(config.id);

    // close remote source
    libssh2_sftp_close(sftpHandle);

    // close local destination
    localFile.close();

    // don't leave partial file on disk if there was an error
    if (wasError || QThread::currentThread()->isInterruptionRequested()) {
        localFile.remove();
        updateState(prevState);
        return false;
    }

    updateState(prevState);
    return true;
}


// private method
bool SSHClient::upload(QString source, QString destination)
{
    SSHClientState prevState = state;
    updateState(Uploading);

    // local source
    QFile localFile(source);

    // open local source for reading
    if (!localFile.open(QIODevice::ReadOnly)) {
        updateState(prevState);
        return false;
    }

    QString destination_temp = QString("%1.%2").arg(destination).arg(QDateTime::currentMSecsSinceEpoch());

    // open remote source for writing
    LIBSSH2_SFTP_HANDLE *sftpHandle = libssh2_sftp_open(sftpSession, destination_temp.toUtf8().constData(), LIBSSH2_FXF_WRITE | LIBSSH2_FXF_CREAT, UPLOAD_CREATE_PERMISSIONS);
    if (!sftpHandle) {
        localFile.close();
        updateState(prevState);
        return false;
    }

    // read source and write temporary destination
    bool wasError = false;
    while (!localFile.atEnd() && !QThread::currentThread()->isInterruptionRequested()) {
        QByteArray bufferRaw = localFile.read(CHUNK_SIZE);
        int remaining        = bufferRaw.count();
        int offset           = 0;
        while (remaining > 0) {
            qint64  timeStart = QDateTime::currentMSecsSinceEpoch();
            ssize_t bytesWritten = libssh2_sftp_write(sftpHandle, bufferRaw.constData() + offset, remaining);

            if (bytesWritten < 0) {
                wasError = true;
                break;
            }

            mutex->lock();
            loadingBytes += bytesWritten;
            mutex->unlock();
            emit stateChanged(config.id);

            remaining -= bytesWritten;
            offset    += bytesWritten;
            updateState(Uploading);
        }
        if (wasError) {
            break;
        }
    }
    mutex->lock();
    loadingBytes = 0;
    mutex->unlock();
    emit stateChanged(config.id);

    // close local source
    localFile.close();

    // close remote destination
    libssh2_sftp_close(sftpHandle);

    // don't leave partial file on remote if there was an error
    if (wasError || QThread::currentThread()->isInterruptionRequested()) {
        executeSSH(QString("rm -f \"%1\"").arg(destination_temp));
        updateState(prevState);
        return false;
    }

    // check size
    if (!executeSSH("stat -c %s \"" + destination_temp + "\"")) {
        executeSSH(QString("rm -f \"%1\"").arg(destination_temp));
        updateState(prevState);
        return false;
    }
    bool OK = false;
    qint64 destination_temp_size = stdOutSSH.at(0).toLongLong(&OK);
    if (!OK) {
        executeSSH(QString("rm -f \"%1\"").arg(destination_temp));
        updateState(prevState);
        return false;
    }
    QFileInfo sourceInfo(source);
    if (sourceInfo.size() != destination_temp_size) {
        executeSSH(QString("rm -f \"%1\"").arg(destination_temp));
        updateState(prevState);
        return false;
    }

    // size OK, let's complete the upload
    updateState(prevState);
    return executeSSH(QString("mv -f \"%1\" \"%2\"").arg(destination_temp).arg(destination));
}


// private method
QString SSHClient::getErrorMessageSSH()
{
    char *sshErrorMessageRaw;
    int error_number = libssh2_session_last_error(session, &sshErrorMessageRaw, NULL, 0);

    if ((error_number == LIBSSH2_ERROR_NONE) || !sshErrorMessageRaw) {
        return "";
    }

    return QString(sshErrorMessageRaw);
}


// private slot
void SSHClient::socketError(QAbstractSocket::SocketError errorCode)
{
    QString errorStr;

    switch (errorCode) {
        case QAbstractSocket::ConnectionRefusedError:
            errorStr = "Connection Refused";
            break;
        case QAbstractSocket::RemoteHostClosedError:
            errorStr = "Remote Host Closed";
            break;
        case QAbstractSocket::HostNotFoundError:
            errorStr = "Host Not Found";
            break;
        case QAbstractSocket::SocketAccessError:
            errorStr = "Socket Access Error";
            break;
        case QAbstractSocket::SocketResourceError:
            errorStr = "Socket Resource Error";
            break;
        case QAbstractSocket::SocketTimeoutError:
            errorStr = "Socket Timeout";
            break;
        case QAbstractSocket::DatagramTooLargeError:
            errorStr = "Datagram Too Large";
            break;
        case QAbstractSocket::NetworkError:
            errorStr = "Network Error";
            break;
        case QAbstractSocket::AddressInUseError:
            errorStr = "Address In Use";
            break;
        case QAbstractSocket::SocketAddressNotAvailableError:
            errorStr = "Socket Address Not Available";
            break;
        case QAbstractSocket::UnsupportedSocketOperationError:
            errorStr = "Unsupported Socket Operation";
            break;
        case QAbstractSocket::ProxyAuthenticationRequiredError:
            errorStr = "Proxy Authentication Required";
            break;
        case QAbstractSocket::SslHandshakeFailedError:
            errorStr = "SSL Handshake Failed";
            break;
        case QAbstractSocket::UnfinishedSocketOperationError:
            errorStr = "Unfinished Socket Operation";
            break;
        case QAbstractSocket::ProxyConnectionRefusedError:
            errorStr = "Proxy Connection Refused";
            break;
        case QAbstractSocket::ProxyConnectionClosedError:
            errorStr = "Proxy Connection Closed";
            break;
        case QAbstractSocket::ProxyConnectionTimeoutError:
            errorStr = "Proxy Connection Timeout";
            break;
        case QAbstractSocket::ProxyNotFoundError:
            errorStr = "Proxy Not Found";
            break;
        case QAbstractSocket::ProxyProtocolError:
            errorStr = "Proxy Protocol";
            break;
        case QAbstractSocket::OperationError:
            errorStr = "Operation";
            break;
        case QAbstractSocket::SslInternalError:
            errorStr = "SSL Internal";
            break;
        case QAbstractSocket::SslInvalidUserDataError:
            errorStr = "SSL Invalid User Data";
            break;
        case QAbstractSocket::TemporaryError:
            errorStr = "Temporary Error";
            break;
        default:
            errorStr = "Unknown Error";
    }

    emit error(config.id, QString("Socket error '%1'").arg(errorStr));
}


// private slot
void SSHClient::socketStateChanged(QAbstractSocket::SocketState socketState)
{
    QString stateString;

    switch (socketState) {
        case QAbstractSocket::UnconnectedState:
            stateString = "Socket is not connected";
            if (connectAttempt < 3) {
                QTimer::singleShot(connectAttempt * 30000 + 5000, this, SLOT(autoConnect()));
                connectAttempt++;
            }
            else {
                emit disconnected(config.id);
            }
            break;
        case QAbstractSocket::HostLookupState:
            stateString = "Socket is looking up host";
            break;
        case QAbstractSocket::ConnectingState:
            stateString = "Socket is connecting";
            break;
        case QAbstractSocket::ConnectedState:
            stateString = "Socket is connected";
            connectAttempt = 0;
            break;
        case QAbstractSocket::BoundState:
            stateString = "Socket is bound";
            break;
        case QAbstractSocket::ClosingState:
            stateString = "Socket is closing";
            break;
        case QAbstractSocket::ListeningState:
            stateString = "Socket is listening";
            break;
    }

    emit info(config.id, stateString);
}


// public slot
void SSHClient::passwordEntryResult(int id, QString fingerprint, QString psw)
{
    if (id != config.id) {
        return;
    }

    password = psw;

    connectAuthPassword(fingerprint);
}


// public slot
void SSHClient::keySetupQuestionResult(int id, bool answer)
{
    if (id != config.id) {
        return;
    }

    if (answer) {
        connectCreateKeys();
        return;
    }
    connectCheckDir();
}


// public slot
void SSHClient::dirSelectorResult(int id, bool openOnly, QString path)
{
    if (id != config.id) {
        return;
    }

    // one dir up
    if (path.endsWith("/../")) {
        path.replace("/../", "");
        path.replace(path.lastIndexOf("/") + 1, 999999, "");
    }

    // user not done yet, needs to display contents of dir
    if (openOnly) {
        DirList dirContents;
        if (!dirList(path, &dirContents)) {
            disconnectSSH(config.id, "Could not get directory listing");
            return;
        }
        emit showDirSelector(config.id, formatUserHost(), path, dirContents);
        return;
    }

    // user is done selecting dir

    config.dir = path;

    emit updateConfig(config.id);

    // OK finally all connection sequence is done
    updateState(Idle);
    emit connected(config.id);
}


// public slot
void SSHClient::findAudio(int id, QString subdir)
{
    if (id != config.id) {
        return;
    }

    QStringList extensionFilters;
    foreach (QString extension, extensions) {
        extensionFilters.append(QString("-iname \"*%1\"").arg(extension));
    }

    QString dir = config.dir;
    if (!subdir.isEmpty()) {
        if (!dir.endsWith("/")) {
            dir = dir + "/";
        }
        dir = dir + subdir;
    }

    // execute find on remote
    QString command = QString("find \"%1\" -type f %2").arg(dir).arg(extensionFilters.join(" -o "));
    if (executeSSH(command)) {
        emit audioList(config.id, QStringList(stdOutSSH), false);
    }
    else {
        emit error(config.id, stdErrSSH.join(" "));
    }
}


// public slot
void SSHClient::getAudio(int id, QStringList remoteFiles)
{
    if (id != config.id) {
        return;
    }

    if (remoteFiles.count() < 1) {
        return;
    }

    // download sequence is interruped after each file with a timer, if currently there are no downloads in progress, then timer must be fired up
    bool startNow = downloadList.isEmpty();

    // add to download list
    downloadList.append(remoteFiles);

    // start if none in progress
    if (startNow) {
        QTimer::singleShot(25, this, SLOT(dowloadNext()));
    }
}


// public slot
void SSHClient::getOpenItems(int id, QString remotePath)
{
    if (id != config.id) {
        return;
    }

    if (remotePath.length() == 0) {
        return;
    }

    OpenTracks returnValue;

    DirList dirContents;
    if (dirList(remotePath, &dirContents)) {
        qSort(dirContents.begin(), dirContents.end(), [](DirListItem a, DirListItem b) {
            return (a.name.compare(b.name) < 0);
        });

        foreach (DirListItem content, dirContents) {
            if (content.name.startsWith(".")) {
                continue;
            }

            OpenTrack openTrack;
            openTrack.hasChildren = content.isDir;
            openTrack.id = QString("%1:%2").arg(content.fullPath).arg(config.id);
            openTrack.label = content.name;
            openTrack.selectable = !content.isDir;

            returnValue.append(openTrack);
        }
    }

    emit gotOpenItems(config.id, returnValue);
}


// public slot
void SSHClient::trackInfoUpdated(TrackInfo trackInfo)
{
    QString localPath = trackInfo.url.toLocalFile();

    // get remote dir listing
    QString remoteDir = localToRemote(localPath.left(localPath.lastIndexOf("/") + 1));
    DirList remoteDirContents;
    if (!dirList(remoteDir, &remoteDirContents)) {
        // probably remote dir doesn't exist, which means track belongs to another client (or connection problem)
        return;
    }

    // check to make sure the remote file exists too, could belong to another client
    bool exists = false;
    foreach (DirListItem dirListItem, remoteDirContents) {
        if (!dirListItem.isDir && (localToRemote(localPath).compare(dirListItem.fullPath) == 0)) {
            exists = true;
            break;
        }
    }
    if (!exists) {
        return;
    }

    if (!trackInfo.title.isEmpty() && !trackInfo.album.isEmpty() && !trackInfo.performer.isEmpty()) {
        bool OK         = true;
        bool downloaded = false;
        if (!QFileInfo::exists(localPath)) {
            if (!download(localToRemote(localPath), localPath)) {
                OK = false;
            }
            downloaded = true;
        }

        if (OK) {
            TagLib::FileRef fileRef(QFile::encodeName(localPath).constData());
            if (!fileRef.isNull()) {
                if (
                    (trackInfo.title.compare(TStringToQString(fileRef.tag()->title()))      == 0) &&
                    (trackInfo.performer.compare(TStringToQString(fileRef.tag()->artist())) == 0) &&
                    (trackInfo.album.compare(TStringToQString(fileRef.tag()->album()))      == 0) &&
                    (trackInfo.year == fileRef.tag()->year())                                     &&
                    (trackInfo.track  == fileRef.tag()->track())
                ) {
                    if (downloaded) {
                        QFile::remove(localPath);
                    }
                    return;
                }

                fileRef.tag()->setTitle(QStringToTString(trackInfo.title));
                fileRef.tag()->setAlbum(QStringToTString(trackInfo.album));
                fileRef.tag()->setArtist(QStringToTString(trackInfo.performer));
                fileRef.tag()->setYear(trackInfo.year);
                fileRef.tag()->setTrack(trackInfo.track);
                fileRef.save();

                upload(localPath, localToRemote(localPath));
            }
            if (downloaded) {
                QFile::remove(localPath);
            }
        }
    }


    // check if picture needs to be uploaded
    foreach (QUrl localPictureUrl, trackInfo.pictures) {
        exists = false;
        foreach (DirListItem dirListItem, remoteDirContents) {
            if (!dirListItem.isDir && (localToRemote(localPictureUrl.toLocalFile()).compare(dirListItem.fullPath) == 0)) {
                exists = true;
                break;
            }
        }
        if (!exists) {
            upload(localPictureUrl.toLocalFile(), localToRemote(localPictureUrl.toLocalFile()));
        }
    }
}

// timer slot
void SSHClient::dowloadNext()
{
    if (downloadList.isEmpty()) {
        return;
    }

    // what to download
    QString remoteFile = downloadList.first();
    downloadList.removeFirst();

    QString remoteDir = remoteFile.left(remoteFile.lastIndexOf("/"));

    // keep same dir structure as on remote
    QDir localDir(remoteToLocal(remoteDir));

    // create dir
    if (!localDir.exists()) {
        localDir.mkpath(localDir.absolutePath());
    }

    // download pictures
    QFileInfoList entries = localDir.entryInfoList(QDir::AllDirs | QDir::Files | QDir::NoDotAndDotDot);
    if (entries.count() < 1) {
        DirList dirContents;
        if (dirList(remoteDir, &dirContents)) {
            foreach (DirListItem dirContent, dirContents) {
                if (!dirContent.isDir && (dirContent.name.endsWith(".jpg", Qt::CaseInsensitive) || dirContent.name.endsWith(".jpeg", Qt::CaseInsensitive) || dirContent.name.endsWith(".png", Qt::CaseInsensitive))) {
                    download(dirContent.fullPath, localDir.absoluteFilePath(dirContent.name));
                }
            }
        }
    }

    // download audio file itself
    QString destination = remoteToLocal(remoteFile);
    if (download(remoteFile, destination)) {
        emit gotAudio(config.id, remoteFile, destination);
    }

    // start next download with a delay so other signals can be processed too
    QTimer::singleShot(250, this, SLOT(dowloadNext()));
}


// helper
void SSHClient::updateState(SSHClientState state)
{
    mutex->lock();
    this->state      = state;
    mutex->unlock();
    emit stateChanged(config.id);
}


// public method
SSHClient::SSHClientState SSHClient::getState()
{
    mutex->lock();
    SSHClientState state = this->state;
    mutex->unlock();

    return state;
}


// public method
qint64 SSHClient::getLoadingBytes()
{
    mutex->lock();
    qint64 loadingBytes = this->loadingBytes;
    mutex->unlock();

    return loadingBytes;
}
