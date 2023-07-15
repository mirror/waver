import QtQml 2.3
import QtQuick 2.12
import QtQuick.Controls 2.3
import QtQuick.Controls.Material 2.3
import QtQuick.Controls.Universal 2.3



Item {
    id: playlistRoot

    property bool  borderVisible: true
    property bool  isFocused: false
    property color borderColor: "#666666"
    property color focusBorderColor: ((Qt.platform.os === "windows") || (Qt.platform.os === "winrt")) ? Universal.accent : Material.accent;
    property int   imageSize: 24
    property int   fontSize: 36
    property bool  titleCurlySpecial: true

    signal itemClicked(int index, int action);
    signal itemDragDropped(int index, int destinationIndex)
    signal explorerItemDragDroped(string id, int destinationIndex)

    function addItem(title, artist, group, image, selected, ampacheURL)
    {
        if ((typeof selected === 'undefined') || (selected === null)) {
            selected = false;
        }

        if (titleCurlySpecial) {
            var curly = title.indexOf('{');
            if (curly >= 0) {
                title = title.substr(0, curly).trim();
            }
        }

        var newDict = {
            title: title,
            artist: artist,
            group: group,
            image: image,
            selected: selected,
            busy: false,
            downloadPercent: 0,
            pcmPercent: 0,
            ampacheURL: ampacheURL,
            isError: false,
            errorMessage: "",
        }

        playlistItems.append(newDict);

        if (internal.currentIndex < playlistItems.count) {
            playlistItemsView.currentIndex = internal.currentIndex;
        }
    }

    function clearItems()
    {
        if (playlistItemsView.currentIndex < 0) {
            internal.currentIndex = playlistItems.count;
        }
        else {
            internal.currentIndex = playlistItemsView.currentIndex;
        }
        playlistItems.clear();
    }

    function moveSelectionDown()
    {
        if ((playlistItems.count >= 1) && (playlistItemsView.currentIndex < playlistItems.count - 1)) {
            playlistItemsView.currentIndex++;
        }
    }

    function moveSelectionUp()
    {
        if ((playlistItems.count >= 1) && (playlistItemsView.currentIndex > 0)) {
            playlistItemsView.currentIndex--;
        }
    }

    function setBusy(index, busy)
    {
        playlistItems.setProperty(index, "busy", busy);
    }

    function setDecoding(index, downloadPercent, pcmPercent)
    {
        if (playlistItems.get(index).downloadPercent !== downloadPercent) {
            playlistItems.setProperty(index, "downloadPercent", downloadPercent);
        }
        if (playlistItems.get(index).pcmPercent !== pcmPercent) {
            playlistItems.setProperty(index, "pcmPercent", pcmPercent);
        }
    }

    function setPlaylistBigBusy(busy)
    {
        playlistBigBusy.visible = busy;
    }

    function setSelected(index, selected)
    {
        playlistItems.setProperty(index, "selected", selected);
    }

    function setTotalTime(totalTimeText)
    {
        if (totalTimeText.length === 0) {
            totalTime.visible = false;
            totalTimeBackground.visible = false;
            return;
        }

        totalTime.text = totalTimeText;
        totalTime.visible = true;
        totalTimeBackground.visible = true;
    }

    function simulateRightClick()
    {
        if (playlistItemsView.currentItem != null) {
            playlistMenu.x = 20
            playlistMenu.y = playlistItemsView.currentItem.y
            playlistMenu.open();
        }
    }


    QtObject {
        id: internal

        property int currentIndex   : -1
        property int dropSourceIndex: 0
        property int dropTargetIndex: 0

        function getLabelColor(isError)
        {
            if ((Qt.platform.os === "windows") || (Qt.platform.os === "winrt")) {
                return isError ? Universal.accent : Universal.foreground;
            }
            return isError ? Material.accent : Material.foreground;
        }
    }

    GlobalConstants {
        id: globalConstants
    }

    Menu {
        id: playlistMenu

        MenuItem {
            id: shufflePlaylistMenu

            icon.source: "qrc:///icons/shuffle.ico"
            icon.height: 24
            icon.width: 24
            text: qsTr("Shuffle playlist")
            font.pointSize: fontSize

            onTriggered: itemClicked(playlistItemsView.currentIndex, globalConstants.action_shuffle_playlist);
        }
        MenuSeparator { }
        MenuItem {
            id: playPlaylistMenu

            icon.source: "qrc:///icons/play.ico"
            icon.height: 24
            icon.width: 24
            text: qsTr("Play Now")
            font.pointSize: fontSize

            onTriggered: itemClicked(playlistItemsView.currentIndex, globalConstants.action_play);
        }
        MenuItem {
            id: moveToTopPlaylistMenu

            enabled: playlistItemsView.currentIndex < 0 ? false : playlistItemsView.currentIndex
            icon.source: "qrc:///icons/move_to_top.ico"
            icon.height: 24
            icon.width: 24
            text: qsTr("Move To Top")
            font.pointSize: fontSize

            onTriggered: itemClicked(playlistItemsView.currentIndex, globalConstants.action_move_to_top);
        }
        MenuItem {
            id: removePlaylistMenu

            icon.source: "qrc:///icons/remove.ico"
            icon.height: 24
            icon.width: 24
            text: qsTr("Remove")
            font.pointSize: fontSize

            onTriggered: itemClicked(playlistItemsView.currentIndex, globalConstants.action_remove);
        }
        MenuItem {
            id: ampacheLinkMenu

            icon.source: "qrc:///icons/search.ico"
            icon.height: 24
            icon.width: 24
            text: qsTr("Find on Ampache")
            font.pointSize: fontSize

            onTriggered: Qt.openUrlExternally(playlistItems.get(playlistItemsView.currentIndex).ampacheURL);
            enabled: (playlistItemsView.currentIndex >= 0) && (playlistItems.get(playlistItemsView.currentIndex).ampacheURL.length > 0);
        }
        MenuSeparator { }
        MenuItem {
            id: selectGroupPlaylistMenu

            enabled: playlistItemsView.currentIndex < 0 ? false : playlistItems.get(playlistItemsView.currentIndex).group.length && !playlistItems.get(playlistItemsView.currentIndex).busy
            icon.source: "qrc:///icons/check_checked.ico"
            icon.height: 24
            icon.width: 24
            text: qsTr("Select Group")
            font.pointSize: fontSize

            onTriggered: itemClicked(playlistItemsView.currentIndex, globalConstants.action_select_group);
        }
        MenuItem {
            id: selectAllPlaylistMenu

            enabled: playlistItemsView.currentIndex < 0 ? false : !playlistItems.get(playlistItemsView.currentIndex).busy
            icon.source: "qrc:///icons/check_checked.ico"
            icon.height: 24
            icon.width: 24
            text: qsTr("Select All")
            font.pointSize: fontSize

            onTriggered: itemClicked(playlistItemsView.currentIndex, globalConstants.action_select_all);
        }
        MenuItem {
            id: deselectAllPlaylistMenu

            enabled: playlistItemsView.currentIndex < 0 ? false : !playlistItems.get(playlistItemsView.currentIndex).busy
            icon.source: "qrc:///icons/check_unchecked.ico"
            icon.height: 24
            icon.width: 24
            text: qsTr("Deselect All")
            font.pointSize: fontSize

            onTriggered: itemClicked(playlistItemsView.currentIndex, globalConstants.action_deselect_all);
        }
    }


    ListView {
        id: playlistItemsView

        anchors.fill: parent

        clip: true
        highlight: Rectangle {
            color: "LightSteelBlue";
        }
        highlightMoveDuration: 500
        highlightMoveVelocity: 500
        delegate: playlistElement
        model: playlistItems

        ScrollBar.vertical: ScrollBar {
        }

        footer: playlistFooter
    }


    ListModel {
        id: playlistItems
    }

    Component {
        id: playlistFooter

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            height: {
                var h = playlistItemsView.height - playlistItems.count * (imageSize + (imageSize / 12));
                if (h < 0) {
                    h = imageSize + (imageSize / 12);
                }
                return h;
            }

            DropArea {
                anchors.fill: parent
                onDropped: {
                    if (!drop.keys.length) {
                        return;
                    }

                    if ((drop.keys[0] === "Explorer") && (drop.keys.length > 1)) {
                        playlistItemsView.currentIndex = playlistItems.count - 1;
                        explorerItemDragDroped(drop.keys[1], playlistItems.count);
                    }
                }
            }
        }
    }

    Component {
        id: playlistElement

        MouseArea {
            id: playlistElementMouseArea

            property bool held: false

            anchors.left: parent ? parent.left : playlistElement.left;
            anchors.right: parent ? parent.right : playlistElement.right;
            height: playlistElementItem.height

            acceptedButtons: Qt.LeftButton | Qt.RightButton

            drag.axis: Drag.YAxis
            drag.target: held ? playlistElementItem : undefined

            onClicked: {
                var prevIndex = playlistItemsView.currentIndex;
                playlistItemsView.currentIndex = index;

                if (mouse.button == Qt.LeftButton) {
                    if ((mouse.x >= selectedImage.x) && (mouse.x <= selectedImage.x + selectedImage.width)) {
                        if (mouse.modifiers == Qt.ShiftModifier) {
                            var clickedSelected = playlistItems.get(index).selected;
                            var increment = index > prevIndex ? -1 : 1;

                            var i = index;
                            while ((i >= 0) && (i < playlistItems.count) && (i !== (prevIndex + increment)) && (playlistItems.get(i).selected === clickedSelected)) {
                                itemClicked(i, globalConstants.action_select);
                                i += increment;
                            }
                        }
                        else {
                            itemClicked(index, globalConstants.action_select);
                        }
                    }
                }
                else if (mouse.button == Qt.RightButton) {
                    playlistMenu.x = mouse.x
                    playlistMenu.y = playlistItemsView.currentItem.y
                    playlistMenu.open(busyIndicator);
                }
            }
            onDoubleClicked: {
                playlistItemsView.currentIndex = index;
                itemClicked(index, globalConstants.action_play);
            }
            onPressAndHold: {
                playlistItemsView.currentIndex = index;
                internal.dropSourceIndex = index;
                internal.dropTargetIndex = index;
                held = true;
            }
            onReleased: {
                if (held) {
                    itemDragDropped(internal.dropSourceIndex, internal.dropTargetIndex);
                    held = false;
                }
            }

            DropArea {
                anchors.fill: parent
                z: 99
                onEntered: {
                    if (drag.keys.length) {
                        return;
                    }

                    var tmp = index;
                    playlistItems.move(internal.dropTargetIndex, index, 1);
                    internal.dropTargetIndex = tmp;
                    playlistItemsView.positionViewAtIndex(tmp > index ? tmp + 1 : tmp - 1, ListView.Visible);
                }
                onDropped: {
                    if (!drop.keys.length) {
                        // re-ordering playlist item is handled in onReleased event of MouseArea
                        return;
                    }

                    if ((drop.keys[0] === "Explorer") && (drop.keys.length > 1)) {
                        playlistItemsView.currentIndex = index;
                        explorerItemDragDroped(drop.keys[1], index);
                    }
                }
            }

            Item {
                id: playlistElementItem

                height: imageSize + (imageSize / 12)
                width: parent.width

                states: State {
                    when: playlistElementMouseArea.held

                    ParentChange {
                        target: playlistElementItem;
                        parent: playlistRoot.parent;
                    }
                }

                Drag.active: playlistElementMouseArea.held
                Drag.hotSpot.x: width / 2
                Drag.hotSpot.y: height / 2
                Drag.source: playlistElementMouseArea


                Rectangle {
                    id: dragIndicator

                    anchors.fill: parent
                    color: ((Qt.platform.os === "windows") || (Qt.platform.os === "winrt")) ? Universal.accent : Material.accent;
                    visible: held
                }

                Image {
                    id: itemImage

                    anchors.left: parent.left
                    anchors.leftMargin: 5
                    anchors.verticalCenter: parent.verticalCenter

                    height: imageSize
                    width: (image === null) || (image.length === 0) ? 0 : imageSize

                    source: image
                }

                BusyIndicator {
                    id: busyIndicator

                    anchors.fill: itemImage

                    visible: busy
                }

                ProgressBar {
                    id: pcmProgress

                    anchors.left: itemImage.left
                    anchors.right: itemImage.right
                    anchors.bottom: itemImage.bottom
                    height: itemImage.height / 8

                    background: Rectangle {
                        width: downloadPercent * parent.availableWidth
                        height: parent.height
                        color: Qt.rgba(pcmProgress.palette.highlight.r, pcmProgress.palette.highlight.g, pcmProgress.palette.highlight.b, 0.5)
                        visible: pcmPercent >= 0
                    }
                    contentItem: Rectangle {
                        width: pcmProgress.visualPosition * parent.availableWidth
                        height: parent.height
                        color: pcmProgress.palette.highlight
                        visible: pcmProgress.visualPosition !== 0
                    }

                    from: 0
                    to: 1
                    value: pcmPercent < 0 ? 0 : pcmPercent
                    indeterminate: pcmPercent < 0
                }

                Image {
                    id: selectedImage

                    anchors.left: itemImage.right
                    anchors.leftMargin: 5
                    anchors.verticalCenter: parent.verticalCenter

                    height: imageSize - 4 < 18 ? imageSize - 4 : 18
                    width: imageSize - 4 < 18 ? imageSize - 4 : 18

                    source: selected ? "qrc:///icons/check_checked.ico" : "qrc:///icons/check_unchecked.ico"
                }

                Label {
                    id: titleLabel

                    anchors.left: selectedImage.right
                    anchors.leftMargin: 5
                    anchors.right: groupLabel.left
                    anchors.rightMargin: 5
                    anchors.verticalCenter: parent.verticalCenter

                    color: internal.getLabelColor(isError)
                    elide: "ElideMiddle"
                    font.pointSize: fontSize
                    text: "<b>" + title + "</b> " + artist
                }

                Label {
                    id: groupLabel

                    anchors.right: parent.right
                    anchors.rightMargin: 5
                    anchors.verticalCenter: parent.verticalCenter

                    font.pointSize: fontSize
                    text: "<i>" + group + "</i>"
                }

                ToolTip {
                    delay: 500
                    text: isError ? errorMessage : title
                    visible: hoverHandler.hovered && (isError || titleLabel.truncated)
                    y: hoverHandler.point.position.y + imageSize
                    x: hoverHandler.point.position.x
                }

                HoverHandler {
                    id: hoverHandler
                }
            }
        }
    }


    Rectangle {
        id: totalTimeBackground

        anchors.bottom: playlistItemsView.bottom
        anchors.horizontalCenter: playlistItemsView.horizontalCenter
        anchors.bottomMargin: 3
        width: totalTime.width + 4
        height: totalTime.height

        color: totalTime.palette.highlight
        radius: 3
    }

    Label {
        id: totalTime

        anchors.bottom: playlistItemsView.bottom
        anchors.horizontalCenter: playlistItemsView.horizontalCenter
        anchors.bottomMargin: 3

        color: totalTime.palette.highlightedText
        font.pointSize: fontSize * 0.8
        horizontalAlignment: Text.AlignHCenter
        text: ""
        wrapMode: Text.NoWrap
    }

    Rectangle {
        anchors.fill: parent
        border.color: isFocused ? focusBorderColor : borderColor
        color: "transparent"
        visible: borderVisible
    }

    BusyIndicator {
        id: playlistBigBusy
        anchors.fill: parent
        visible: false
    }
}
