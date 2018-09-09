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

    readonly property real fontLargeMul: 1.75
    readonly property real fontSmallMul: .75
    readonly property int  duration_visible_before_fadeout: 5000
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
    readonly property int  tracks_per_source: 4
    readonly property int  loved_normal: 0;
    readonly property int  loved_loved: 1;
    readonly property int  loved_similar: 2;

    readonly property var playtimeTexts: ["3 minutes", "5 minutes", "7.5 minutes", "10 minutes", "15 minutes", "30 minutes"]
    readonly property var playtimeValues: [180000, 300000, 450000, 600000, 900000, 1800000]
    readonly property var playlistAddTypes: ["End", "Beginning", "Immediate"]

    property bool active : true
    property int  openUpY: 0;

    // these signals are processed by C++ (WaverApplication class)
    signal menuPause()
    signal menuResume()
    signal menuNext()
    signal menuCollection(variant collectionLabel)
    signal menuOptions()
    signal menuSourcePriorities()
    signal menuPlugin(variant foreignId)
    signal menuAbout()
    signal menuQuit()
    signal collectionsDialogResults(variant collectionsArray)
    signal sourcePrioritiesDialogResults(variant priorities)
    signal optionsDialogResults(variant options)
    signal pluginUIResults(variant foreignId, variant results)
    signal getOpenTracks(variant pluginId, variant parentId)
    signal startSearch(variant criteria)
    signal resolveOpenTracks(variant selected)
    signal trackAction(variant index, variant action)
    signal getDiagnostics(variant id)
    signal doneDiagnostics()


    /*****
     handlers for signals received from C++
    *****/

    // error messages and warnings
    function displayUserMessage(messageText, type)
    {
        if (!messageText) {
            messageText = "Unknown Error";
        }

        userMessageFadeoutAnimation.stop()

        if (type === "Info") {
            userMessage.color= "#336600"
            userMessage.styleColor= "#FFEE00"
        }
        else {
            userMessage.color= "#800000"
            userMessage.styleColor= "#E0B0B0"
        }

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

    function updateTrackInfo(title, performer, album, year, track, loved)
    {
        trackInfoOut.start();
        trackInfoChange.newTitle = title;
        trackInfoChange.newPerformer = performer;
        trackInfoChange.newAlbum = album;
        trackInfoChange.newYear = year;
        trackInfoChange.newTrack = track;
        trackInfoChange.loved = loved;
        trackInfoChange.start();
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
        while (nowPlayingMenu.count > 0) {
            nowPlayingMenu.removeItem(0);
        }

        var actions = text.split("||");
        for(var i = 0; i < actions.length; i++) {
            var elements = actions[i].split("|");
            nowPlayingMenu.addItem(Qt.createQmlObject("import QtQuick.Controls 2.0; MenuItem { text: \"" + elements[1] + "\"; onTriggered: trackAction(-1, \"" + elements[0] + "\"); }", nowPlayingMenu, "dynamicMenu"));
        }
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

    function addToPlaylist(pictureUrl, title, performer, actions, showActions, loved)
    {
        if (title.length < 1) {
            return;
        }

        playlistItems.append({
            labelTitle: title,
            labelPerformer: performer,
            actions: actions,
            imageSource: pictureUrl,
            initialShowActions: showActions,
            loved: loved
        });

        playlistAddButton.enabled = (playlistItems.count < 25);
        searchButton.enabled = ((searchText.text.length >= 3) && (playlistItems.count < 25));
    }

    function setPlaylistIndex(index)
    {
        playlist.currentIndex = index;
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

    function addToPluginsWithUIList(id, label)
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
            imageSource: "/images/plugin_settings.png",
            clickId: lastId + 1,
            foreignId: id
        });
    }


    // plugin user interface

    function displayPluginUI(id, qml, header)
    {
        pluginUIHeader.text = header

        var pluginUIContent = Qt.createQmlObject(qml, pluginUI, "");
        pluginUIContent.done.connect(finishPluginUI);
        pluginUIContent.apply.connect(sendPluginUI);

        pluginUI.foreignId = id;
        pluginUI.uiObject  = pluginUIContent;

        pluginUIOuter.visible = true;
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

        if ((playlistAddSelectableItems.count == 1) && (playlistAddSelectableItems.get(0).label == "Loading...")) {
            playlistAddSelectableItems.clear();
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

        playlistAddSelectables.contentY = openUpY;
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


    // diagnostics

    function clearDiagnosticsSelectorList()
    {
        var i = 0;
        while (i < diagnosticsSelectorItems.count) {
            if (diagnosticsSelectorItems.get(i).foreignId != "error_log") {
                diagnosticsSelectorItems.remove(i);
            }
            else {
                i++;
            }
        }
    }

    function addToDiagnosticsSelectorList(id, label)
    {
        diagnosticsSelectorItems.append({
            modelData: label,
            foreignId: id
        });
    }

    function displayDiagnosticsMessage(id, text) {
        if (diagnostics.visible) {
            if (diagnosticsSelectorItems.get(diagnosticsSelector.currentIndex).foreignId == id) {
                var x = diagnosticsFlickable.contentX
                var y = diagnosticsFlickable.contentY
                diagnosticsText.text = text
                diagnosticsFlickable.contentX = x
                diagnosticsFlickable.contentY = y
            }
            else {
                var stoppedMsg = "Diagnostic updates have been stopped for this category"
                if (diagnosticsText.text.search(stoppedMsg) < 0) {
                    diagnosticsText.text = "<i><font color=\"#880000\">" + stoppedMsg + "</font></i><br><br>" + diagnosticsText.text
                    diagnosticsFlickable.contentX = 0
                    diagnosticsFlickable.contentY = 0
                }
            }
        }
    }


    // source priorities

    function clearSourcePrioritiesList()
    {
        sourcePrioritiesItems.clear();
    }

    function addToSourcePrioritiesList(id, name, priority, recurring, love)
    {
        sourcePrioritiesItems.append({
            pluginId       : id,
            pluginName     : name,
            pluginPriority : priority,
            pluginRecurring: recurring,
            pluginLove     : love,
        });
    }


    // options

    function optionsData(streamPlayTime, lovedStreamPlayTime, playlistAddMode, startColl)
    {
        var colls = collection.model;
        colls.unshift("--- last used ---");
        startCollection.model = colls;

        for(var i = 0; i < colls.length; i++) {
            if (colls[i] == startColl) {
                startCollection.currentIndex = i;
            }
        }

        var index = -1;
        for (var i = 0; i < playtimeValues.length; ++i) {
            if (playtimeValues[i] == streamPlayTime) {
                index = i;
            }
        }
        if (index < 0) {
            index = 2;
        }
        castPlayTime.value = index;

        index = -1;
        for (var i = 0; i < playtimeValues.length; ++i) {
            if (playtimeValues[i] == lovedStreamPlayTime) {
                index = i;
            }
        }
        if (index < 0) {
            index = 2;
        }
        castLovedPlayTime.value = index;

        playlistAddType.value = playlistAddMode
    }


    // about dialog
    function aboutDialog(data)
    {
        var dataParsed = JSON.parse(data);

        aboutName.text          = dataParsed["name"];
        aboutVersion.text       = dataParsed["version"];
        aboutDescription.text   = dataParsed["description"];
        aboutCopyright.text     = dataParsed["copyright"];
        aboutLink.text          = dataParsed["website"];
        aboutEmail.text         = dataParsed["email"];
        aboutFlickableText.text = dataParsed["credits"].replace(/\n/g, "<br>") + "<br><br><b>" + dataParsed["privacy"].replace(/\n/g, "<br>") + "</b><br><br>" + dataParsed["license"].replace(/\n/g, "<br>");

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
            sourcePriorities.visible = true;
            sourcePrioritiesIn.start();

            menuSourcePriorities();
            break;

        case 3:
            options.visible = true;
            optionsIn.start();

            menuOptions();
            break;

        case 4:
            diagnostics.visible = true;
            diagnosticsIn.start();
            getDiagnostics(diagnosticsSelectorItems.get(diagnosticsSelector.currentIndex).foreignId);
            break;

        case 5:
            menuAbout();
            break;
        }
    }


    /*****
     helpers
    *****/

    function playTimeTextFromValue(value) {
        return playtimeTexts[value];
    }

    function playTimeValueFromText(text) {
        for (var i = 0; i < playtimeTexts.length; ++i) {
            if (playtimeTexts[i].toLowerCase().indexOf(text.toLowerCase()) === 0) {
                return i;
            }
        }
        return "7.5 minutes";
    }


    /*****
     animation definitions
    *****/

    // activation

    Timer {
        id: activatedTimer
        interval: 100
        onTriggered: active = true
    }

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

            if (loved == loved_normal) {
                title.color = "#000000";
                performer.color = "#000000";
                album.color = "#000000";
                year.color = "#000000";
                track.color = "#000000";
            }
            else if (loved == loved_loved) {
                title.color = "#440000";
                performer.color = "#440000";
                album.color = "#440000";
                year.color = "#440000";
                track.color = "#440000";
            }
            else if (loved == loved_similar) {
                title.color = "#000044";
                performer.color = "#000044";
                album.color = "#000044";
                year.color = "#000044";
                track.color = "#000044";
            }

            trackInfoIn.start();
        }

        property string newTitle
        property string newPerformer
        property string newAlbum
        property string newYear
        property string newTrack
        property int    loved
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
        target: pluginUIOuter
        property: "opacity"
        from: opacity_transparent
        to: opacity_opaque
        duration: duration_fadeout
    }

    NumberAnimation {
        id: pluginUIout
        target: pluginUIOuter
        property: "opacity"
        from: opacity_opaque
        to: opacity_transparent
        duration: duration_fadeout
    }

    Timer {
        id: pluginUIoutVisibility
        interval: duration_fadeout + 25
        onTriggered: {
            pluginUIOuter.visible = false;
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


    // source priorities dialog transitions

    NumberAnimation {
        id: sourcePrioritiesIn
        target: sourcePriorities
        property: "opacity"
        from: opacity_transparent
        to: opacity_opaque
        duration: duration_fadeout
    }

    NumberAnimation {
        id: sourcePrioritiesOut
        target: sourcePriorities
        property: "opacity"
        from: opacity_opaque
        to: opacity_transparent
        duration: duration_fadeout
    }

    Timer {
        id: sourcePrioritiesOutVisibility
        interval: duration_fadeout + 25
        onTriggered: {
            sourcePriorities.visible = false;
        }
    }


    // options dialog transitions

    NumberAnimation {
        id: optionsIn
        target: options
        property: "opacity"
        from: opacity_transparent
        to: opacity_opaque
        duration: duration_fadeout
    }

    NumberAnimation {
        id: optionsOut
        target: options
        property: "opacity"
        from: opacity_opaque
        to: opacity_transparent
        duration: duration_fadeout
    }

    Timer {
        id: optionsOutVisibility
        interval: duration_fadeout + 25
        onTriggered: {
            options.visible = false;
        }
    }


    // diagnostics dialog transitions

    NumberAnimation {
        id: diagnosticsIn
        target: diagnostics
        property: "opacity"
        from: opacity_transparent
        to: opacity_opaque
        duration: duration_fadeout
    }

    NumberAnimation {
        id: diagnosticsOut
        target: diagnostics
        property: "opacity"
        from: opacity_opaque
        to: opacity_transparent
        duration: duration_fadeout
    }

    Timer {
        id: diagnosticsOutVisibility
        interval: duration_fadeout + 25
        onTriggered: {
            diagnostics.visible = false;
        }
    }


    // about dialog transitions

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

    FontLoader {
        id: decorativeFont
        source: "/ttf/RockSalt.ttf"
    }

    // menu

    ListModel {
        id: menuItems

        ListElement {
            labelText: "Collections"
            imageSource: "/images/collections.png"
            clickId: 1
            foreignId: "N/A"
        }

        ListElement {
            labelText: "Source priorities"
            imageSource: "/images/collections.png"
            clickId: 2
            foreignId: "N/A"
        }

        ListElement {
            labelText: "Options"
            imageSource: "/images/collections.png"
            clickId: 3
            foreignId: "N/A"
        }

        ListElement {
            labelText: "Diagnostics"
            imageSource: "/images/diagnostics.png"
            clickId: 4
            foreignId: "N/A"
        }

        ListElement {
            labelText: "About"
            imageSource: "/images/about.png"
            clickId: 5
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

                Label {
                    id: menuElementLabel
                    text: labelText
                    anchors.verticalCenter: menuElementImage.verticalCenter
                }

                Label {
                    text: " "
                    anchors.verticalCenter: menuElementImage.verticalCenter
                }
            }
        }
    }


    // now playing

    Menu {
        id: nowPlayingMenu
    }


    // playlist

    ListModel {
        id: playlistItems

        ListElement {
            labelTitle: "Title"
            labelPerformer: "Performer"
            actions: ""
            imageSource: "/images/waver.png"
            loved: 0
        }
    }

    Component {
        id: playlistElement

        Item {
            id: playlistElementOuter
            height: playlistElementImage.height + (playlistElementInner.anchors.margins * 2) + playlistElementBorder.anchors.bottomMargin
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
                    onStatusChanged: if (playlistElementImage.status == Image.Error) playlistElementImage.source = "/images/waver.png"
                }

                MouseArea {
                    anchors.fill: playlistElementInner
                    acceptedButtons: Qt.RightButton
                    onClicked: {
                        if (playlistElementMenu.count == 0) {
                            var actionsSplit = actions.split("||");
                            for(var i = 0; i < actionsSplit.length; i++) {
                                var actionSplit = actionsSplit[i].split("|");
                                playlistElementMenu.addItem(Qt.createQmlObject("import QtQuick.Controls 2.0; MenuItem { text: \"" + actionSplit[1] + "\"; onTriggered: trackAction(index, \"" + actionSplit[0] + "\"); }", playlistElementMenu, "dynamicMenu"));
                            }
                        }
                        playlistElementMenu.popup();
                    }
                }

                Rectangle {
                    anchors.fill: playlistElementImage
                    border.color: "#666666"
                    color: "transparent"
                }

                Column {
                    id: playlistElementLabels
                    spacing: -3
                    anchors.left: playlistElementImage.right
                    anchors.right: playlistElementInner.right
                    anchors.verticalCenter: playlistElementImage.verticalCenter

                    Label {
                        id: playlistElementTitle
                        anchors.left: playlistElementLabels.left
                        anchors.right: playlistElementLabels.right
                        anchors.leftMargin: 6
                        anchors.rightMargin: 3
                        text: labelTitle
                        font.bold: true
                        font.family: decorativeFont.name
                        font.pointSize: userMessage.font.pointSize * fontSmallMul
                        elide: Text.ElideRight
                        color: (loved == loved_loved) ? "#440000" : (loved == loved_similar) ? "#000044" : "#000000"
                    }

                    Label {
                        id: playlistElementPerformer
                        anchors.left: playlistElementLabels.left
                        anchors.right: playlistElementLabels.right
                        anchors.leftMargin: 6
                        anchors.rightMargin: 3
                        text: labelPerformer
                        font.family: decorativeFont.name
                        font.pointSize: userMessage.font.pointSize * fontSmallMul
                        elide: Text.ElideRight
                        color: (loved == loved_loved) ? "#440000" : (loved == loved_similar) ? "#000044" : "#000000"
                    }
                }
            }

            Menu {
                id: playlistElementMenu
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

            Label {
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

            Label {
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
                        if ((labelProperty == "..") || (labelProperty == "Loading...")) {
                            var historyY = playlistAdd.directoryHistory.pop();
                            openUpY = historyY.y
                        }
                        else {
                            playlistAdd.directoryHistory.push({
                                pluginId: pluginIdProperty,
                                id: idProperty,
                                y: playlistAddSelectables.contentY
                            });
                            openUpY = 0;
                        }

                        playlistAddSelectableItems.clear();

                        if (playlistAdd.directoryHistory.length > 1) {
                            var history = playlistAdd.directoryHistory[playlistAdd.directoryHistory.length - 2]
                            if (history) {
                                playlistAddSelectableItems.append({
                                    pluginId: history.pluginId,
                                    actions: "<i>--- <a href=\"open\">cancel</a> ---</i>",
                                    label: "Loading...",
                                    id: history.id
                                });
                            }
                        }
                        else {
                            playlistAddSelectableItems.append({
                                pluginId: "LOADING",
                                actions: "<i>--- please wait while getting data ---</i>",
                                label: "Loading...",
                                id: "LOADING",
                            });
                        }

                        getOpenTracks(pluginIdProperty, idProperty);
                    }

                    if (link == "add") {
                        playlistAddSelectedItems.append({
                            pluginId: pluginIdProperty,
                            id: idProperty,
                            label: labelProperty
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
            text: label
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

            Label {
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

            Label {
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
                            id: idProperty,
                            label: labelProperty
                        });
                        searchSelected.positionViewAtEnd();
                    }
                }

                property string pluginIdProperty: pluginId
                property string idProperty      : id
                property string labelProperty   : label
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
            text: label
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
                cursorShape: (String(label).localeCompare(String("Default")) ? Qt.PointingHandCursor : Qt.ArrowCursor)
            }

            Label {
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

            Label {
                id: elementActions
                text: (String(label).localeCompare(String("Default")) ? "<a href=\"delete\">Delete</a>" : "<i>--- can not be deleted ---</i>")
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


    // source priorities

    ListModel {
        id: sourcePrioritiesItems
    }

    Component {
        id: sourcePrioritiesElement

        Item {
            height: sourcePrioritySlider.height + sourceLovedFrequency.height + 18
            width: sourcePrioritiesList.width

            Rectangle {
                anchors.fill: parent
                border.color: "#666666"
                color: "transparent"
                radius: 3
            }

            Label {
                text: pluginName
                font.bold: true
                elide: Text.ElideRight
                anchors.left: parent.left
                anchors.leftMargin: 6
                anchors.right: parent.horizontalCenter
                anchors.verticalCenter: sourcePrioritySlider.verticalCenter
            }

            Slider {
                id: sourcePrioritySlider
                anchors.left: parent.horizontalCenter
                anchors.right: sourcePriorityFeedback.left
                value: pluginPriority
                from: 1
                to: 10
                onValueChanged: {
                    pluginPriority = Math.round(value);
                }
            }

            Label {
                id: sourcePriorityFeedback
                anchors.right: parent.right
                anchors.rightMargin: 6
                text: pluginPriority
                anchors.verticalCenter: sourcePrioritySlider.verticalCenter
            }

            Label {
                id: sourceLovedFrequencyLabel
                anchors.verticalCenter: sourceLovedFrequency.verticalCenter
                anchors.left: parent.left
                anchors.leftMargin: 12
                text: "Loved:"
            }

            ComboBox {
                id: sourceLovedFrequency
                anchors.top: sourcePrioritySlider.bottom
                anchors.topMargin: 12
                anchors.left: sourceLovedFrequencyLabel.right
                anchors.right: parent.horizontalCenter
                anchors.leftMargin: 6
                background: Rectangle {
                    color: "transparent"
                    border.width: 1
                    border.color: "#CCCCCC"
                }
                model: ListModel {
                    ListElement { text: "Rare" }
                    ListElement { text: "Normal" }
                    ListElement { text: "Frequent" }
                }
                Component.onCompleted: {
                    currentIndex = find(pluginLove)
                }
                onActivated: {
                    pluginLove = currentText
                }
            }

            Label {
                id: sourcePriorityRecurringLabel
                anchors.verticalCenter: sourceLovedFrequency.verticalCenter
                anchors.right: sourcePriorityRecurring.left
                text: "Recurring:"
            }

            RadioButton {
                id: sourcePriorityRecurring
                anchors.verticalCenter: sourceLovedFrequency.verticalCenter
                anchors.right: parent.right
                anchors.rightMargin: 12
                ButtonGroup.group: radios
                checked: pluginRecurring
                onClicked: {
                    for(var i = 0; i < sourcePrioritiesItems.count; i++) {
                        sourcePrioritiesItems.get(i).pluginRecurring = false;
                    }
                    pluginRecurring = checked;
                    if (!checked) {
                        sourcePriorityNoRecurring.checked = true;
                    }
                }
            }
        }
    }

    // diagnostics

    ListModel {
        id: diagnosticsSelectorItems

        ListElement {
            modelData: "Error Log"
            foreignId: "error_log"
        }
    }

    Component {
        id: diagnosticsSelectorElement

        Label {
            text: labelText
            elide: Text.ElideRight
        }
    }


    /*****
     the actual UI, finally
    *****/

    // menu

    ToolButton {
        id: menuButton
        background: Image {
            source: "/images/menu.png"
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
            source: "/images/pause.png"
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
            source: "/images/resume.png"
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
            source: "/images/next.png"
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
            source: "/images/quit.png"
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
                    source: "/images/waver.png"
                    smooth: true
                    visible: false
                    anchors.right: artArea.right
                    anchors.left: artArea.left
                    anchors.top: artArea.top
                    anchors.bottom: artArea.bottom
                    onStatusChanged: if (art1.status == Image.Error) art1.source = "/images/waver.png"
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
                    source: "/images/waver.png"
                    smooth: true
                    visible: false
                    anchors.right: artArea.right
                    anchors.left: artArea.left
                    anchors.top: artArea.top
                    anchors.bottom: artArea.bottom
                    onStatusChanged: if (art2.status == Image.Error) art2.source = "/images/waver.png"
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

            // labels and their background

            Rectangle {
                id: label_bkg
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
                    GradientStop { position: 0.475; color: "transparent" }
                    GradientStop { position: 1.0; color: "#AAAAAA" }
                }
            }

            Label {
                id: title
                text: "Title"
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                font.family: decorativeFont.name
                font.bold: true
                font.pointSize: userMessage.font.pointSize * fontLargeMul
                lineHeight: .5
                lineHeightMode: Text.ProportionalHeight
                anchors.right: parent.right
                anchors.rightMargin: 9
                anchors.left: parent.left
                anchors.leftMargin: 9
                anchors.bottom: performer.top
                anchors.bottomMargin: 12
            }

            Label {
                id: performer
                text: "Performer"
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                font.family: decorativeFont.name
                font.pointSize: userMessage.font.pointSize * fontLargeMul
                lineHeight: .5
                lineHeightMode: Text.ProportionalHeight
                anchors.right: parent.right
                anchors.rightMargin: 9
                anchors.left: parent.left
                anchors.leftMargin: 9
                anchors.bottom: album.top
                anchors.bottomMargin: 18
            }

            Label {
                id: album
                text: "Album"
                elide: Text.ElideRight
                font.pointSize: userMessage.font.pointSize * fontSmallMul
                anchors.left: parent.left
                anchors.leftMargin: 9
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 3
                width: label_bkg.width * 0.85
            }
            Label {
                id: track
                text: "Track"
                font.pointSize: userMessage.font.pointSize * fontSmallMul
                font.italic: true
                anchors.right: year.left
                anchors.rightMargin: 6
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 3
            }
            Label {
                id: year
                text: "Year"
                font.pointSize: userMessage.font.pointSize * fontSmallMul
                anchors.right: parent.right
                anchors.rightMargin: 9
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 3
            }

            // track actions - normally invisible

            MouseArea {
                anchors.fill: label_bkg
                acceptedButtons: Qt.RightButton
                onClicked: {
                    nowPlayingMenu.popup();
                }
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
                        id: "",
                        y: 0
                    });
                    openUpY = 0;

                    getOpenTracks("", "");

                    playlistAdd.visible = true;
                    playlistAddIn.start();

                    playlistAddSelectables.focus = true;
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

    Label {
        id: position_elapsed
        text: "00:00"
        font.pointSize: userMessage.font.pointSize * fontSmallMul
        anchors.left: parent.left
        anchors.leftMargin: 6
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 4
    }

    Label {
        id: position_remaining
        text: "00:00"
        font.pointSize: userMessage.font.pointSize * fontSmallMul
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


    // error messages and warnings - in the center of the screen, normally invisible
    Label {
        id: userMessage
        text: ""
        wrapMode: Text.Wrap
        width: app.width - 12;
        horizontalAlignment: Text.AlignHCenter
        font.bold: true
        style: Text.Outline
        anchors.horizontalCenter: app.contentItem.horizontalCenter
        anchors.verticalCenter: app.contentItem.verticalCenter
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

            Keys.onPressed: {
                var index = -1;
                var i     = 1;
                while ((i < playlistAddSelectableItems.count) && (index < 0)) {
                    if (playlistAddSelectableItems.get(i).label.toLowerCase().charAt(0) == event.text.toLowerCase().charAt(0)) {
                        index = i;
                    }
                    i++;
                }
                if (index > -1) {
                    playlistAddSelectables.positionViewAtIndex(index, ListView.Beginning);
                }
            }
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
        id: pluginUIOuter
        anchors.fill: parent
        opacity: opacity_transparent
        visible: false;

        Rectangle {
            anchors.fill: pluginUIOuter
        }

        Label {
            id: pluginUIHeader
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: parent.top
            anchors.topMargin: 3
            font.underline: true
        }

        Item {
            id: pluginUI
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: pluginUIHeader.bottom
            anchors.bottom: parent.bottom

            property string   foreignId: "N/A"
            property QtObject uiObject

            MouseArea {
                anchors.fill: pluginUI
            }
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
      source priorities dialog
    *****/

    Item {
        id: sourcePriorities
        anchors.fill: parent
        opacity: opacity_transparent
        visible: false

        MouseArea {
            anchors.fill: sourcePriorities
        }

        Rectangle {
            id: sourcePrioritiesBackground
            anchors.fill: sourcePriorities
        }

        ButtonGroup {
            id: radios
        }

        Item {
            id: sourcePriorityNoRecurringContainer
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.topMargin: 6
            anchors.leftMargin: 6
            height: sourcePriorityNoRecurring.height
            width: sourcePrioritiesList.width

            Rectangle {
                anchors.fill: parent
                border.color: "#666666"
                color: "#EEEEEE"
                radius: 3
            }

            Label {
                text: "No recurring plugin"
                elide: Text.ElideRight
                anchors.left: parent.left
                anchors.leftMargin: 6
                anchors.right: parent.horizontalCenter
                anchors.verticalCenter: sourcePriorityNoRecurring.verticalCenter
            }

            RadioButton {
                id: sourcePriorityNoRecurring
                anchors.right: parent.right
                anchors.rightMargin: 12
                anchors.top: parent.top
                ButtonGroup.group: radios
                checked: true
                onClicked: {
                    for(var i = 0; i < sourcePrioritiesItems.count; i++) {
                        sourcePrioritiesItems.get(i).pluginRecurring = false;
                    }
                }
            }
        }

        ListView {
            id: sourcePrioritiesList
            model: sourcePrioritiesItems
            delegate: sourcePrioritiesElement
            anchors.left: sourcePrioritiesBackground.left
            anchors.leftMargin: 6
            anchors.right: sourcePrioritiesBackground.right
            anchors.rightMargin: 6
            anchors.top: sourcePriorityNoRecurringContainer.bottom
            anchors.topMargin: 6
            anchors.bottom: sourcePrioritiesDone.top
            anchors.bottomMargin: 6
            spacing: 6
            clip: true
        }

        Button {
            id: sourcePrioritiesDone
            anchors.right: parent.right
            anchors.rightMargin: 6
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 6
            text: "Done"
            onClicked: {
                var retval = [];
                for(var i = 0; i < sourcePrioritiesItems.count; i++) {
                    retval.push({
                         id: sourcePrioritiesItems.get(i).pluginId,
                         priority: sourcePrioritiesItems.get(i).pluginPriority,
                         recurring: sourcePrioritiesItems.get(i).pluginRecurring,
                         lovedMode: sourcePrioritiesItems.get(i).pluginLove
                    });
                }
                sourcePrioritiesDialogResults(JSON.stringify(retval));

                sourcePrioritiesOut.start();
                sourcePrioritiesOutVisibility.start();
            }
        }

        Button {
            anchors.left: parent.left
            anchors.leftMargin: 6
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 6
            text: "Cancel"
            onClicked: {
                sourcePrioritiesOut.start();
                sourcePrioritiesOutVisibility.start();
            }
        }
    }


    /*****
     options dialog
    *****/

    Item {
        id: options
        anchors.fill: parent
        opacity: opacity_transparent
        visible: false

        MouseArea {
            anchors.fill: options
        }

        Rectangle {
            id: optionsBackground
            anchors.fill: options
        }

        Label {
            id: globalOptionsLabel
            text: "Global Options"
            font.bold: true
            anchors.left: parent.left
            anchors.leftMargin: 6
            anchors.top: parent.top
            anchors.topMargin: 6
        }

        Item {
            id: startCollectionItem
            anchors.left: parent.left
            anchors.leftMargin: 12
            anchors.right: parent.right
            anchors.rightMargin: 6
            anchors.top: globalOptionsLabel.bottom
            anchors.topMargin: 6
            height: startCollection.height

            Label {
                id: startCollectionLabel
                anchors.left: parent.left
                anchors.verticalCenter: startCollection.verticalCenter
                text: "Start with collection"
            }
            ComboBox {
                id: startCollection
                anchors.right: parent.right
                anchors.left: startCollectionLabel.right
                anchors.leftMargin: 6
                anchors.top:parent.top
            }
        }

        Label {
            id: collectionOptionsLabel
            text: "Current Collection Options"
            font.bold: true
            anchors.left: parent.left
            anchors.leftMargin: 6
            anchors.top: startCollectionItem.bottom
            anchors.topMargin: 24
        }

        Item {
            id: castPlayTimeItem
            anchors.left: parent.left
            anchors.leftMargin: 12
            anchors.right: parent.right
            anchors.rightMargin: 6
            anchors.top: collectionOptionsLabel.bottom
            anchors.topMargin: 6
            height: castPlayTime.height

            Label {
                anchors.left: parent.left
                anchors.verticalCenter: castPlayTime.verticalCenter
                text: "Stream play time"
            }
            SpinBox {
                id: castPlayTime
                anchors.right: parent.right
                anchors.left: parent.horizontalCenter
                anchors.top:parent.top
                from: 0
                to: items.length - 1

                property var items: playtimeTexts

                textFromValue: function(value) {
                    return playTimeTextFromValue(value);
                }

                valueFromText: function(text) {
                    return playTimeValueFromText(text);
                }
            }
        }

        Item {
            id: castLovedPlayTimeItem
            anchors.left: parent.left
            anchors.leftMargin: 12
            anchors.right: parent.right
            anchors.rightMargin: 6
            anchors.top: castPlayTimeItem.bottom
            anchors.topMargin: 6
            height: castLovedPlayTime.height

            Label {
                anchors.left: parent.left
                anchors.verticalCenter: castLovedPlayTime.verticalCenter
                text: "Loved stream play time"
            }
            SpinBox {
                id: castLovedPlayTime
                anchors.right: parent.right
                anchors.left: parent.horizontalCenter
                anchors.top:parent.top
                from: 0
                to: items.length - 1

                property var items: playtimeTexts

                textFromValue: function(value) {
                    return playTimeTextFromValue(value);
                }

                valueFromText: function(text) {
                    return playTimeValueFromText(text);
                }
            }
        }

        Item {
            id: playlistAddTypeItem
            anchors.left: parent.left
            anchors.leftMargin: 12
            anchors.right: parent.right
            anchors.rightMargin: 6
            anchors.top: castLovedPlayTimeItem.bottom
            anchors.topMargin: 6
            height: playlistAddType.height

            Label {
                anchors.left: parent.left
                anchors.verticalCenter: playlistAddType.verticalCenter
                text: "Add to playlist"
            }
            SpinBox {
                id: playlistAddType
                anchors.right: parent.right
                anchors.left: parent.horizontalCenter
                anchors.top:parent.top
                from: 0
                to: items.length - 1

                property var items: playlistAddTypes

                textFromValue: function(value) {
                    return playlistAddTypes[value];
                }

                valueFromText: function(text) {
                    for (var i = 0; i < items.length; ++i) {
                        if (playlistAddTypes[i].toLowerCase().indexOf(text.toLowerCase()) === 0)
                            return i;
                        }
                        return 0;
                }
            }
        }

        Button {
            id: optionsDone
            anchors.right: parent.right
            anchors.rightMargin: 6
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 6
            text: "Done"
            onClicked: {
                var retval = { castPlayTime: playtimeValues[castPlayTime.value], castLovedPlayTime: playtimeValues[castLovedPlayTime.value], playlistAddMode: playlistAddType.value, startupCollection: startCollection.currentText };

                optionsDialogResults(JSON.stringify(retval));

                optionsOut.start();
                optionsOutVisibility.start();
            }
        }

        Button {
            anchors.left: parent.left
            anchors.leftMargin: 6
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 6
            text: "Cancel"
            onClicked: {
                optionsOut.start();
                optionsOutVisibility.start();
            }
        }
    }


    /*****
     diagnostics dialog
    *****/

    Item {
        id: diagnostics
        anchors.fill: parent
        opacity: opacity_transparent
        visible: false;

        MouseArea {
            anchors.fill: diagnostics
        }

        Rectangle {
            id: diagnosticsBackground
            anchors.fill: diagnostics
        }

        ComboBox {
            id: diagnosticsSelector
            anchors.left: diagnostics.left
            anchors.leftMargin: 6
            anchors.right: diagnostics.right
            anchors.rightMargin: 4
            anchors.top: parent.top
            anchors.topMargin: 6
            model: diagnosticsSelectorItems
            onActivated: {
                diagnosticsText.text = ""
                diagnosticsFlickable.contentX = 0
                diagnosticsFlickable.contentY = 0
                getDiagnostics(diagnosticsSelectorItems.get(diagnosticsSelector.currentIndex).foreignId);
            }
        }

        Flickable {
            id: diagnosticsFlickable
            anchors.left: parent.left
            anchors.leftMargin: 6
            anchors.right: parent.right
            anchors.rightMargin: 6
            anchors.top: diagnosticsSelector.bottom
            anchors.topMargin: 6
            anchors.bottom: diagnosticsClose.top
            anchors.bottomMargin: 6
            contentWidth: diagnosticsText.width
            contentHeight: diagnosticsText.height
            clip: true

            Label {
                id: diagnosticsText
                font.pointSize: userMessage.font.pointSize * fontSmallMul
            }
        }

        Button {
            id: diagnosticsClose
            anchors.right: parent.right
            anchors.rightMargin: 6
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 6
            text: "OK"
            onClicked: {
                diagnosticsOut.start();
                diagnosticsOutVisibility.start();
                doneDiagnostics();
                diagnosticsText.text = ""
                diagnosticsFlickable.contentX = 0
                diagnosticsFlickable.contentY = 0
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

        Label {
            id: aboutName
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: parent.top
            anchors.topMargin: 6
            font.pointSize: userMessage.font.pointSize * fontLargeMul
            font.bold: true
        }

        Label {
            id: aboutDescription
            anchors.left: parent.left
            anchors.leftMargin: 6
            anchors.top: aboutName.bottom
            anchors.topMargin: 12
            font.italic: true
        }

        Label {
            id: aboutVersionLabel
            anchors.right: aboutVersion.left
            anchors.rightMargin: 6
            anchors.top: aboutName.bottom
            anchors.topMargin: 12
            text: "version"
        }

        Label {
            id: aboutVersion
            anchors.right: parent.right
            anchors.rightMargin: 6
            anchors.top: aboutName.bottom
            anchors.topMargin: 12
        }

        Label {
            id: aboutLink
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: aboutDescription.bottom
            anchors.topMargin: 12
            onLinkActivated: Qt.openUrlExternally(link);
        }

        Label {
            id: aboutCopyright
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: aboutLink.bottom
            anchors.topMargin: 18
        }

        Label {
            id: aboutEmail
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: aboutCopyright.bottom
            anchors.topMargin: 6
            onLinkActivated: Qt.openUrlExternally(link);
        }

        Flickable {
            id: aboutFlickable
            anchors.left: parent.left
            anchors.leftMargin: 6
            anchors.right: parent.right
            anchors.rightMargin: 6
            anchors.top: aboutEmail.bottom
            anchors.topMargin: 24
            anchors.bottom: aboutOK.top
            anchors.bottomMargin: 12
            contentWidth: aboutFlickableText.width
            contentHeight: aboutFlickableText.height
            clip: true

            Label {
                id: aboutFlickableText
                wrapMode: Text.WordWrap
                width: aboutFlickable.width
                font.pointSize: userMessage.font.pointSize * fontSmallMul
                onLinkActivated: Qt.openUrlExternally(link);
            }
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
