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

    socket      = NULL;
    session     = NULL;
    sftpSession = NULL;

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
}


// thread entry point
void SSHClient::run()
{
    // instantiate socket (must be in this thread)
    socket = new QTcpSocket();

    // socket signals
    connect(socket, SIGNAL(connected()),                                this, SLOT(socketConnected()));
    connect(socket, SIGNAL(error(QAbstractSocket::SocketError)),        this, SLOT(socketError(QAbstractSocket::SocketError)));
    connect(socket, SIGNAL(stateChanged(QAbstractSocket::SocketState)), this, SLOT(socketStateChanged(QAbstractSocket::SocketState)));

    // auto-connect
    connectSSH(config.id);
}


// public method
SSHClient::SSHClientConfig SSHClient::getConfig()
{
    return config;
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
bool SSHClient::executeSSH(QString command)
{
    // clear output buffers from previous execution
    stdOutSSH.clear();
    stdErrSSH.clear();

    // need a channel for each command
    LIBSSH2_CHANNEL *channel = libssh2_channel_open_session(session);
    if (!channel) {
        stdErrSSH.append(getErrorMessageSSH());
        return false;
    }

    // do the deed
    if (libssh2_channel_exec(channel, command.toUtf8().constData())) {
        stdErrSSH.append(getErrorMessageSSH());
        libssh2_channel_close(channel);
        libssh2_channel_wait_closed(channel);
        libssh2_channel_free(channel);
        return false;
    }

    // read outputs

    QString tmp;
    char    bufferRaw[65536];

    ssize_t readCount = libssh2_channel_read(channel, bufferRaw, sizeof(bufferRaw));
    while (readCount > 0) {
        tmp.append(QByteArray(bufferRaw, readCount));
        readCount = libssh2_channel_read(channel, bufferRaw, sizeof(bufferRaw));
    }
    stdOutSSH = tmp.split("\n");

    tmp.clear();

    readCount = libssh2_channel_read_stderr(channel, bufferRaw, sizeof(bufferRaw));
    while (readCount > 0) {
        tmp.append(QByteArray(bufferRaw, readCount));
        readCount = libssh2_channel_read_stderr(channel, bufferRaw, sizeof(bufferRaw));
    }
    stdErrSSH = tmp.split("\n");

    // housekeeping
    libssh2_channel_close(channel);
    libssh2_channel_wait_closed(channel);
    libssh2_channel_free(channel);

    return true;
}


// private method
bool SSHClient::dirList(QString dir, DirList *contents)
{
    dir.replace(QRegExp("/$"), "");

    // clear buffer from previous listing
    contents->clear();

    // open remote dir
    LIBSSH2_SFTP_HANDLE *sftpDirHandle = libssh2_sftp_opendir(sftpSession, dir.toUtf8().constData());
    if (!sftpDirHandle) {
        return false;
    }

    // get contents
    char                    buffer[256];
    LIBSSH2_SFTP_ATTRIBUTES attrs;
    int size = libssh2_sftp_readdir(sftpDirHandle, buffer, sizeof(buffer), &attrs);
    while (size) {
        DirListItem dirListItem;

        dirListItem.isDir    = LIBSSH2_SFTP_S_ISDIR(attrs.permissions);
        dirListItem.name     = QString(buffer);
        dirListItem.fullPath = QString("%1/%2%3").arg(dir).arg(dirListItem.name).arg(dirListItem.isDir ? "/" : "");

        contents->append(dirListItem);

        size = libssh2_sftp_readdir(sftpDirHandle, buffer, sizeof(buffer), &attrs);
    }

    // housekeeping
    libssh2_sftp_closedir(sftpDirHandle);

    return true;
}


// private method
bool SSHClient::download(QString source, QString destination)
{
    // local destination
    QFile localFile(destination);

    // open local destination for writing
    if (!localFile.open(QIODevice::WriteOnly)) {
        return false;
    }

    // pair local destination with data stream
    QDataStream localOutput(&localFile);

    // open remote source for reading
    LIBSSH2_SFTP_HANDLE *sftpHandle = libssh2_sftp_open(sftpSession, source.toUtf8().constData(), LIBSSH2_FXF_READ, 0);
    if (!sftpHandle) {
        localFile.close();
        localFile.remove();
        return false;
    }

    // read source and write destination

    char bufferRaw[65536];

    ssize_t readCount = libssh2_sftp_read(sftpHandle, bufferRaw, sizeof(bufferRaw));
    while (readCount > 0) {
        if (localOutput.writeRawData(bufferRaw, readCount) != readCount) {
            localFile.close();
            localFile.remove();
            return false;
        }
        readCount = libssh2_sftp_read(sftpHandle, bufferRaw, sizeof(bufferRaw));
    }

    // close remote source
    libssh2_sftp_close(sftpHandle);

    // close local destination
    localFile.close();

    return true;
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
            disconnectSSH(config.id);
            stateString = "Socket is not connected";
            break;
        case QAbstractSocket::HostLookupState:
            stateString = "Socket is looking up host";
            break;
        case QAbstractSocket::ConnectingState:
            stateString = "Socket is connecting";
            break;
        case QAbstractSocket::ConnectedState:
            stateString = "Socket is connected";
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
        default:
            stateString = "Socket is in an unknown state";
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

    // not done yet, needs to display contents of this dir
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
    emit connected(config.id);
}


// public slot
void SSHClient::findAudio(int id)
{
    if (id != config.id) {
        return;
    }

    QStringList extensions;

    // TODO re-think this, are all of them really needed?
    extensions.append("-iname \"*.3gp\"");
    extensions.append("-iname \"*.aa\"");
    extensions.append("-iname \"*.aac\"");
    extensions.append("-iname \"*.aax\"");
    extensions.append("-iname \"*.act\"");
    extensions.append("-iname \"*.aiff\"");
    extensions.append("-iname \"*.amr\"");
    extensions.append("-iname \"*.ape\"");
    extensions.append("-iname \"*.au\"");
    extensions.append("-iname \"*.awb\"");
    extensions.append("-iname \"*.dct\"");
    extensions.append("-iname \"*.dss\"");
    extensions.append("-iname \"*.dvf\"");
    extensions.append("-iname \"*.flac\"");
    extensions.append("-iname \"*.gsm\"");
    extensions.append("-iname \"*.iklax\"");
    extensions.append("-iname \"*.ivs\"");
    extensions.append("-iname \"*.m4a\"");
    extensions.append("-iname \"*.m4b\"");
    extensions.append("-iname \"*.m4p\"");
    extensions.append("-iname \"*.mmf\"");
    extensions.append("-iname \"*.mp3\"");
    extensions.append("-iname \"*.mpc\"");
    extensions.append("-iname \"*.msv\"");
    extensions.append("-iname \"*.nsf\"");
    extensions.append("-iname \"*.ogg,\"");
    extensions.append("-iname \"*.opus\"");
    extensions.append("-iname \"*.ra,\"");
    extensions.append("-iname \"*.raw\"");
    extensions.append("-iname \"*.sln\"");
    extensions.append("-iname \"*.tta\"");
    extensions.append("-iname \"*.vox\"");
    extensions.append("-iname \"*.wav\"");
    extensions.append("-iname \"*.wma\"");
    extensions.append("-iname \"*.wv\"");
    extensions.append("-iname \"*.webm\"");
    extensions.append("-iname \"*.8svx\"");

    // execute find on remote
    QString command = QString("find \"%1\" -type f %2").arg(config.dir).arg(extensions.join(" -o "));
    if (executeSSH(command)) {
        emit audioList(config.id, QStringList(stdOutSSH));
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


// timer slot
void SSHClient::dowloadNext()
{
    if (downloadList.isEmpty()) {
        return;
    }

    // what to download
    QString remoteFile = downloadList.first();
    downloadList.removeFirst();

    // keep same dir structure as on remote
    QDir localDir = QDir(QString("%1/%2").arg(config.cacheDir).arg(remoteFile.left(remoteFile.lastIndexOf("/")).replace(config.dir, "")));

    // create dir and download pictures too
    if (!localDir.exists()) {
        localDir.mkpath(localDir.absolutePath());

        DirList dirContents;
        if (dirList(remoteFile.left(remoteFile.lastIndexOf("/")), &dirContents)) {
            foreach (DirListItem dirContent, dirContents) {
                if (!dirContent.isDir && (dirContent.name.endsWith(".jpg", Qt::CaseInsensitive) || dirContent.name.endsWith(".jpeg", Qt::CaseInsensitive) || dirContent.name.endsWith(".png", Qt::CaseInsensitive))) {
                    download(dirContent.fullPath, localDir.absoluteFilePath(dirContent.name));
                }
            }
        }
    }

    // download audio file itself
    QString destination = localDir.absoluteFilePath(remoteFile.mid(remoteFile.lastIndexOf("/") + 1));
    if (download(remoteFile, destination)) {
        emit gotAudio(config.id, remoteFile, destination);
    }

    // start next with a delay so other signals can be processed too
    QTimer::singleShot(250, this, SLOT(dowloadNext()));
}
