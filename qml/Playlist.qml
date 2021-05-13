import QtQml 2.3
import QtQuick 2.12
import QtQuick.Controls 2.3
import QtQuick.Controls.Material 2.3
import QtQuick.Controls.Universal 2.3



Item {
    id: playlistRoot

    property bool  borderVisible: true
    property color borderColor: "#666666"
    property int   imageSize: 36

    signal itemClicked(int index, int action);
    signal itemDragDropped(int index, int destinationIndex)

    function addItem(title, artist, group, image, selected)
    {
        if ((typeof selected === 'undefined') || (selected === null)) {
            selected = false;
        }

        var newDict = {
            title: title,
            artist: artist,
            group: group,
            image: image,
            selected: selected,
            busy: false,
            isError: false,
            memoryUsage: "",
            errorMessage: "",
        }

        playlistItems.append(newDict);

        if (internal.currentIndex < playlistItems.count) {
            playlistItemsView.currentIndex = internal.currentIndex;
        }
    }

    function clearItems()
    {
        internal.currentIndex = playlistItemsView.currentIndex;
        playlistItems.clear();
    }

    function setBufferData(index, memoryUsageText)
    {
        playlistItems.setProperty(index, "memoryUsage", memoryUsageText);
    }

    function setBusy(index, busy)
    {
        playlistItems.setProperty(index, "busy", busy);
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

    QtObject {
        id: internal

        property int currentIndex   : 0
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

    Label {
        id: originalFontSize
        height: 0
        visible: false
        width: 0
    }


    Menu {
        id: playlistMenu

        MenuItem {
            id: playPlaylistMenu

            icon.source: "qrc:///icons/play.ico"
            icon.height: 24
            icon.width: 24
            text: qsTr("Play Now")

            onTriggered: itemClicked(playlistItemsView.currentIndex, globalConstants.action_play);
        }
        MenuSeparator { }
        MenuItem {
            id: moveToTopPlaylistMenu

            enabled: playlistItemsView.currentIndex < 0 ? false : playlistItemsView.currentIndex
            icon.source: "qrc:///icons/move_to_top.ico"
            icon.height: 24
            icon.width: 24
            text: qsTr("Move To Top")

            onTriggered: itemClicked(playlistItemsView.currentIndex, globalConstants.action_move_to_top);
        }
        MenuItem {
            id: removePlaylistMenu

            icon.source: "qrc:///icons/remove.ico"
            icon.height: 24
            icon.width: 24
            text: qsTr("Remove")

            onTriggered: itemClicked(playlistItemsView.currentIndex, globalConstants.action_remove);
        }
        MenuSeparator { }
        MenuItem {
            id: selectGroupPlaylistMenu

            enabled: playlistItemsView.currentIndex < 0 ? false : playlistItems.get(playlistItemsView.currentIndex).group.length && !playlistItems.get(playlistItemsView.currentIndex).busy
            icon.source: "qrc:///icons/check_checked.ico"
            icon.height: 24
            icon.width: 24
            text: qsTr("Select Group")

            onTriggered: itemClicked(playlistItemsView.currentIndex, globalConstants.action_select_group);
        }
        MenuItem {
            id: selectAllPlaylistMenu

            enabled: playlistItemsView.currentIndex < 0 ? false : !playlistItems.get(playlistItemsView.currentIndex).busy
            icon.source: "qrc:///icons/check_checked.ico"
            icon.height: 24
            icon.width: 24
            text: qsTr("Select All")

            onTriggered: itemClicked(playlistItemsView.currentIndex, globalConstants.action_select_all);
        }
        MenuItem {
            id: deselectAllPlaylistMenu

            enabled: playlistItemsView.currentIndex < 0 ? false : !playlistItems.get(playlistItemsView.currentIndex).busy
            icon.source: "qrc:///icons/check_unchecked.ico"
            icon.height: 24
            icon.width: 24
            text: qsTr("Deselect All")

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
        focus: true
        keyNavigationEnabled: true
        model: playlistItems

        ScrollBar.vertical: ScrollBar {
        }
    }


    ListModel {
        id: playlistItems
    }

    Component {
        id: playlistElement

        MouseArea {
            id: playlistElementMouseArea

            property bool held: false

            anchors.left: parent.left
            anchors.right: parent.right
            height: playlistElementItem.height

            acceptedButtons: Qt.LeftButton | Qt.RightButton

            drag.axis: Drag.YAxis
            drag.target: held ? playlistElementItem : undefined

            onClicked: {
                playlistItemsView.currentIndex = index;

                if (mouse.button == Qt.LeftButton) {
                    if ((mouse.x >= selectedImage.x) && (mouse.x <= selectedImage.x + selectedImage.width)) {
                        itemClicked(index, globalConstants.action_select);
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
                onEntered: {
                    if (internal.dropTempIndex !== index) {
                        var tmp = index;
                        playlistItems.move(internal.dropTargetIndex, index, 1);
                        internal.dropTargetIndex = tmp;
                        playlistItemsView.positionViewAtIndex(tmp > index ? tmp + 1 : tmp - 1, ListView.Visible);
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

                    width: visible ? imageSize : 0
                    height: imageSize

                    visible: busy
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
                    font.pixelSize: imageSize <= 36 ? originalFontSize.font.pixelSize : originalFontSize.font.pixelSize * 1.25
                    text: "<b>" + title + "</b> " + artist
                }

                Label {
                    id: groupLabel

                    anchors.right: memoryUsageLabel.left
                    anchors.rightMargin: 5
                    anchors.verticalCenter: parent.verticalCenter

                    font.pixelSize: imageSize <= 36 ? originalFontSize.font.pixelSize * 0.75 : originalFontSize.font.pixelSize
                    text: "<i>" + group + "</i>"
                }

                Label {
                    id: memoryUsageLabel

                    anchors.right: parent.right
                    anchors.rightMargin: 5
                    anchors.verticalCenter: parent.verticalCenter

                    font.pixelSize: imageSize <= 36 ? originalFontSize.font.pixelSize * 0.75 : originalFontSize.font.pixelSize
                    text: memoryUsage
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
        font.pixelSize: textMetrics.font.pixelSize * 0.8
        horizontalAlignment: Text.AlignHCenter
        text: ""
        wrapMode: Text.NoWrap
    }

    Rectangle {
        anchors.fill: parent
        border.color: borderColor
        color: "transparent"
        visible: borderVisible
    }
}
