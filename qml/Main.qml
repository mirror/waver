import QtQml 2.3
import QtQuick 2.12
import QtQuick.Controls 2.3
import QtQuick.Controls.Material 2.3
import QtQuick.Controls.Universal 2.3
import QtQuick.Layouts 1.3
import QtQml.Models 2.15

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
    signal playlistExplorerItemDragDropped(string id, string extra, int destinationIndex);
    signal positioned(double percent);
    signal saveGeometry(int x, int y, int width, int height);
    signal previousButton(int index);
    signal nextButton();
    signal playButton();
    signal pauseButton();
    signal ppButton();
    signal stopButton();
    signal favoriteButton(bool fav);
    signal requestOptions();
    signal updatedOptions(string optionsJSON);
    signal requestEQ(int eq_chooser)
    signal requestLog();
    signal peakUILag();
    signal searchCriteriaEntered(string criteria);
    signal searchResult(string parent, string id, string extra);

    onClosing: {
        saveGeometry(x, y, width, height);
    }

    onWidthChanged: {
        titleSizeRecalcOnResizeTimer.restart();
    }
    onHeightChanged: {
        titleSizeRecalcOnResizeTimer.restart();
    }

    onActiveChanged: {
        if (active) {
            rightSide.focus = true;
        }
    }

    function bringToFront()
    {
        applicationWindow.raise();
    }

    function explorerAddItem(id, parent, title, image, extra, expandable, playable, selectable, selected)
    {
        explorer.addItem(id, parent, title, image, extra, expandable, playable, selectable, selected);
    }

    function explorerGetSearchResult(parentId, index)
    {
        var id = explorer.getSearchResultId(parentId, index);
        if (!id) {
            searchResult(parentId, "", {});
            return;
        }

        var extra = explorer.getExtra(id);
        searchResult(parentId, id, JSON.stringify(extra));
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

    function explorerSetTitle(id, title)
    {
        explorer.setTitle(id, title);
    }

    function explorerSortChildren(id)
    {
        explorer.sortChildren(id);
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

    function historyRemove(count)
    {
        historyModel.remove(0, count);
    }

    function optionsAsRequested(optionsObj)
    {
        options.setOptions(optionsObj)
        options.open();
    }

    function eqAsRequested(eqObj)
    {
        options.setEQ(eqObj);
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

    function playlistBigBusy(busy)
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

    function setFontSize(fs)
    {
        if (fs < 8) {
            fs = 8;
        }
        if (fs > 16) {
            fs = 16;
        }

        internal.fontSize = fs
        textMetrics.font.pointSize = fs
        aboutWaver.fontSize = fs
        explorer.fontSize = fs;
        explorer.imageSize = fs * 2.25 > 24 ? fs * 2.25 : 24;
        playlist.fontSize = fs;
        playlist.imageSize = fs * 3 > 36 ? fs * 3 : 36;
    }

    function setImage(image)
    {
        art.swapImage(image);
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
        artistSummaryContainer.visible = false;

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

    function setTitleCurlySpecial(tcs)
    {
        internal.titleCurlySpecial = tcs;
        explorer.titleCurlySpecial = tcs;
        playlist.titleCurlySpecial = tcs;
    }

    function setTrackAmpacheURL(url)
    {
        internal.searchAmpacheURL = url;
    }

    function setTrackBusy(busy)
    {
        networkBusy.visible = busy;
    }

    function setTrackData(titleText, performerText, albumText, trackNumberText, yearText, artistSummaryText)
    {
        if (internal.titleCurlySpecial) {
            var curly = titleText.indexOf('{');
            if (curly >= 0) {
                albumText = albumText + titleText.substr(curly).replace(/{/g, "\n{ ").replace(/}/g, " }");
                titleText = titleText.substr(0, curly).trim();
            }
        }

        title.text       = titleText;
        performer.text   = performerText;
        album.text       = albumText;
        trackNumber.text = "#" + trackNumberText;
        year.text        = yearText;

        artistSummaryText = artistSummaryText.replace(/\n/g, " ");
        artistSummary.text = artistSummaryText;
        artistSummaryMetrics.text = artistSummaryText;
        artistSummaryAnimation.running = artistSummaryText.length > 0
        if (artistSummaryText.length > 0) {
            artistSummaryPauser.restart();
        }
    }

    function setTrackLength(lengthText)
    {
        length.text = lengthText;
    }

    function setTrackPosition(positionText, positionPercent)
    {
        position.text = positionText;

        if (!positioner.pressed && !internal.kbPositioning) {
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


    function showSearchCriteria()
    {
        searchCriteria.clear();
        searchCriteria.open();
    }


    QtObject {
        id: internal

        readonly property int outlinePointSize: 28

        property double positionerMovedValue: -1
        property double shuffleCountdown: 0.5
        property bool kbPositioning: false
        property int kbLastFocused: 0
        property string searchAmpacheURL: ""
        property int fontSize: 12
        property bool titleCurlySpecial: true

        function calculateTitlePointSize()
        {
            title.font.pointSize = 99;
            while ((title.font.pointSize >= 8) && ((title.width > track.width - art.width) || (title.height > art.height / 12 * 5))) {
                title.font.pointSize--;
            }
        }

        function calculatePerformerPointSize()
        {
            performer.font.pointSize = 99;
            while ((performer.font.pointSize >= 8) && ((performer.width > track.width - art.width) || (performer.contentHeight > performer.height - 20))) {
                performer.font.pointSize--;
            }
        }

        function rememberKBLastFocused()
        {
            kbLastFocused = 0;
            if (explorer.isFocused) {
                kbLastFocused = 1;
            }
            else if (playlist.isFocused) {
                kbLastFocused = 2;
            }
        }

        function restoreKBLastFocused()
        {
            if (explorer.isFocused || playlist.isFocused) {
                return false;
            }

            if ((kbLastFocused < 1) || (kbLastFocused > 2)) {
                kbLastFocused = 1;
            }

            if (kbLastFocused == 1) {
                explorer.isFocused = true;
                playlist.isFocused = false;
            }
            else {
                explorer.isFocused = false;
                playlist.isFocused = true;
            }

            focusTimer.restart();
            return true;
        }
    }

    Timer {
        id: statusTempTimer
        interval: 3333

        onTriggered: {
            statusTemp.visible = false;

            tags.visible = true;
            status.visible = true;
            artistSummaryContainer.visible = true
        }
    }
    Timer {
        id: titleSizeRecalcOnResizeTimer
        interval: 100

        onTriggered: {
            internal.calculateTitlePointSize();
            internal.calculatePerformerPointSize();
        }
    }
    Timer {
        id: focusTimer
        interval: 15000

        onTriggered: {
            internal.rememberKBLastFocused();
            explorer.isFocused = false;
            playlist.isFocused = false;
        }
    }
    Timer {
        id: artistSummaryPauser
        interval: 7500

        onTriggered: {
            if (!artistSummaryMouseArea.containsMouse) {
                artistSummaryAnimation.pause();
            }
        }
    }


    header: ToolBar {
        RowLayout {
            anchors.fill: parent
            Keys.forwardTo: rightSide

            ToolButton {
                icon.name: 'media-skip-backward'
                icon.source: "qrc:///icons/skip_previous.ico"
                onClicked: {
                    if (historyModel.count > 0) {
                        historyMenu.popup();
                    }
                }
                Keys.forwardTo: rightSide
            }
            ToolButton {
                icon.name: 'media-playback-start'
                icon.source: "qrc:///icons/play.ico"
                onClicked: playButton()
                Keys.forwardTo: rightSide
            }
            ToolButton {
                icon.name: 'media-playback-pause'
                icon.source: "qrc:///icons/pause.ico"
                onClicked: pauseButton()
                Keys.forwardTo: rightSide
            }
            ToolButton {
                icon.name: 'media-playback-stop'
                icon.source: "qrc:///icons/stop.ico"
                onClicked: stopButton()
                Keys.forwardTo: rightSide
            }
            ToolButton {
                icon.name: 'media-skip-forward'
                icon.source: "qrc:///icons/skip_next.ico"
                onClicked: nextButton()
                Keys.forwardTo: rightSide
            }
            ToolSeparator {
                Keys.forwardTo: rightSide
            }
            ToolButton {
                id: favorite
                checkable: true
                icon.name: 'starred'
                icon.source: "qrc:///icons/star.ico"
                onClicked: favoriteButton(checked)
                Keys.forwardTo: rightSide
            }
            ToolButton {
                icon.name: 'search'
                icon.source: "qrc:///icons/search.ico"
                enabled: title.text.length && performer.text.length
                onClicked: searchMenu.popup();
                Keys.forwardTo: rightSide
            }
            Label {
                Layout.fillWidth: true
            }
            ToolButton {
                icon.name: 'open-menu'
                icon.source: "qrc:///icons/menu.ico"
                onClicked: menu.popup();
                Keys.forwardTo: rightSide
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
                leftPadding: 10
                rightPadding: 17
                font.pointSize: internal.fontSize
            }
            Item {
                anchors.left: tags.right
                anchors.right: status.left
                anchors.verticalCenter: parent.verticalCenter
                height: parent.height
                id: artistSummaryContainer
                clip: true

                MouseArea {
                    id: artistSummaryMouseArea
                    hoverEnabled: true
                    anchors.fill: parent
                }

                TextMetrics {
                    id: artistSummaryMetrics
                    font.pointSize: internal.fontSize
                    text: ""
                }

                Label {
                    id: artistSummary
                    text: ""
                    anchors.verticalCenter: parent.verticalCenter
                    font.pointSize: internal.fontSize
                    color: statusTemp.palette.buttonText

                    NumberAnimation on x {
                        id: artistSummaryAnimation
                        running: false
                        paused: !artistSummaryMouseArea.containsMouse
                        from: artistSummaryContainer.width
                        to: artistSummaryMetrics.boundingRect.width * -1
                        duration: artistSummary.text.length * 75
                        loops: Animation.Infinite
                    }
                }
            }
            Label {
                anchors.verticalCenter: parent.verticalCenter
                anchors.right: parent.right
                id: status
                text: qsTr("Idle")
                font.family: "Monospace"
                font.pointSize: internal.fontSize
                horizontalAlignment: Text.AlignRight
                rightPadding: 10
                leftPadding: 17
                width: parent.width / 10
            }
            Label {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                id: statusTemp
                color: statusTemp.palette.buttonText
                font.family: "Monospace"
                font.pointSize: internal.fontSize
                elide: Text.ElideRight
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
            font.pointSize: internal.fontSize
            onTriggered: {
                servers.serversModel = explorer.getServersForDialog();
                servers.open();
            }
        }
        MenuItem {
            text: qsTr("Options")
            font.pointSize: internal.fontSize
            onTriggered: requestOptions()
        }
        MenuSeparator { }
        MenuItem {
            text: qsTr("Quick Start Guide")
            font.pointSize: internal.fontSize
            onTriggered: quickStartGuide.open()
        }
        MenuItem {
            text: qsTr("About")
            font.pointSize: internal.fontSize
            onTriggered: aboutWaver.open()
        }
        MenuSeparator { }
        MenuItem {
            text: qsTr("Quit")
            font.pointSize: internal.fontSize
            onTriggered: close();
        }
    }

    ListModel {
        id: historyModel
    }

    Menu {
        id: historyMenu
        leftMargin: 10
        width: parent.width * .8

        Instantiator {
            model: historyModel

            delegate: MenuItem {
                text: title
                font.pointSize: internal.fontSize
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
            font.pointSize: internal.fontSize
            onTriggered: Qt.openUrlExternally("https://google.com/search?q=" + performer.text + " " + title.text + " lyrics");
        }
        MenuItem {
            text: qsTr("Artist")
            font.pointSize: internal.fontSize
            onTriggered: Qt.openUrlExternally("https://google.com/search?q=\"" + performer.text + "\" band");
        }
        MenuSeparator { }
        MenuItem {
            text: qsTr("Ampache")
            font.pointSize: internal.fontSize
            onTriggered: Qt.openUrlExternally(internal.searchAmpacheURL);
            enabled: internal.searchAmpacheURL.length > 0
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
        onReqEQ: {
            requestEQ(eq_chooser)
        }
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
            font.pointSize: internal.fontSize
            text: qsTr("Delete server?")
        }
    }

    Dialog {
        id: searchCriteria

        anchors.centerIn: parent
        focus: true
        height: parent.height * 0.75
        width: parent.width * 0.75

        modal: true
        standardButtons: Dialog.Ok | Dialog.Cancel

        title: qsTr("Search Criteria")

        function clear()
        {
            searchText.text = "";
        }

        onAccepted: {
            searchCriteriaEntered(searchText.text);
        }

        TextField {
            id: searchText
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            focus: true
            onAccepted: searchCriteria.accept()
            font.pointSize: internal.fontSize
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

        Keys.enabled: true
        Keys.onPressed: {
            if (event.key === Qt.Key_Space) {
                ppButton();
            }
            else if (event.key === Qt.Key_Left) {
                internal.kbPositioning = true;
                if (positioner.value > .025) {
                    positioner.value -= .025
                }
            }
            else if (event.key === Qt.Key_Right) {
                internal.kbPositioning = true;
                if (positioner.value < .975) {
                    positioner.value += .025;
                }
            }
            else if (event.key === Qt.Key_PageDown) {
                previousButton(0);
            }
            else if (event.key === Qt.Key_PageUp) {
                nextButton();
            }
            else if (event.key === Qt.Key_Tab) {
                if (internal.restoreKBLastFocused()) {
                    return;
                }

                if (explorer.isFocused) {
                    explorer.isFocused = false;
                    playlist.isFocused = true;
                }
                else {
                    explorer.isFocused = true;
                    playlist.isFocused = false;
                }

                internal.rememberKBLastFocused();
            }
            else if (event.key === Qt.Key_Down) {
                internal.restoreKBLastFocused();
                if (explorer.isFocused) {
                    explorer.moveSelectionDown();
                }
                else if (playlist.isFocused) {
                    playlist.moveSelectionDown();
                }
            }
            else if (event.key === Qt.Key_Up) {
                internal.restoreKBLastFocused();
                if (explorer.isFocused) {
                    explorer.moveSelectionUp();
                }
                else if (playlist.isFocused) {
                    playlist.moveSelectionUp();
                }
            }
            else if ((event.key === Qt.Key_Enter) || (event.key === Qt.Key_Return)) {
                internal.restoreKBLastFocused();
                if (explorer.isFocused) {
                    explorer.simulateRightClick();
                }
                else if (playlist.isFocused) {
                    playlist.simulateRightClick();
                }
            }

            event.accepted = true;
        }
        Keys.onReleased: {
            if (((event.key === Qt.Key_Left) || (event.key === Qt.Key_Right)) && !event.isAutoRepeat) {
                internal.kbPositioning = false;
                positioned(positioner.value);
            }
            event.accepted = true;
        }

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
                style: { font.pointSize >= internal.outlinePointSize ? Text.Outline : Text.Normal }
                styleColor: title.palette.windowText
                wrapMode: Text.Wrap

                onTextChanged: {
                    internal.calculateTitlePointSize();
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
                style: { font.pointSize >= internal.outlinePointSize ? Text.Outline : Text.Normal }
                styleColor: performer.palette.windowText
                verticalAlignment: Text.AlignVCenter
                wrapMode: Text.Wrap

                onTextChanged: {
                    internal.calculatePerformerPointSize();
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
                font.pointSize: internal.fontSize * 1.25
                text: "trackNumber"
            }

            Label {
                id: album

                anchors.left: trackNumber.right
                anchors.right: year.left
                anchors.verticalCenter: albumBackground.verticalCenter

                color: title.palette.highlightedText
                font.pointSize: internal.fontSize * (Math.min(title.font.pointSize, performer.font.pointSize) >= internal.outlinePointSize ? 1.25 : 1)
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
                font.pointSize: internal.fontSize * 1.25
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

                font.pointSize: internal.fontSize * .75 > 8 ? internal.fontSize * .75 : 8
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

                font.pointSize: internal.fontSize * .75 > 8 ? internal.fontSize * .75 : 8
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
            onExplorerItemDragDroped: {
                var extra = explorer.getExtra(id);
                playlistExplorerItemDragDropped(id, JSON.stringify(extra), destinationIndex);
            }
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
