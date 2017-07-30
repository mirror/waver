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


import QtQuick 2.7
import QtQuick.Controls 2.0
import QtGraphicalEffects 1.0
import QtQuick.Dialogs 1.2


ApplicationWindow {
    id: app
    visible: true
    width: 400
    height: 525
    title: "Waver"


    readonly property int  duration_visible_before_fadeout: 1500
    readonly property int  duration_fadeout: 250
    readonly property int  duration_art_transition: 750
    readonly property int  duration_slideout: 150
    readonly property real brightness_full: 0
    readonly property real brightness_dark: -.8
    readonly property real opacity_opaque: 1.0
    readonly property real opacity_transparent: 0.0
    readonly property int  menu_x_open: 6
    readonly property int  menu_x_closed: app.width * -1
    readonly property int  plugins_id_start: 1000
    readonly property int  plugins_id_end: 1999


    // these signals are processed by C++ (WaverApplication class)
    signal menuPause()
    signal menuResume()
    signal menuNext()
    signal menuCollection(variant collectionLabel)
    signal menuPlugin(variant foreignId)
    signal menuAbout()
    signal menuQuit()
    signal collectionsDialogResults(variant collectionsArray);
    signal pluginUIResults(variant foreignId, variant results)
    signal getOpenTracks(variant pluginId, variant parentId)
    signal startSearch(variant criteria)
    signal resolveOpenTracks(variant selected)
    signal trackAction(variant index, variant action)


    /*****
     handlers for signals received from C++
    *****/

    // error messages and warnings
    function displayUserMessage(messageText)
    {
        userMessageFadeoutAnimation.stop()
        userMessage.text    = messageText;
        userMessage.opacity = 1;
        userMessageResetTimer.restart();
    }


    // pause / resume

    function paused()
    {
        artPausedAnimation.start();
        nextButton.enabled = false;
        pauseButton.visible = false;
        resumeButton.visible = true;
    }

    function resumed()
    {
        artResumedAnimation.start();
        nextButton.enabled = true;
        resumeButton.visible = false;
        pauseButton.visible = true;
    }


    // collections list
    function fillCollectionsList(collectionsList, currentIndex)
    {
        collection.model        = collectionsList;
        collection.currentIndex = currentIndex;
    }


    // now playing page

    function updateTrackInfo(title, performer, album, year, track)
    {
        trackInfoOut.start();
        trackInfoChange.newTitle = title;
        trackInfoChange.newPerformer = performer;
        trackInfoChange.newAlbum = album;
        trackInfoChange.newYear = year;
        trackInfoChange.newTrack = track;
        trackInfoChange.start();

        nowPlayingActions.visible = false
        nowPlayingActionsBackground.visible = false
    }

    function updateArt(url)
    {
        if (artEffect1.opacity > opacity_transparent) {
            art2.source = url;
            art1Out.start();
            art2In.start();
            return;
        }
        art1.source = url;
        art2Out.start();
        art1In.start();
    }

    function updateTrackActions(text) {
        nowPlayingActions.text = text;
    }

    function updatePosition(elapsed, remaining)
    {
        position_elapsed.text = elapsed;
        position_remaining.text = remaining;
    }


    // playlist page

    function clearPlaylist()
    {
        playlistItems.clear();
        playlistAddButton.enabled = true;
    }

    function addToPlaylist(pictureUrl, title, performer, actions, showActions)
    {
        if (title.length < 1) {
            return;
        }

        playlistItems.append({
            labelTitle: title,
            labelPerformer: performer,
            labelActions: actions,
            imageSource: pictureUrl,
            initialShowActions: showActions
        });

        playlistAddButton.enabled = (playlistItems.count < 25);
        searchButton.enabled = ((searchText.text.length >= 3) && (playlistItems.count < 25));
    }


    // plugin menu items

    function clearPluginsList()
    {
        var i = 0;
        while (i < menuItems.count) {
            if ((menuItems.get(i).clickId >= plugins_id_start) && (menuItems.get(i).clickId <= plugins_id_end)) {
                menuItems.remove(i);
            }
            else {
                i++;
            }
        }
    }

    function addToPluginsList(id, label)
    {
        var lastId = 0;
        for(var i = 0; i < menuItems.count; i++) {
            if ((menuItems.get(i).clickId >= plugins_id_start) && (menuItems.get(i).clickId <= plugins_id_end)) {
                if (menuItems.get(i).clickId > lastId) {
                    lastId = menuItems.get(i).clickId;
                }
            }
        }
        if (lastId == 0) {
            lastId = plugins_id_start - 1;
        }

        menuItems.append({
            labelText: label,
            imageSource: "images/plugin_settings.png",
            clickId: lastId + 1,
            foreignId: id
        });
    }


    // plugin user interface

    function displayPluginUI(id, qml)
    {
        var pluginUIContent = Qt.createQmlObject(qml, pluginUI, "");
        pluginUIContent.done.connect(finishPluginUI);
        pluginUIContent.apply.connect(sendPluginUI);

        pluginUI.foreignId = id;
        pluginUI.uiObject  = pluginUIContent;

        pluginUI.visible = true;
        pluginUIin.start();
    }

    function finishPluginUI(results)
    {
        if (results) {
            pluginUIResults(pluginUI.foreignId, results);
        }

        pluginUIout.start();
        pluginUIoutVisibility.start();

        pluginUI.uiObject.destroy(duration_fadeout + 50);
    }

    function sendPluginUI(results)
    {
        if (results) {
            pluginUIResults(pluginUI.foreignId, results);
        }
    }


    // open tracks

    function addToOpenTracksList(pluginId, hasChildren, selectable, label, id)
    {
        if (!playlistAdd.visible) {
            return;
        }

        if ((playlistAddSelectableItems.count < 1) && (playlistAdd.directoryHistory.length > 1)) {
            var history = playlistAdd.directoryHistory[playlistAdd.directoryHistory.length - 2]
            if (history) {
                playlistAddSelectableItems.append({
                    pluginId: history.pluginId,
                    actions: "<a href=\"open\">Open</a>",
                    label: "..",
                    id: history.id
                });
            }
        }

        var actions = [];
        if (hasChildren) {
            actions.push("<a href=\"open\">Open</a>");
        }
        if (selectable) {
            actions.push("<a href=\"add\">Add</a>");
        }

        playlistAddSelectableItems.append({
            pluginId: pluginId,
            actions: actions.join(" "),
            label: label,
            id: id
        });

        playlistAddSelectables.contentY = 0;
    }


    // search

    function addToSearchList(pluginId, label, id)
    {
        if (!search.visible) {
            return;
        }

        searchSelectableItems.append({
            pluginId: pluginId,
            actions: "<a href=\"add\">Add</a>",
            label: label,
            id: id
        });
    }


    // about dialog
    function aboutDialog(appName, AppVersion, appDescription)
    {

        aboutName.text        = appName
        aboutVersion.text     = AppVersion
        aboutDescription.text = appDescription

        about.visible = true;
        aboutIn.start();
    }


    /*****
     handlers for signals within the UI
    *****/

    function menuClick(id)
    {
        menuContainerClose.start();

        if ((id >= plugins_id_start) && (id <= plugins_id_end)) {
            var foreignId;
            for(var i = 0; i < menuItems.count; i++) {
                if (menuItems.get(i).clickId == id) {
                    foreignId = menuItems.get(i).foreignId;
                }
            }
            if (foreignId) {
                menuPlugin(foreignId);
            }
            return;
        }

        switch (id) {
        case 1:
            collectionsItems.clear();
            for(var i = 0; i < collection.count; i++) {
                collectionsItems.append({ label: collection.textAt(i) });
            }

            collections.visible = true;
            collectionsIn.start();
            break;

        case 2:
            menuAbout();
            break;
        }
    }


    /*****
     animation definitions
    ****/

    // error messages and warnings

    NumberAnimation {
        id: userMessageFadeoutAnimation
        target: userMessage
        property: "opacity"
        to: 0
        duration: duration_fadeout
    }

    Timer {
        id: userMessageResetTimer
        interval: duration_visible_before_fadeout
        onTriggered: userMessageFadeoutAnimation.start()
    }


    // menu slide in and out

    NumberAnimation {
        id: menuContainerClose
        target: menuContainer
        property: "x"
        to: menu_x_closed
        duration: duration_slideout
    }

    NumberAnimation {
        id: menuContainerOpen
        target: menuContainer
        property: "x"
        to: menu_x_open
        duration: duration_slideout
    }


    // pause and resume

    ParallelAnimation {
        id: artPausedAnimation

        NumberAnimation {
            target: artEffect1
            property: "brightness"
            to: brightness_dark
            duration: duration_fadeout
        }
        NumberAnimation {
            target: artEffect2
            property: "brightness"
            to: brightness_dark
            duration: duration_fadeout
        }
    }

    ParallelAnimation {
        id: artResumedAnimation

        NumberAnimation {
            target: artEffect1
            property: "brightness"
            to: brightness_full
            duration: duration_fadeout
        }
        NumberAnimation {
            target: artEffect2
            property: "brightness"
            to: brightness_full
            duration: duration_fadeout
        }
    }


    // now playing transitions - labels

    NumberAnimation {
        id: trackInfoOut
        targets: [title, performer, album, year, track]
        property: "opacity"
        to: opacity_transparent
        duration: duration_fadeout
    }

    NumberAnimation {
        id: trackInfoIn
        targets: [title, performer, album, year, track]
        property: "opacity"
        to: opacity_opaque
        duration: duration_fadeout
    }

    Timer {
        id: trackInfoChange
        interval: duration_fadeout + 25
        onTriggered: {
            title.text = newTitle;
            performer.text = newPerformer;
            album.text = newAlbum;

            if (newYear == 0) {
                year.text = "";
            }
            else {
                year.text = newYear;
            }

            if (newTrack == 0) {
                track.text = "";
            }
            else {
                track.text = newTrack;
            }

            trackInfoIn.start();
        }

        property string newTitle
        property string newPerformer
        property string newAlbum
        property string newYear
        property string newTrack
    }


    // now playing transitions - image

    NumberAnimation {
        id: art1Out
        target: artEffect1
        property: "opacity"
        to: opacity_transparent
        duration: duration_art_transition
    }

    NumberAnimation {
        id: art1In
        target: artEffect1
        property: "opacity"
        to: opacity_opaque
        duration: duration_art_transition
    }

    NumberAnimation {
        id: art2Out
        target: artEffect2
        property: "opacity"
        to: opacity_transparent
        duration: duration_art_transition
    }

    NumberAnimation {
        id: art2In
        target: artEffect2
        property: "opacity"
        to: opacity_opaque
        duration: duration_art_transition
    }


    // playlist transitions

    Transition {
        id: playlistIn

        NumberAnimation {
            property: "opacity"
            from: opacity_transparent
            duration: duration_fadeout
        }
    }

    Transition {
        id: playlistOut

        NumberAnimation {
            property: "opacity"
            to: opacity_transparent
            duration: duration_fadeout
        }
    }


    // playlist add dialog transitions

    NumberAnimation {
        id: playlistAddIn
        target: playlistAdd
        property: "opacity"
        from: opacity_transparent
        to: opacity_opaque
        duration: duration_fadeout
    }

    NumberAnimation {
        id: playlistAddOut
        target: playlistAdd
        property: "opacity"
        from: opacity_opaque
        to: opacity_transparent
        duration: duration_fadeout
    }

    Timer {
        id: playlistAddOutVisibility
        interval: duration_fadeout + 25
        onTriggered: {
            playlistAdd.visible = false;
        }
    }


    // search dialog transitions

    NumberAnimation {
        id: searchIn
        target: search
        property: "opacity"
        from: opacity_transparent
        to: opacity_opaque
        duration: duration_fadeout
    }

    NumberAnimation {
        id: searchOut
        target: search
        property: "opacity"
        from: opacity_opaque
        to: opacity_transparent
        duration: duration_fadeout
    }

    Timer {
        id: searchOutVisibility
        interval: duration_fadeout + 25
        onTriggered: {
            search.visible = false;
        }
    }


    // plugin UI dialog transitions

    NumberAnimation {
        id: pluginUIin
        target: pluginUI
        property: "opacity"
        from: opacity_transparent
        to: opacity_opaque
        duration: duration_fadeout
    }

    NumberAnimation {
        id: pluginUIout
        target: pluginUI
        property: "opacity"
        from: opacity_opaque
        to: opacity_transparent
        duration: duration_fadeout
    }

    Timer {
        id: pluginUIoutVisibility
        interval: duration_fadeout + 25
        onTriggered: {
            pluginUI.visible = false;
        }
    }


    // collections dialog transitions

    NumberAnimation {
        id: collectionsIn
        target: collections
        property: "opacity"
        from: opacity_transparent
        to: opacity_opaque
        duration: duration_fadeout
    }

    NumberAnimation {
        id: collectionsOut
        target: collections
        property: "opacity"
        from: opacity_opaque
        to: opacity_transparent
        duration: duration_fadeout
    }

    Timer {
        id: collectionsOutVisibility
        interval: duration_fadeout + 25
        onTriggered: {
            collections.visible = false;
        }
    }


    // about dialog transitions
    // playlist add dialog transitions

    NumberAnimation {
        id: aboutIn
        target: about
        property: "opacity"
        from: opacity_transparent
        to: opacity_opaque
        duration: duration_fadeout
    }

    NumberAnimation {
        id: aboutOut
        target: about
        property: "opacity"
        from: opacity_opaque
        to: opacity_transparent
        duration: duration_fadeout
    }

    Timer {
        id: aboutOutVisibility
        interval: duration_fadeout + 25
        onTriggered: {
            about.visible = false;
        }
    }

    /*****
     "non-UI" objects
    *****/

    TextMetrics {
        id: textMetrics
        text: "Át a gyárakon"
    }


    // menu

    ListModel {
        id: menuItems

        ListElement {
            labelText: "Collections"
            imageSource: "images/collections.png"
            clickId: 1
            foreignId: "N/A"
        }

        ListElement {
            labelText: "About"
            imageSource: "images/about.png"
            clickId: 2
            foreignId: "N/A"
        }
    }

    Component {
        id: menuElement

        Item {
            height: menuElementImage.height
            width: menuContainer.width

            MouseArea {
                hoverEnabled: true
                onClicked: menuClick(clickId)
                onEntered: {
                    if (Qt.platform.os !== "android") {
                        menuElementLabel.font.bold = true
                    }
                }
                onExited: {
                    if (Qt.platform.os !== "android") {
                        menuElementLabel.font.bold = false
                    }
                }
                anchors.fill: parent
            }

            Row {
                spacing: 6

                Image {
                    id: menuElementImage
                    source: imageSource
                }

                PlatformLabel {
                    id: menuElementLabel
                    text: labelText
                    anchors.verticalCenter: menuElementImage.verticalCenter
                }

                PlatformLabel {
                    text: " "
                    anchors.verticalCenter: menuElementImage.verticalCenter
                }
            }
        }
    }


    // playlist

    ListModel {
        id: playlistItems

        ListElement {
            labelTitle: "Title"
            labelPerformer: "Performer"
            labelActions: "<a href=\"action\">Action</a>"
            imageSource: "images/waver.png"
            initialShowActions: true
        }
    }

    Component {
        id: playlistElement

        Item {
            id: playlistElementOuter
            height: playlistElementImage.height + (playlistElementInner.anchors.margins * 2) + playlistActions.height + playlistActions.anchors.bottomMargin + playlistElementBorder.anchors.bottomMargin
            width: playlist.width

            Rectangle {
                id: playlistElementBorder
                anchors.fill: parent
                anchors.bottomMargin: 6
                border.color: "#AAAAAA"
                radius: 3
                gradient: Gradient {
                    GradientStop { position: 0.0; color: "#AAAAAA" }
                    GradientStop { position: 0.5; color: "transparent" }
                    GradientStop { position: 1; color: "#AAAAAA" }
                }
            }

            Item {
                id: playlistElementInner
                anchors.fill: playlistElementOuter
                anchors.margins: 3

                Image {
                    id: playlistElementImage
                    anchors.left: playlistElementInner.left
                    anchors.top: playlistElementInner.top
                    source: imageSource
                    width: 48
                    height: 48
                }

                MouseArea {
                    anchors.fill: playlistElementImage
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        if (playlistActions.visible) {
                            playlistActions.height = 0;
                            playlistActions.anchors.bottomMargin = 0;
                            playlistActions.visible = false;
                        }
                        else {
                            playlistActions.height = textMetrics.height + 6;
                            playlistActions.anchors.bottomMargin = 3;
                            playlistActions.visible = true;
                        }
                    }
                }

                Rectangle {
                    anchors.fill: playlistElementImage
                    border.color: "#666666"
                    color: "transparent"
                }

                Column {
                    id: playlistElementLabels
                    spacing: 6
                    anchors.left: playlistElementImage.right
                    anchors.right: playlistElementInner.right
                    anchors.verticalCenter: playlistElementImage.verticalCenter

                    PlatformLabel {
                        id: playlistElementTitle
                        anchors.left: playlistElementLabels.left
                        anchors.right: playlistElementLabels.right
                        anchors.leftMargin: 6
                        anchors.rightMargin: 3
                        text: labelTitle
                        font.bold: true
                    }

                    PlatformLabel {
                        id: playlistElementPerformer
                        anchors.left: playlistElementLabels.left
                        anchors.right: playlistElementLabels.right
                        anchors.leftMargin: 6
                        anchors.rightMargin: 3
                        text: labelPerformer
                    }
                }
            }

            MouseArea {
                anchors.fill: playlistActions
                cursorShape: Qt.PointingHandCursor
            }

            Rectangle {
                anchors.fill: playlistActions
                radius: 3
                color: "#FFFFFF"
            }

            PlatformLabel {
                id: playlistActions
                anchors.left: parent.left
                anchors.bottom: playlistElementBorder.bottom
                anchors.leftMargin: 3
                anchors.bottomMargin: (initialShowActions ? 3 : 0)
                leftPadding: 3
                rightPadding: 3
                height: (initialShowActions ? textMetrics.height + 6 : 0)
                visible: (initialShowActions ? true : false)
                text: labelActions
                verticalAlignment: Text.AlignVCenter
                onLinkActivated: {
                    trackAction(index, link);
                }
            }
        }
    }


    // add to playlist

    ListModel {
        id: playlistAddSelectableItems
    }

    Component {
        id: playlistAddSelectableElement

        Item {
            height: addElementLabel.height + elementActions.height
            width: playlistAddSelectables.width

            MouseArea {
                anchors.fill: elementActions
                cursorShape: Qt.PointingHandCursor
            }

            PlatformLabel {
                id: addElementLabel
                text: label
                elide: Text.ElideLeft
                height: textMetrics.height + (6 + 3)
                width: parent.width
                leftPadding: 6
                rightPadding: 6
                topPadding: 6
                bottomPadding: 3
            }

            PlatformLabel {
                id: elementActions
                text: actions
                anchors.top: addElementLabel.bottom
                height: textMetrics.height + 6
                leftPadding: 6
                rightPadding: 6
                topPadding: 0
                bottomPadding: 6
                onLinkActivated: {
                    if (link == "open") {
                        if (labelProperty == "..") {
                            playlistAdd.directoryHistory.pop();
                        }
                        else {
                            playlistAdd.directoryHistory.push({
                                pluginId: pluginIdProperty,
                                id: idProperty
                            });
                        }

                        playlistAddSelectableItems.clear();
                        getOpenTracks(pluginIdProperty, idProperty);
                    }

                    if (link == "add") {
                        playlistAddSelectedItems.append({
                            pluginId: pluginIdProperty,
                            id: idProperty
                        });
                        playlistAddSelected.positionViewAtEnd();
                    }
                }

                property string labelProperty   : label
                property string pluginIdProperty: pluginId
                property string idProperty      : id
            }

            Rectangle {
                anchors.fill: parent
                border.color: "#666666"
                color: "transparent"
                radius: 3
            }
        }
    }

    ListModel {
        id: playlistAddSelectedItems
    }

    Component {
        id: playlistAddSelectedElement

        Text {
            text: id
            elide: Text.ElideLeft
            height: textMetrics.height + (3 * 2)
            width: playlistAddSelected.width;
            padding: 3
        }
    }


    // search

    ListModel {
        id: searchSelectableItems
    }

    Component {
        id: searchSelectableElement

        Item {
            height: searchElementLabel.height + elementActions.height
            width: searchSelectables.width

            MouseArea {
                anchors.fill: elementActions
                cursorShape: Qt.PointingHandCursor
            }

            PlatformLabel {
                id: searchElementLabel
                text: label
                elide: Text.ElideLeft
                height: textMetrics.height + (6 + 3)
                width: parent.width
                leftPadding: 6
                rightPadding: 6
                topPadding: 6
                bottomPadding: 3
            }

            PlatformLabel {
                id: elementActions
                text: actions
                anchors.top: searchElementLabel.bottom
                height: textMetrics.height + 6
                leftPadding: 6
                rightPadding: 6
                topPadding: 0
                bottomPadding: 6
                onLinkActivated: {
                    if (link == "add") {
                        searchSelectedItems.append({
                            pluginId: pluginIdProperty,
                            id: idProperty
                        });
                        searchSelected.positionViewAtEnd();
                    }
                }

                property string pluginIdProperty: pluginId
                property string idProperty      : id
            }

            Rectangle {
                anchors.fill: parent
                border.color: "#666666"
                color: "transparent"
                radius: 3
            }
        }
    }

    ListModel {
        id: searchSelectedItems
    }

    Component {
        id: searchSelectedElement

        Text {
            text: id
            elide: Text.ElideLeft
            height: textMetrics.height + (3 * 2)
            width: searchSelected.width;
            padding: 3

            property string pluginIdProperty: pluginId
            property string idProperty      : id
        }
    }


    // collections

    ListModel {
        id: collectionsItems
    }

    Component {
        id: collectionsElement

        Item {
            height: collectionsLabel.height + elementActions.height
            width: collectionsList.width

            MouseArea {
                anchors.fill: elementActions
                cursorShape: Qt.PointingHandCursor
            }

            PlatformLabel {
                id: collectionsLabel
                text: label
                elide: Text.ElideLeft
                height: textMetrics.height + (6 + 3)
                width: parent.width
                leftPadding: 6
                rightPadding: 6
                topPadding: 6
                bottomPadding: 3
            }

            PlatformLabel {
                id: elementActions
                text: "<a href=\"delete\">Delete</a>"
                anchors.top: collectionsLabel.bottom
                height: textMetrics.height + 6
                leftPadding: 6
                rightPadding: 6
                topPadding: 0
                bottomPadding: 6
                onLinkActivated: {
                    if (link == "delete") {
                        collectionsItems.remove(index, 1);
                    }
                }
            }

            Rectangle {
                anchors.fill: parent
                border.color: "#666666"
                color: "transparent"
                radius: 3
            }
        }
    }


    /*****
     the actual UI, finally
    *****/

    // menu

    ToolButton {
        id: menuButton
        background: Image {
            source: "images/menu.png"
            fillMode: Image.PreserveAspectFit
            anchors.verticalCenter: menuButton.verticalCenter
        }
        onClicked: {
            if (menuContainer.x == menu_x_open) {
                menuContainerClose.start()
            }
            if (menuContainer.x == menu_x_closed) {
                menuContainerOpen.start()
            }
        }
        anchors.left: parent.left
        anchors.leftMargin: 6
        anchors.top: collection.top
        anchors.bottom: collection.bottom
    }

    Rectangle {
        id: pauseFrame
        width: 24
        anchors.left: menuButton.right
        anchors.leftMargin: 6
        anchors.top: collection.top
        anchors.topMargin: 3
        anchors.bottom: collection.bottom
        anchors.bottomMargin: 3
        border.color: "#666666"
        color: "transparent"
        radius: 2
    }

    Rectangle {
        id: nextFrame
        width: 24
        anchors.left: pauseFrame.right
        anchors.leftMargin: 3
        anchors.top: collection.top
        anchors.topMargin: 3
        anchors.bottom: collection.bottom
        anchors.bottomMargin: 3
        border.color: "#666666"
        color: "transparent"
        radius: 2
    }

    ToolButton {
        id: pauseButton
        background: Image {
            source: "images/pause.png"
            fillMode: Image.PreserveAspectFit
            anchors.verticalCenter: pauseButton.verticalCenter
            anchors.horizontalCenter: pauseButton.horizontalCenter
        }
        onClicked: {
            menuPause();
        }
        anchors.fill: pauseFrame
    }

    ToolButton {
        id: resumeButton
        background: Image {
            source: "images/resume.png"
            fillMode: Image.PreserveAspectFit
            anchors.verticalCenter: resumeButton.verticalCenter
            anchors.horizontalCenter: resumeButton.horizontalCenter
        }
        onClicked: {
            menuResume();
        }
        anchors.fill: pauseFrame
        visible: false
    }

    ToolButton {
        id: nextButton
        background: Image {
            source: "images/next.png"
            fillMode: Image.PreserveAspectFit
            anchors.verticalCenter: nextButton.verticalCenter
            anchors.horizontalCenter: nextButton.horizontalCenter
        }
        onClicked: {
            menuNext();
        }
        anchors.fill: nextFrame
    }

    ToolButton {
        id: quitButton
        background: Image {
            source: "images/quit.png"
            fillMode: Image.PreserveAspectFit
            anchors.verticalCenter: quitButton.verticalCenter
        }
        onClicked: {
            menuQuit();
        }
        anchors.right: parent.right
        anchors.rightMargin: 6
        anchors.top: collection.top
        anchors.bottom: collection.bottom
    }

    Item {
        id: menuContainer
        width: app.width * .66
        x: app.width * -1
        anchors.top: collection.bottom
        anchors.topMargin: 6
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 6
        z: 100

        Rectangle {
            id: menuBackground
            anchors.fill: menuContainer
        }

        DropShadow {
            source: menuBackground
            horizontalOffset: 3
            verticalOffset: 3
            radius: 6.0
            samples: 12
            color: "#80000000"
            anchors.fill: menuBackground
        }

        ListView {
            id: menu
            model: menuItems
            delegate: menuElement
            anchors.fill: menuContainer
            spacing: 6
            clip: true
        }
    }


    // collections

    ComboBox {
        id: collection
        anchors.left: nextFrame.right
        anchors.leftMargin: 6
        anchors.right: quitButton.left
        anchors.rightMargin: 4
        anchors.top: parent.top
        anchors.topMargin: 6
        onActivated: menuCollection(collection.textAt(index));
    }


    // pages

    SwipeView {
        id: swiper
        currentIndex: pager.currentIndex
        anchors.right: parent.right
        anchors.left: parent.left
        anchors.top: collection.bottom
        anchors.topMargin: 6
        anchors.bottom: pager.top

        // now playing page
        Item {
            id: now_playing

            // image
            Item {
                id: artArea
                anchors.right: parent.right
                anchors.rightMargin: 6
                anchors.left: parent.left
                anchors.leftMargin: 6
                anchors.top: parent.top
                anchors.bottom: title.top
                anchors.bottomMargin: 6

                // two images are used for the transition (one is opaque and the other is transparent except during transition)

                Image {
                    id: art1
                    asynchronous: true
                    source: "images/waver.png"
                    smooth: true
                    visible: false
                    anchors.right: artArea.right
                    anchors.left: artArea.left
                    anchors.top: artArea.top
                    anchors.bottom: artArea.bottom
                }

                BrightnessContrast {
                    id: artEffect1
                    source: art1
                    anchors.fill: art1
                    brightness: 0
                    contrast: 0
                    opacity: opacity_opaque
                }

                Image {
                    id: art2
                    asynchronous: true
                    source: "images/waver.png"
                    smooth: true
                    visible: false
                    anchors.right: artArea.right
                    anchors.left: artArea.left
                    anchors.top: artArea.top
                    anchors.bottom: artArea.bottom
                }

                BrightnessContrast {
                    id: artEffect2
                    source: art2
                    anchors.fill: art2
                    brightness: 0
                    contrast: 0
                    opacity: opacity_transparent
                }

                // just a border around the image
                Rectangle {
                    anchors.fill: art1
                    border.color: "#666666"
                    color: "transparent"
                }
            }

            // error messages and warnings - in the center of the image, normally invisible
            PlatformLabel {
                id: userMessage
                text: ""
                horizontalAlignment: Text.AlignHCenter
                font.bold: true
                style: Text.Outline
                color: "#800000"
                styleColor: "#F2F2F2"
                anchors.horizontalCenter: artArea.horizontalCenter
                anchors.verticalCenter: artArea.verticalCenter
            }

            // track actions - normally invisible

            MouseArea {
                id: nowPlayingActionsMouseArea
                anchors.fill: artArea
                cursorShape: Qt.PointingHandCursor
                onClicked: {
                    nowPlayingActions.visible = !nowPlayingActions.visible;
                    nowPlayingActionsBackground.visible = !nowPlayingActionsBackground.visible;
                }
            }

            Rectangle {
                id: nowPlayingActionsBackground
                anchors.fill: nowPlayingActions
                color: "#FFFFFF"
                radius: 3
                visible: false
            }

            PlatformLabel {
                id: nowPlayingActions
                text: "<a href=\"action\">Action</a>"
                horizontalAlignment: Text.AlignHCenter
                anchors.horizontalCenter: artArea.horizontalCenter
                anchors.bottom: artArea.bottom
                anchors.bottomMargin: 18
                padding: 3
                visible: false
                onLinkActivated: {
                    trackAction(-1, link);
                }
            }

            // labels and their background

            Rectangle {
                anchors.right: parent.right
                anchors.rightMargin: 6
                anchors.left: parent.left
                anchors.leftMargin: 6
                anchors.top: title.top
                anchors.bottom: parent.bottom
                border.color: "#AAAAAA"
                radius: 3
                gradient: Gradient {
                    GradientStop { position: 0.0; color: "#AAAAAA" }
                    GradientStop { position: 0.28; color: "transparent" }
                    GradientStop { position: 1.0; color: "#AAAAAA" }
                }
            }

            PlatformLabel {
                id: title
                text: "Title"
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                font.pointSize: userMessage.font.pointSize * largeMul
                font.bold: true
                style: Text.Outline
                styleColor: "#FFFFFF"
                anchors.right: parent.right
                anchors.rightMargin: 9
                anchors.left: parent.left
                anchors.leftMargin: 9
                anchors.bottom: performer.top
            }

            PlatformLabel {
                id: performer
                text: "Performer"
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                font.pointSize: userMessage.font.pointSize * largeMul
                anchors.right: parent.right
                anchors.rightMargin: 9
                anchors.left: parent.left
                anchors.leftMargin: 9
                anchors.bottom: album.top
                anchors.bottomMargin: 3
            }

            PlatformLabel {
                id: album
                text: "Album"
                font.pointSize: userMessage.font.pointSize * smallMul
                anchors.left: parent.left
                anchors.leftMargin: 9
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 3
            }
            PlatformLabel {
                id: track
                text: "Track"
                font.pointSize: userMessage.font.pointSize * smallMul
                font.italic: true
                anchors.left: album.right
                anchors.leftMargin: 6
                anchors.right: year.left
                anchors.rightMargin: 6
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 3
            }
            PlatformLabel {
                id: year
                text: "Year"
                font.pointSize: userMessage.font.pointSize * smallMul
                anchors.right: parent.right
                anchors.rightMargin: 9
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 3
            }
        }

        // playlist page
        Item {
            id: coming_up

            ListView {
                id: playlist
                anchors.right: parent.right
                anchors.rightMargin: 6
                anchors.left: parent.left
                anchors.leftMargin: 6
                anchors.top: parent.top
                anchors.bottom: playlistAddButton.top
                anchors.bottomMargin: 6
                model: playlistItems
                delegate: playlistElement
                add: playlistIn
                remove: playlistOut
                clip: true

                ScrollBar.vertical: ScrollBar { }
            }

            Button {
                id: playlistAddButton
                anchors.right: parent.right
                anchors.rightMargin: 6
                anchors.bottom: parent.bottom
                text: "Add"
                onClicked: {
                    playlistAddSelectableItems.clear();
                    playlistAddSelectedItems.clear();

                    playlistAdd.directoryHistory = [];
                    playlistAdd.directoryHistory.push({
                        pluginId: "",
                        id: ""
                    });


                    getOpenTracks("", "");

                    playlistAdd.visible = true;
                    playlistAddIn.start();
                }
            }

            TextField {
                id: searchText
                anchors.left: parent.left
                anchors.leftMargin: 6
                anchors.right: searchButton.left
                anchors.rightMargin: 6
                anchors.bottom: parent.bottom
                onTextChanged: {
                    searchButton.enabled = ((searchText.text.length >= 3) && (playlistItems.count < 25));
                }
                onAccepted: {
                    if (text.length >= 3) {
                        searchSelectableItems.clear();
                        searchSelectedItems.clear();

                        startSearch(text);

                        search.visible = true;
                        searchIn.start();
                    }
                }
            }

            Button {
                id: searchButton
                anchors.right: playlistAddButton.left
                anchors.rightMargin: 6
                anchors.bottom: parent.bottom
                text: "Search"
                enabled: false
                onClicked: {
                    searchSelectableItems.clear();
                    searchSelectedItems.clear();

                    startSearch(searchText.text);

                    search.visible = true;
                    searchIn.start();
                }
            }
        }
    }


    // bottom row, playing times and page selector

    PlatformLabel {
        id: position_elapsed
        text: "00:00"
        font.pointSize: userMessage.font.pointSize * smallMul
        anchors.left: parent.left
        anchors.leftMargin: 6
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 4
    }

    PlatformLabel {
        id: position_remaining
        text: "00:00"
        font.pointSize: userMessage.font.pointSize * smallMul
        anchors.right: parent.right
        anchors.rightMargin: 6
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 4
    }

    PageIndicator {
        id: pager
        count: swiper.count
        currentIndex: swiper.currentIndex
        interactive: true
        anchors.bottom: parent.bottom
        anchors.horizontalCenter: parent.horizontalCenter

    }


    /*****
     add to playlist dialog
    *****/

    Item {
        id: playlistAdd
        anchors.fill: parent
        opacity: opacity_transparent
        visible: false;

        property variant directoryHistory: [];

        MouseArea {
            anchors.fill: playlistAdd
        }

        Rectangle {
            id: playlistAddBackground
            anchors.fill: playlistAdd
        }

        ListView {
            id: playlistAddSelectables
            anchors.right: parent.right
            anchors.rightMargin: 6
            anchors.left: parent.left
            anchors.leftMargin: 6
            anchors.top: parent.top
            anchors.topMargin: 6
            anchors.bottom: playlistAdd.verticalCenter
            anchors.bottomMargin: 3
            model: playlistAddSelectableItems
            delegate: playlistAddSelectableElement
            spacing: 3
            clip: true

            ScrollBar.vertical: ScrollBar { }
        }

        ListView {
            id: playlistAddSelected
            anchors.right: parent.right
            anchors.rightMargin: 6
            anchors.left: parent.left
            anchors.leftMargin: 6
            anchors.top: playlistAdd.verticalCenter
            anchors.topMargin: 6
            anchors.bottom: playlistAddDone.top
            anchors.bottomMargin: 3
            model: playlistAddSelectedItems
            delegate: playlistAddSelectedElement
            spacing: 3
            clip: true
        }

        Rectangle {
            anchors.fill: playlistAddSelected
            border.color: "#666666"
            color: "transparent"
            radius: 3
        }

        Button {
            id: playlistAddDone
            anchors.right: parent.right
            anchors.rightMargin: 6
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 6
            text: "Done"
            onClicked: {
                var retval = [];
                for(var i = 0; i < playlistAddSelectedItems.count; i++) {
                    retval.push({
                        plugin_id: playlistAddSelectedItems.get(i).pluginId,
                        track_id: playlistAddSelectedItems.get(i).id
                    });
                }
                resolveOpenTracks(JSON.stringify(retval));

                playlistAddOut.start();
                playlistAddOutVisibility.start();
            }
        }

        Button {
            anchors.left: parent.left
            anchors.leftMargin: 6
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 6
            text: "Cancel"
            onClicked: {
                playlistAddOut.start();
                playlistAddOutVisibility.start();
            }
        }
    }


    /*****
     search dialog
    *****/

    Item {
        id: search
        anchors.fill: parent
        opacity: opacity_transparent
        visible: false;

        MouseArea {
            anchors.fill: search
        }

        Rectangle {
            id: searchBackground
            anchors.fill: search
        }

        ListView {
            id: searchSelectables
            anchors.right: parent.right
            anchors.rightMargin: 6
            anchors.left: parent.left
            anchors.leftMargin: 6
            anchors.top: parent.top
            anchors.topMargin: 6
            anchors.bottom: search.verticalCenter
            anchors.bottomMargin: 3
            model: searchSelectableItems
            delegate: searchSelectableElement
            spacing: 3
            clip: true

            ScrollBar.vertical: ScrollBar { }
        }

        ListView {
            id: searchSelected
            anchors.right: parent.right
            anchors.rightMargin: 6
            anchors.left: parent.left
            anchors.leftMargin: 6
            anchors.top: search.verticalCenter
            anchors.topMargin: 6
            anchors.bottom: searchDone.top
            anchors.bottomMargin: 3
            model: searchSelectedItems
            delegate: searchSelectedElement
            spacing: 3
            clip: true
        }

        Rectangle {
            anchors.fill: searchSelected
            border.color: "#666666"
            color: "transparent"
            radius: 3
        }

        Button {
            id: searchDone
            anchors.right: parent.right
            anchors.rightMargin: 6
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 6
            text: "Done"
            onClicked: {
                var retval = [];
                for(var i = 0; i < searchSelectedItems.count; i++) {
                    retval.push({
                        plugin_id: searchSelectedItems.get(i).pluginId,
                        track_id: searchSelectedItems.get(i).id
                    });
                }
                resolveOpenTracks(JSON.stringify(retval));

                searchOut.start();
                searchOutVisibility.start();
            }
        }

        Button {
            anchors.left: parent.left
            anchors.leftMargin: 6
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 6
            text: "Cancel"
            onClicked: {
                searchOut.start();
                searchOutVisibility.start();
            }
        }
    }


    /*****
     plugin UI dialog
    *****/

    Item {
        id: pluginUI
        anchors.fill: parent
        opacity: opacity_transparent
        visible: false;

        property string   foreignId: "NA"
        property QtObject uiObject

        MouseArea {
            anchors.fill: pluginUI
        }

        Rectangle {
            id: pluginUIBackground
            anchors.fill: pluginUI
        }
    }


    /*****
     collections dialog
    *****/

    Item {
        id: collections
        anchors.fill: parent
        opacity: opacity_transparent
        visible: false;

        MouseArea {
            anchors.fill: collections
        }

        Rectangle {
            id: collectionsBackground
            anchors.fill: collections
        }

        ListView {
            id: collectionsList
            anchors.right: parent.right
            anchors.rightMargin: 6
            anchors.left: parent.left
            anchors.leftMargin: 6
            anchors.top: parent.top
            anchors.topMargin: 6
            anchors.bottom: addCollectionText.top
            anchors.bottomMargin: 6
            model: collectionsItems
            delegate: collectionsElement
            spacing: 3
            clip: true

            ScrollBar.vertical: ScrollBar { }
        }

        TextField {
            id: addCollectionText
            anchors.left: parent.left
            anchors.leftMargin: 6
            anchors.right: addCollectionButton.left
            anchors.rightMargin: 6
            anchors.bottom: collectionsDone.top
            anchors.bottomMargin: 6
            onTextChanged: {
                addCollectionButton.enabled = (addCollectionText.text.length > 0);
            }
        }

        Button {
            id: addCollectionButton
            anchors.right: parent.right
            anchors.rightMargin: 6
            anchors.verticalCenter: addCollectionText.verticalCenter
            text: "Add"
            enabled: false
            onClicked: {
                var exists = false;
                for(var i = 0; i < collectionsItems.count; i++) {
                    if (collectionsItems.get(i).label == addCollectionText.text) {
                        exists = true;
                    }
                }
                if (!exists) {
                    collectionsItems.append({ label: addCollectionText.text });
                }
            }
        }

        Button {
            id: collectionsDone
            anchors.right: parent.right
            anchors.rightMargin: 6
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 6
            text: "Done"
            onClicked: {
                var retval = [];
                for(var i = 0; i < collectionsItems.count; i++) {
                    retval.push(collectionsItems.get(i).label);
                }
                collectionsDialogResults(retval);

                collectionsOut.start();
                collectionsOutVisibility.start();
            }
        }

        Button {
            anchors.left: parent.left
            anchors.leftMargin: 6
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 6
            text: "Cancel"
            onClicked: {
                collectionsOut.start();
                collectionsOutVisibility.start();
            }
        }
    }


    /*****
     about dialog
    *****/

    Item {
        id: about
        anchors.fill: parent
        opacity: opacity_transparent
        visible: false;

        MouseArea {
            anchors.fill: about
        }

        Rectangle {
            id: aboutBackground
            anchors.fill: about
        }

        MouseArea {
            anchors.fill: aboutLink
            cursorShape: Qt.PointingHandCursor
        }

        PlatformLabel {
            id: aboutName
            anchors.left: parent.left
            anchors.leftMargin: 6
            anchors.top: parent.top
            anchors.topMargin: 6
            font.pointSize: userMessage.font.pointSize * largeMul
            font.bold: true
            text: ""
        }
        PlatformLabel {
            id: aboutVersionLabel
            anchors.left: aboutName.right
            anchors.leftMargin: 6
            anchors.top: parent.top
            anchors.topMargin: 6
            font.pointSize: userMessage.font.pointSize * largeMul
            text: "version"
        }
        PlatformLabel {
            id: aboutVersion
            anchors.left: aboutVersionLabel.right
            anchors.leftMargin: 6
            anchors.top: parent.top
            anchors.topMargin: 6
            font.pointSize: userMessage.font.pointSize * largeMul
            text: ""
        }
        PlatformLabel {
            id: aboutDescription
            anchors.left: parent.left
            anchors.leftMargin: 6
            anchors.top: aboutName.bottom
            anchors.topMargin: 12
            font.italic: true
            text: ""
        }
        PlatformLabel {
            id: aboutLink
            anchors.left: parent.left
            anchors.leftMargin: 6
            anchors.top: aboutDescription.bottom
            anchors.topMargin: 12
            text: "<a href=\"https://launchpad.net/waver\">https://launchpad.net/waver</a>"
            onLinkActivated: {
                Qt.openUrlExternally("https://launchpad.net/waver");
            }
        }
        PlatformLabel {
            id: aboutCopyright
            anchors.left: parent.left
            anchors.leftMargin: 6
            anchors.right: parent.right
            anchors.rightMargin: 6
            anchors.top: aboutLink.bottom
            anchors.topMargin: 12
            anchors.bottom: aboutOK.top
            anchors.bottomMargin: 6
            elide: Text.ElideNone
            wrapMode: Text.WordWrap
            text: "Copyright (C) 2017 Peter Papp <peter.papp.p@gmail.com>\n\nThis is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.\n\nThis software is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.\n\nYou should have received a copy of the GNU General Public License (GPL.TXT) along with this software. If not, see http://www.gnu.org/licenses/"
        }
        Button {
            id: aboutOK
            anchors.right: parent.right
            anchors.rightMargin: 6
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 6
            text: "OK"
            onClicked: {
                aboutOut.start();
                aboutOutVisibility.start();
            }
        }
    }
}
