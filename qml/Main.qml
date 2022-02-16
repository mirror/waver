import QtQml 2.3
import QtQuick 2.12
import QtQuick.Controls 2.3
import QtQuick.Controls.Material 2.3
import QtQuick.Controls.Universal 2.3
import QtQuick.Layouts 1.3

ApplicationWindow {
    id: applicationWindow
    height: 480
    minimumHeight: 320
    width: 640
    minimumWidth: 480
    visible: true
    title: "Waver"

    signal addNewServer(string host, string user, string psw);
    signal deleteServer(string id);
    signal setServerPassword(string id, string psw);
    signal explorerItemClicked(string id, int action, string extra);
    signal explorerChildrenExtra(string childrenExtraJSON);
    signal playlistItemClicked(int index, int action);
    signal playlistItemDragDropped(int index, int destinationIndex);
    signal positioned(double percent);
    signal saveGeometry(int x, int y, int width, int height);
    signal previousButton(int index);
    signal nextButton();
    signal playButton();
    signal pauseButton();
    signal stopButton();
    signal favoriteButton(bool fav);
    signal requestOptions();
    signal updatedOptions(string optionsJSON);
    signal requestLog();
    signal peakUILag();

    onClosing: {
        saveGeometry(x, y, width, height);
    }

    onWidthChanged: {
        titleSizeRecalcOnResizeTimer.restart();
    }
    onHeightChanged: {
        titleSizeRecalcOnResizeTimer.restart();
    }


    function bringToFront()
    {
        applicationWindow.raise();
    }

    function explorerAddItem(id, parent, title, image, extra, expandable, playable, selectable, selected)
    {
        explorer.addItem(id, parent, title, image, extra, expandable, playable, selectable, selected);
    }

    function explorerDisableQueueable(id)
    {
        explorer.disableQueueable(id);
    }

    function explorerRemoveAboveLevel(id)
    {
        explorer.removeAboveLevel(id);
    }

    function explorerRemoveChildren(id)
    {
        explorer.removeChildren(id);
    }

    function explorerRemoveItem(id)
    {
        explorer.removeItem(id);
    }

    function explorerSetBusy(id, busy)
    {
        explorer.setBusy(id, busy);
    }

    function explorerSetError(id, isError, errorMessage)
    {
        explorer.setError(id, isError, errorMessage);
    }

    function explorerSetFlagExtra(id, flag)
    {
        explorer.setFlagExtra(id, flag);
    }

    function explorerSetSelected(id, selected)
    {
        explorer.setSelected(id, selected);
    }

    function explorerToggleSelected(id)
    {
        explorer.toggleSelected(id);
    }

    function historyAdd(title)
    {
        var newDict = {
            title: title
        }
        historyModel.insert(0, newDict);
    }

    function logAsRequested(logText)
    {
        log.setLog(logText);
        log.open();
    }

    function logUpdate(newText)
    {
        if (log.visible) {
            log.addLog(newText);
        }
    }

    function optionsAsRequested(optionsObj)
    {
        options.setOptions(optionsObj)
        options.open();
    }

    function playlistAddItem(title, artist, group, image, selected)
    {
        playlist.addItem(title, artist, group, image, selected);
    }

    function playlistBusy(index, busy)
    {
        playlist.setBusy(index, busy);
    }

    function playlistDecoding(index, downloadPercent, pcmPercent)
    {
        playlist.setDecoding(index, downloadPercent, pcmPercent)
    }

    function  playlistBigBusy(busy)
    {
        playlist.setPlaylistBigBusy(busy);
    }

    function playlistTotalTime(totalTime)
    {
        playlist.setTotalTime(totalTime);
    }

    function playlistClearItems()
    {
        playlist.clearItems();
    }

    function playlistSelected(index, selected)
    {
        playlist.setSelected(index, selected);
    }

    function promptServerPsw(id, formattedName)
    {
        serverPassword.promptAdd(id, formattedName);

        if (!serverPassword.visible) {
            serverPassword.open();
        }
    }

    function quickStartGuideSetIsSnap(isSnap)
    {
        quickStartGuide.setIsSnap(isSnap);
    }

    function setFavorite(fav)
    {
        favorite.checked = fav
    }

    function setImage(image)
    {
        art.swapImage(image);
    }

    function setPeakFPS(fpsText)
    {
        peakFPS.text = fpsText;
    }

    function setPeakMeter(l, r, scheduledTimeMS)
    {
        peakMeter.setPeak(l, r);

        if (Math.abs(Date.now() - scheduledTimeMS) >= 10) {
            peakUILag();
        }
    }

    function setPeakMeterReplayGain(g)
    {
        peakMeter.setReplayGain(g);
    }

    function setShuffleCountdown(percent)
    {
        internal.shuffleCountdown = percent
    }

    function setStatusTempText(statusText)
    {
        statusTemp.text  = statusText;
        statusTemp.visible = true;

        tags.visible = false;
        status.visible = false;
        peakFPS.visible = false;

        statusTempTimer.restart();
    }

    function setStatusText(statusText)
    {
        status.text = statusText;
    }

    function setTempImage(image)
    {
        art.swapTempImage(image);
    }

    function setTrackBusy(busy)
    {
        networkBusy.visible = busy;
    }

    function setTrackData(titleText, performerText, albumText, trackNumberText, yearText)
    {
        title.text       = titleText.replace(" {", "\n{");
        performer.text   = performerText;
        album.text       = albumText;
        trackNumber.text = "#" + trackNumberText;
        year.text        = yearText;
    }

    function setTrackLength(lengthText)
    {
        length.text = lengthText;
    }

    function setTrackPosition(positionText, positionPercent)
    {
        position.text = positionText;

        if (!positioner.pressed) {
            positioner.value = positionPercent;
        }
    }

    function setTrackDecoding(downloadPercent, pcmPercent)
    {
        if (positioner.downloadedPercent !== downloadPercent) {
            positioner.downloadedPercent = downloadPercent;
        }
        if (positioner.decodedPercent !== pcmPercent) {
            positioner.decodedPercent = pcmPercent
        }
    }

    function setTrackTags(tagsText)
    {
        tags.text = tagsText;
    }


    QtObject {
        id: internal

        readonly property int outlinePixelSize: 28

        property double positionerMovedValue: -1
        property double shuffleCountdown: 0.5

        function calculateTitleSize()
        {
            title.font.pixelSize = 99;
            while ((title.font.pixelSize >= 8) && ((title.width > track.width - art.width) || (title.height > art.height / 12 * 5))) {
                title.font.pixelSize--;
            }
        }

        function calculatePerformerSize()
        {
            performer.font.pixelSize = 99;
            while ((performer.font.pixelSize >= 8) && ((performer.width > track.width - art.width) || (performer.contentHeight > performer.height - 20))) {
                performer.font.pixelSize--;
            }
        }
    }

    Timer {
        id: statusTempTimer
        interval: 3333

        onTriggered: {
            statusTemp.visible = false;

            tags.visible = true;
            status.visible = true;
            peakFPS.visible = true;
        }
    }
    Timer {
        id: titleSizeRecalcOnResizeTimer
        interval: 100

        onTriggered: {
            internal.calculateTitleSize();
            internal.calculatePerformerSize();
        }
    }


    header: ToolBar {
        RowLayout {
            anchors.fill: parent

            ToolButton {
                icon.name: 'media-skip-backward'
                icon.source: "qrc:///icons/skip_previous.ico"
                onClicked: {
                    if (historyModel.count > 0) {
                        historyMenu.popup();
                    }
                }
            }
            ToolButton {
                icon.name: 'media-playback-start'
                icon.source: "qrc:///icons/play.ico"
                onClicked: playButton()
            }
            ToolButton {
                icon.name: 'media-playback-pause'
                icon.source: "qrc:///icons/pause.ico"
                onClicked: pauseButton()
            }
            ToolButton {
                icon.name: 'media-playback-stop'
                icon.source: "qrc:///icons/stop.ico"
                onClicked: stopButton()
            }
            ToolButton {
                icon.name: 'media-skip-forward'
                icon.source: "qrc:///icons/skip_next.ico"
                onClicked: nextButton()
            }
            ToolSeparator {
            }
            ToolButton {
                id: favorite
                checkable: true
                icon.name: 'starred'
                icon.source: "qrc:///icons/star.ico"
                onClicked: favoriteButton(checked)
            }
            ToolButton {
                icon.name: 'search'
                icon.source: "qrc:///icons/search.ico"
                enabled: title.text.length && performer.text.length
                onClicked: searchMenu.popup();
            }
            Label {
                Layout.fillWidth: true
            }
            ToolButton {
                icon.name: 'open-menu'
                icon.source: "qrc:///icons/menu.ico"
                onClicked: menu.popup();
            }
        }
    }

    footer: ToolBar {
        Item {
            anchors.fill: parent

            Label {
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                id: tags
                text: "-"
                elide: Text.ElideRight
                maximumLineCount: 1
                leftPadding: 5
                width: parent.width / 10 * 8
            }
            Label {
                anchors.left: tags.right
                anchors.verticalCenter: parent.verticalCenter
                id: status
                text: qsTr("Idle")
                font.family: "Monospace"
                font.pixelSize: textMetrics.font.pixelSize * 0.8
                leftPadding: 5
                width: parent.width / 10
            }
            Label {
                anchors.left: status.right
                anchors.verticalCenter: parent.verticalCenter
                id: peakFPS
                text: ""
                font.family: "Monospace"
                font.pixelSize: textMetrics.font.pixelSize * 0.8
                textFormat: Qt.RichText
                leftPadding: 5
                width: parent.width / 10
            }
            Label {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                id: statusTemp
                color: statusTemp.palette.buttonText
                leftPadding: 5
                rightPadding: 5
                visible: false
            }
        }
    }


    Menu {
        id: menu
        rightMargin: 10

        MenuItem {
            text: qsTr("Servers")
            onTriggered: {
                servers.serversModel = explorer.getServersForDialog();
                servers.open();
            }
        }
        MenuItem {
            text: qsTr("Options")
            onTriggered: requestOptions()
        }
        MenuItem {
            text: qsTr("Log")
            onTriggered: requestLog()
        }
        MenuSeparator { }
        MenuItem {
            text: qsTr("Quick Start Guide")
            onTriggered: quickStartGuide.open()
        }
        MenuItem {
            text: qsTr("About")
            onTriggered: aboutWaver.open()
        }
        MenuSeparator { }
        MenuItem {
            text: qsTr("Quit")
            onTriggered: close();
        }
    }

    ListModel {
        id: historyModel
    }

    Menu {
        id: historyMenu
        leftMargin: 10

        Instantiator {
            model: historyModel

            delegate: MenuItem {
                text: title
                onTriggered: previousButton(index)
            }

            onObjectAdded: historyMenu.insertItem(index, object)
            onObjectRemoved: historyMenu.removeItem(object)
        }
    }

    Menu {
        id: searchMenu

        MenuItem {
            text: qsTr("Lyrics")
            onTriggered: Qt.openUrlExternally("https://google.com/search?q=" + performer.text + " " + title.text + " lyrics");
        }
        MenuItem {
            text: qsTr("Artist")
            onTriggered: Qt.openUrlExternally("https://google.com/search?q=\"" + performer.text + "\" band");
        }
    }


    TextMetrics {
        id: textMetrics
        font: trackNumber.font
        text: "Áy:"
    }

    Servers {
        id: servers

        anchors.centerIn: parent
        height: parent.height * 0.9
        width: parent.width * 0.9

        onAddServer: addNewServer(host, user, psw)
        onDelServer: {
            serverDeleteConfirmation.serverId = id;
            serverDeleteConfirmation.open();
        }
    }

    ServerPassword {
        id: serverPassword

        anchors.centerIn: parent
        height: parent.height * 0.9
        width: parent.width * 0.9

        onSetPassword: setServerPassword(id, psw);
    }

    Options {
        id: options

        anchors.centerIn: parent
        height: parent.height * 0.9
        width: parent.width * 0.9

        onOptionsSending: {
            updatedOptions(optionsJSON);
        }
    }

    Log {
        id: log

        anchors.centerIn: parent
        height: parent.height * 0.9
        width: parent.width * 0.9
    }

    About {
        id: aboutWaver

        anchors.centerIn: parent
        height: parent.height * 0.9
        width: parent.width * 0.9
    }

    QuickStart {
        id: quickStartGuide

        anchors.centerIn: parent
        height: parent.height * 0.9
        width: parent.width * 0.9
    }

    Dialog {
        id: serverDeleteConfirmation

        property string serverId: ""

        anchors.centerIn: parent
        height: parent.height * 0.75
        width: parent.width * 0.75

        modal: true
        standardButtons: Dialog.Yes | Dialog.Cancel
        title: qsTr("Confirmation Required")

        onAccepted: deleteServer(serverId);

        Label {
            anchors.fill: parent
            text: qsTr("Delete server?")
        }
    }

    Dialog {
        id: searchCriteria

        property string searchId: ""
        property int    searchAction: 0

        anchors.centerIn: parent
        focus: true
        height: parent.height * 0.75
        width: parent.width * 0.75

        modal: true
        standardButtons: Dialog.Ok | Dialog.Cancel

        title: qsTr("Search Criteria")

        onAccepted: {
            var extraObj = {};
            extraObj["criteria"] = searchText.text;

            explorerItemClicked(searchId, searchAction, JSON.stringify(extraObj));
        }

        TextField {
            id: searchText
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            focus: true
            onAccepted: searchCriteria.accept()
        }
    }


    Explorer {
        id: explorer

        anchors.bottom: parent.bottom
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.margins: 5
        width: parent.width / 3

        borderVisible: true
        imageSize: parent.width <= 640 ? 16 : parent.width <= 1280 ? 24 : 32

        onItemClicked: {
            if (id.startsWith("E")) {
                searchCriteria.searchId = id;
                searchCriteria.searchAction = action;
                searchCriteria.open();
                return;
            }
            explorerItemClicked(id, action, JSON.stringify(extra));
        }
    }

    Item {
        id: rightSide

        anchors.bottom: parent.bottom
        anchors.top: parent.top
        anchors.left: explorer.right
        anchors.right: parent.right
        anchors.margins: 5

        Item {
            id: track

            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            height: art.height

            Label {
                id: title

                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: art.left
                anchors.rightMargin: 5

                color: title.palette.highlight
                font.bold: true
                horizontalAlignment: Text.AlignHCenter
                text: "title"
                style: { font.pixelSize >= internal.outlinePixelSize ? Text.Outline : Text.Normal }
                styleColor: title.palette.windowText
                wrapMode: Text.Wrap

                onTextChanged: {
                    internal.calculateTitleSize();
                    titleSizeRecalcOnResizeTimer.restart();
                }
            }

            Label {
                id: performer

                anchors.top: title.bottom
                anchors.bottom: albumBackground.top
                anchors.left: parent.left
                anchors.right: art.left
                anchors.rightMargin: 5

                color: performer.palette.highlight
                horizontalAlignment: Text.AlignHCenter
                text: "performer"
                style: { font.pixelSize >= internal.outlinePixelSize ? Text.Outline : Text.Normal }
                styleColor: performer.palette.windowText
                verticalAlignment: Text.AlignVCenter
                wrapMode: Text.Wrap

                onTextChanged: {
                    internal.calculatePerformerSize();
                    titleSizeRecalcOnResizeTimer.restart();
                }
            }

            Rectangle {
                id: albumBackground

                anchors.bottom: parent.bottom
                anchors.left: parent.left
                anchors.right: art.left
                anchors.rightMargin: 5
                height: album.height + 6

                color: album.palette.highlight
                border.color: album.palette.windowText
                border.width: 1
                radius: 7
            }

            Label {
                id: trackNumber

                anchors.verticalCenter: albumBackground.verticalCenter
                anchors.left: albumBackground.left
                anchors.leftMargin: 7

                color: title.palette.highlightedText
                font.italic: true
                text: "trackNumber"
            }

            Label {
                id: album

                anchors.left: trackNumber.right
                anchors.right: year.left
                anchors.verticalCenter: albumBackground.verticalCenter

                color: title.palette.highlightedText
                font.pixelSize: textMetrics.font.pixelSize * (Math.min(title.font.pixelSize, performer.font.pixelSize) >= internal.outlinePixelSize ? 1.5 : 1)
                horizontalAlignment: Text.AlignHCenter
                text: "album"
                wrapMode: Text.Wrap
            }

            Label {
                id: year

                anchors.verticalCenter: albumBackground.verticalCenter
                anchors.right: albumBackground.right
                anchors.rightMargin: 7

                color: title.palette.highlightedText
                font.italic: true
                text: "year"
            }

            ImgSwp {
                id: art

                anchors.right: parent.right
                anchors.top: parent.top
                width: parent.width / 3
                height: width

                defaultImage: "qrc:///images/waver.png"
            }

            BusyIndicator {
                id: networkBusy
                anchors.fill: art
                visible: false
            }
        }

        Item {
            id: buffer

            anchors.top: track.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            height: positioner.implicitHeight > textMetrics.boundingRect.height ? positioner.implicitHeight : textMetrics.boundingRect.height

            Label {
                id: position

                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter

                font.pixelSize: textMetrics.font.pixelSize * 0.8
                text: "position"
            }

            Slider {
                id: positioner

                property double decodedPercent: 0.4
                property double downloadedPercent: 0.6

                anchors.left: position.right
                anchors.right: length.left
                anchors.verticalCenter: parent.verticalCenter
                anchors.leftMargin: 5
                anchors.rightMargin: 5

                live: true
                value: 0.25

                background: Rectangle {
                    x: positioner.leftPadding
                    y: positioner.topPadding + positioner.availableHeight / 2 - height / 2
                    width: positioner.availableWidth * positioner.downloadedPercent
                    height: 1
                    color: positioner.palette.windowText

                    Rectangle {
                        x: positioner.downloadedPercent < positioner.decodedPercent ? parent.width : 0
                        width: positioner.availableWidth * positioner.decodedPercent - x
                        height: 1
                        color: positioner.palette.highlight;
                    }

                    Rectangle {
                        y: -1
                        width: positioner.visualPosition * parent.width
                        height: 3
                        color: ((Qt.platform.os === "windows") || (Qt.platform.os === "winrt")) ? Universal.accent : Material.accent;
                    }
                }

                onMoved: internal.positionerMovedValue = positioner.value;
                onPressedChanged: {
                    if (!positioner.pressed && (internal.positionerMovedValue >= 0)) {
                        positioned(internal.positionerMovedValue);
                        internal.positionerMovedValue = -1;
                    }
                }
            }

            Label {
                id: length

                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter

                font.pixelSize: textMetrics.font.pixelSize * 0.8
                text: "length"
            }
        }

        PeakMeter {
            id: peakMeter

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: buffer.bottom
            height: 25
        }

        Playlist {
            id: playlist

            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: peakMeter.bottom
            anchors.topMargin: 5

            borderVisible: true
            imageSize: parent.width <= 640 ? 24 : parent.width <= 1280 ? 36 : 48

            onItemClicked: playlistItemClicked(index, action)
            onItemDragDropped: playlistItemDragDropped(index, destinationIndex)
        }

        Rectangle {
            id: shuffleCountdown

            anchors.bottom: playlist.top
            anchors.left: playlist.left
            height: 3
            width: playlist.width * internal.shuffleCountdown

            color: palette.highlight
        }
    }
}
