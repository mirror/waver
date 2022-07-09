import QtQml 2.3
import QtQuick 2.12
import QtQuick.Controls 2.3
import QtQuick.Controls.Material 2.3
import QtQuick.Controls.Universal 2.3
import QtQuick.Layouts 1.3


Item {
    id: explorerRoot

    property bool  borderVisible: true
    property bool  isFocused: false
    property color borderColor: "#666666"
    property color focusBorderColor: ((Qt.platform.os === "windows") || (Qt.platform.os === "winrt")) ? Universal.accent : Material.accent;
    property int   imageSize: 24

    signal itemClicked(string id, int action, var extra);

    function addItem(id, parent, title, image, extra, expandable, playable, selectable, selected)
    {
        if ((typeof expandable === 'undefined') || (expandable === null)) {
            expandable = false;
        }
        if ((typeof playable === 'undefined') || (playable === null)) {
            playable = true;
        }
        if ((typeof selectable === 'undefined') || (selectable === null)) {
            selectable = false;
        }
        if ((typeof selected === 'undefined') || (selected === null)) {
            selected = false;
        }

        var newDict = {
            id: id,
            parent: parent,
            level: 0,
            title: title,
            image: image,
            extra: extra,
            expandable: expandable,
            playable: playable,
            queueable: true,
            selectable: selectable,
            selected: selected,
            isError: false,
            errorMessage: "",
            busy: false
        }

        if (parent) {
            var parentIndex = internal.findItem(parent);
            if (parentIndex >= 0) {
                newDict.level = explorerItems.get(parentIndex).level + 1;
                explorerItems.insert(internal.afterLastChild(parentIndex), newDict);
                return;
            }
        }

        explorerItems.append(newDict);
    }


    function disableQueueable(id)
    {
        var index = internal.findItem(id);
        if (index >= 0) {
            explorerItems.get(index).queueable = false;
        }
    }


    function getExtra(id)
    {
        var index = internal.findItem(id);
        if (index < 0) {
            return;
        }

        return explorerItems.get(index).extra;
    }


    function getServersForDialog()
    {
        var servers = [];

        for (var i = 0; i < explorerItems.count; i++) {
            if (explorerItems.get(i).id.startsWith("D") ) {
                servers.push({ id: explorerItems.get(i).id, title: explorerItems.get(i).title });
            }
        }

        return servers;
    }


    function moveSelectionDown()
    {
        if ((explorerItems.count >= 1) && (explorerItemsView.currentIndex < explorerItems.count - 1)) {
            explorerItemsView.currentIndex++;
        }
    }


    function moveSelectionUp()
    {
        if ((explorerItems.count >= 1) && (explorerItemsView.currentIndex > 0)) {
            explorerItemsView.currentIndex--;
        }
    }


    function removeAboveLevel(id)
    {
        var index = internal.findItem(id);
        if (index >= 0) {
            var level = explorerItems.get(index).level;

            var i = 0;
            while (i < explorerItems.count) {
                if (explorerItems.get(i).level > level) {
                    explorerItems.remove(i);
                }
                else {
                    i++;
                }
            }
        }
    }


    function removeChildren(id)
    {
        var i = 0;
        while (i < explorerItems.count) {
            if (explorerItems.get(i).parent === id) {
                removeChildren(explorerItems.get(i).id);
                explorerItems.remove(i);
            }
            else {
                i++;
            }
        }
    }


    function removeItem(id)
    {
        var index = internal.findItem(id);
        if (index >= 0) {
            removeChildren(id);
            explorerItems.remove(index);
        }
    }


    function setBusy(id, busy)
    {
        var index = internal.findItem(id);
        if (index < 0) {
            return;
        }

        explorerItems.setProperty(index, "busy", busy);
    }


    function setError(id, isError, errorMessage)
    {
        var index = internal.findItem(id);
        if (index < 0) {
            return;
        }

        explorerItems.setProperty(index, "isError", isError);
        explorerItems.setProperty(index, "errorMessage", errorMessage);
    }


    function setFlagExtra(id, flag)
    {
        var index = internal.findItem(id);
        if (index < 0) {
            return;
        }

        var extra = explorerItems.get(index).extra;
        extra.flag = flag ? 1 : 0;
        explorerItems.get(index).extra = extra;
    }


    function setSelected(id, selected)
    {
        var index = internal.findItem(id);
        if (index < 0) {
            return;
        }

        explorerItems.setProperty(index, "selected", selected);
    }


    function simulateRightClick()
    {
        if (explorerItemsView.currentItem != null) {
            explorerMenu.x = 20
            explorerMenu.y = explorerItemsView.currentItem.y
            explorerMenu.open();
        }
    }


    function toggleSelected(id)
    {
        var index = internal.findItem(id);
        if (index < 0) {
            return;
        }

        explorerItems.setProperty(index, "selected", !explorerItems.get(index).selected);
    }


    QtObject {
        id: internal

        function afterLastChild(index)
        {
            var parent = explorerItems.get(index).id;

            var i = index + 1;
            while (i < explorerItems.count) {
                if (explorerItems.get(i).parent !== parent) {
                    return i;
                }
                i++;
            }
            return i;
        }

        function findItem(id)
        {
            for (var i = 0; i < explorerItems.count; i++) {
                if (explorerItems.get(i).id === id) {
                    return i;
                }
            }
            return -1;
        }

        function getLabelColor(isError)
        {
            if ((Qt.platform.os === "windows") || (Qt.platform.os === "winrt")) {
                return isError ? Universal.accent : Universal.foreground;
            }
            return isError ? Material.accent : Material.foreground;
        }

        function hasChildren(id) {
            if (typeof id === 'undefined') {
                return false;
            }

            for (var i = 0; i < explorerItems.count; i++) {
                if (explorerItems.get(i).id === id) {
                    continue;
                }
                if (explorerItems.get(i).parent === id) {
                    return true;
                }
            }
            return false;
        }

        function isExpandEnabled()
        {
            if (explorerItems.count <= 0) {
                return false;
            }
            if (explorerItemsView.currentIndex < 0) {
                return false;
            }
            if (explorerItems.get(explorerItemsView.currentIndex) && explorerItems.get(explorerItemsView.currentIndex).busy) {
                return false;
            }
            if (explorerItems.get(explorerItemsView.currentIndex) && !explorerItems.get(explorerItemsView.currentIndex).expandable) {
                return false;
            }
            if (explorerItems.get(explorerItemsView.currentIndex) && hasChildren(explorerItems.get(explorerItemsView.currentIndex).id)) {
                return false;
            }
            return true;
        }

        function isRefreshEnabled()
        {
            if (explorerItems.count <= 0) {
                return false;
            }
            if (explorerItemsView.currentIndex < 0) {
                return false;
            }
            if (explorerItems.get(explorerItemsView.currentIndex) && explorerItems.get(explorerItemsView.currentIndex).busy) {
                return false;
            }
            if (explorerItems.get(explorerItemsView.currentIndex) && !explorerItems.get(explorerItemsView.currentIndex).expandable) {
                return false;
            }
            if (explorerItems.get(explorerItemsView.currentIndex) && !hasChildren(explorerItems.get(explorerItemsView.currentIndex).id)) {
                return false;
            }
            return true;
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
        id: explorerMenu

        MenuItem {
            id: expandExplorerMenu

            enabled: internal.isExpandEnabled()
            icon.source: "qrc:///icons/expand.ico"
            icon.height: 24
            icon.width: 24
            text: qsTr("Expand")

            onTriggered: itemClicked(explorerItems.get(explorerItemsView.currentIndex).id, globalConstants.action_expand, explorerItems.get(explorerItemsView.currentIndex).extra);
        }
        MenuItem {
            id: refreshExplorerMenu

            enabled: internal.isRefreshEnabled()
            icon.source: "qrc:///icons/refresh.ico"
            icon.height: 24
            icon.width: 24
            text: qsTr("Refresh")

            onTriggered: itemClicked(explorerItems.get(explorerItemsView.currentIndex).id, globalConstants.action_refresh, explorerItems.get(explorerItemsView.currentIndex).extra);
        }
        MenuSeparator { }
        MenuItem {
            id: playExplorerMenu

            enabled: explorerItemsView.currentIndex < 0 ? false : !explorerItems.get(explorerItemsView.currentIndex).busy && explorerItems.get(explorerItemsView.currentIndex).playable
            icon.source: "qrc:///icons/play.ico"
            icon.height: 24
            icon.width: 24
            text: qsTr("Play")

            onTriggered: itemClicked(explorerItems.get(explorerItemsView.currentIndex).id, globalConstants.action_play, explorerItems.get(explorerItemsView.currentIndex).extra);
        }
        MenuItem {
            id: enqueueTopExplorerMenu

            enabled: explorerItemsView.currentIndex < 0 ? false : !explorerItems.get(explorerItemsView.currentIndex).busy && explorerItems.get(explorerItemsView.currentIndex).playable && explorerItems.get(explorerItemsView.currentIndex).queueable
            icon.source: "qrc:///icons/enqueue_top.ico"
            icon.height: 24
            icon.width: 24
            text: qsTr("Play Next")

            onTriggered: itemClicked(explorerItems.get(explorerItemsView.currentIndex).id, globalConstants.action_playnext, explorerItems.get(explorerItemsView.currentIndex).extra);
        }
        MenuItem {
            id: enqueueExplorerMenu

            enabled: explorerItemsView.currentIndex < 0 ? false : !explorerItems.get(explorerItemsView.currentIndex).busy && explorerItems.get(explorerItemsView.currentIndex).playable && explorerItems.get(explorerItemsView.currentIndex).queueable
            icon.source: "qrc:///icons/enqueue.ico"
            icon.height: 24
            icon.width: 24

            onTriggered: itemClicked(explorerItems.get(explorerItemsView.currentIndex).id, globalConstants.action_enqueue, explorerItems.get(explorerItemsView.currentIndex).extra);
            text: qsTr("Enqueue")
        }
        MenuItem {
            id: enqueueShuffledExplorerMenu

            enabled: explorerItemsView.currentIndex < 0 ? false : !explorerItems.get(explorerItemsView.currentIndex).busy && explorerItems.get(explorerItemsView.currentIndex).playable && explorerItems.get(explorerItemsView.currentIndex).queueable
            icon.source: "qrc:///icons/enqueue_shuffled.ico"
            icon.height: 24
            icon.width: 24

            onTriggered: itemClicked(explorerItems.get(explorerItemsView.currentIndex).id, globalConstants.action_enqueueshuffled, explorerItems.get(explorerItemsView.currentIndex).extra);
            text: qsTr("Randomize")
        }
        MenuSeparator { }
        MenuItem {
            id: selectExplorerMenu

            checkable: explorerItemsView.currentIndex < 0 ? false : explorerItems.get(explorerItemsView.currentIndex).selectable
            checked: explorerItemsView.currentIndex < 0 ? true : explorerItems.get(explorerItemsView.currentIndex).selected
            enabled: explorerItemsView.currentIndex < 0 ? false : !explorerItems.get(explorerItemsView.currentIndex).busy && explorerItems.get(explorerItemsView.currentIndex).selectable
            text: explorerItemsView.currentIndex < 0 ? qsTr("Select") : explorerItems.get(explorerItemsView.currentIndex).selected ? qsTr("Deselect") : qsTr("Select")

            onTriggered: itemClicked(explorerItems.get(explorerItemsView.currentIndex).id, globalConstants.action_select, explorerItems.get(explorerItemsView.currentIndex).extra);
        }
    }

    ListView {
        id: explorerItemsView

        anchors.fill: parent

        clip: true
        highlight: Rectangle {
            color: "LightSteelBlue";
        }
        highlightFollowsCurrentItem: true
        highlightMoveDuration: 500
        highlightMoveVelocity: 500
        delegate: explorerElement
        model: explorerItems

        ScrollBar.vertical: ScrollBar {
        }
    }

    Rectangle {
        anchors.fill: parent
        border.color: isFocused ? focusBorderColor : borderColor
        color: "transparent"
        visible: borderVisible
    }


    ListModel {
        id: explorerItems
    }

    Component {
        id: explorerElement

        MouseArea {
            id: explorerElementMouseArea

            property bool held: false

            anchors.left: parent.left
            anchors.right: parent.right
            height: explorerElementItem.height

            acceptedButtons: Qt.LeftButton | Qt.RightButton

            drag.axis: Drag.XandYAxis
            drag.target: dragger.visible ? dragger : undefined

            onClicked: {
                explorerItemsView.currentIndex = index;

                if (mouse.button == Qt.LeftButton) {
                    if (expandable) {
                        if (internal.hasChildren(id)) {
                            itemClicked(id, globalConstants.action_collapse, extra);
                        }
                        else {
                            if (level >= 2) {
                                var toBeCollapsed = [];
                                for (var i = 0; i < explorerItems.count; i++) {
                                    if (explorerItems.get(i).level === level) {
                                        toBeCollapsed.push({ id: explorerItems.get(i).id, extra: explorerItems.get(i).extra });
                                    }
                                }
                                for (var j = 0; j < toBeCollapsed.length; j++) {
                                    itemClicked(toBeCollapsed[j].id, globalConstants.action_collapse, toBeCollapsed[j].extra);
                                }
                            }
                            itemClicked(id, globalConstants.action_expand, extra);
                        }
                    }
                    else if (id.startsWith("F") || id.startsWith("f") || id.startsWith("[") || id.startsWith("]") || id.startsWith("M")) {
                        itemClicked(id, globalConstants.action_noop, extra);
                    }
                }
                else if (mouse.button == Qt.RightButton) {
                    explorerMenu.x = mouse.x
                    explorerMenu.y = explorerItemsView.currentItem.y
                    explorerMenu.open(busyIndicator);
                }
            }

            onDoubleClicked: {
                explorerItemsView.currentIndex = index;

                if (playable) {
                    itemClicked(id, globalConstants.action_play, extra);
                }
            }

            onPressAndHold: {
                if (playable && queueable) {
                    explorerItemsView.interactive = false
                    explorerItemsView.currentIndex = index
                    var coordinates = mapToItem(explorerRoot.parent, 20, mouseY);
                    dragger.x = coordinates.x;
                    dragger.y = coordinates.y;
                    held = true;
                }
            }

            onReleased: {
                if (held) {
                    dragger.Drag.drop();
                    held = false;

                    explorerItemsView.interactive = true
                }
            }

            Item {
                id: explorerElementItem

                height: imageSize + (imageSize / 12)
                width: parent.width

                Rectangle {
                    id: dragger

                    x: 0
                    y: 0
                    height: imageSize + (imageSize / 12)
                    width: 360
                    color: ((Qt.platform.os === "windows") || (Qt.platform.os === "winrt")) ? Universal.accent : Material.accent;
                    visible: explorerElementMouseArea.held

                    Drag.active: visible
                    Drag.hotSpot.x: width / 2
                    Drag.hotSpot.y: height / 2
                    Drag.source: explorerElementMouseArea
                    Drag.keys: {
                        var explorerItem = explorerItems.get(index);
                        if (explorerItem) {
                            return [ "Explorer", explorerItem.id ]
                        }
                        return [];
                    }

                    states: State {
                        when: visible

                        ParentChange {
                            target: dragger;
                            parent: explorerRoot.parent;
                        }
                    }

                    Label {
                        anchors.centerIn: parent

                        elide: "ElideMiddle"
                        font.pixelSize: imageSize <= 16 ? originalFontSize.font.pixelSize * 0.8 : originalFontSize.font.pixelSize
                        text: title
                    }
                }

                Image {
                    id: itemImage

                    anchors.left: parent.left
                    anchors.leftMargin: level * (imageSize / 3) + 5
                    anchors.verticalCenter: parent.verticalCenter

                    height: imageSize
                    width: (image === null) || (image.length === 0) ? 0 : imageSize

                    source: image
                }

                BusyIndicator {
                    id: busyIndicator

                    anchors.left: itemImage.right
                    anchors.verticalCenter: parent.verticalCenter

                    width: visible ? imageSize : 0
                    height: imageSize

                    visible: busy
                }

                Label {
                    id: titleLabel

                    anchors.left: busyIndicator.right
                    anchors.leftMargin: 5
                    anchors.right: parent.right
                    anchors.rightMargin: 2
                    anchors.verticalCenter: parent.verticalCenter

                    color: internal.getLabelColor(isError);
                    elide: "ElideMiddle"
                    font.pixelSize: imageSize <= 16 ? originalFontSize.font.pixelSize * 0.8 : originalFontSize.font.pixelSize
                    text: (selectable && selected ? "\u2713 " : "") + title
                }

                ToolTip {
                    delay: 500
                    text: isError ? errorMessage : title
                    visible: hoverHandler.hovered && !explorerElementMouseArea.held && (isError || titleLabel.truncated)
                    y: hoverHandler.point.position.y + imageSize
                    x: hoverHandler.point.position.x
                }

                HoverHandler {
                    id: hoverHandler
                }
            }
        }
    }
}
