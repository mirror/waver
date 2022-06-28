import QtQml 2.3
import QtQuick 2.12
import QtQuick.Controls 2.3
import QtQuick.Layouts 1.3

Dialog {
    title: qsTr("Quick Start Guide")
    modal: true
    focus: true
    standardButtons: Dialog.Ok

    function setIsSnap(isSnap)
    {
        internal.isSnap = isSnap
    }

    QtObject {
        id: internal

        readonly property string snap_command_text: 'CONNS=$(snap connections amp-waver | tail -n +2 | tr -s \' \'); echo "$CONNS" | while read -r conn; do if test $(echo "$conn" | cut -d \' \' -f 3) = "-"; then interface=$(echo "$conn" | cut -d \' \' -f 2 | sed s/^.*:/:/); echo "$interface"; sudo snap connect "amp-waver$interface" "$interface"; fi; done;';

        property bool isSnap: false;

        function showSnap()
        {
            return (Qt.platform.os !== "windows") && (Qt.platform.os !== "winrt") && isSnap;
        }
    }

    TextEdit {
        id: copier
        text: internal.snap_command_text
        visible: false
    }

    Flickable {
        x: 10
        y: 10
        width: parent.width - 20
        height: parent.height - 20
        clip: true
        contentHeight: quickStartColumn.height

        ScrollBar.vertical: ScrollBar { }

        Column {
            id: quickStartColumn
            width: parent.width

            Label {
                id: header
                bottomPadding: 31
                onLinkActivated: Qt.openUrlExternally(link);
                text: qsTr("Waver is an independently developed audio client for the <a href=\"https://ampache.org/\">Ampache</a> media server. It is NOT a streaming service. To use Waver, it must be connected to your Ampache instance.")
                width: parent.width
                wrapMode: Text.WrapAtWordBoundaryOrAnywhere

                MouseArea {
                    anchors.fill: parent
                    acceptedButtons: Qt.NoButton
                    cursorShape: parent.hoveredLink ? Qt.PointingHandCursor : Qt.ArrowCursor
                }
            }

            Label {
                bottomPadding: 17
                font.bold: true
                font.italic: true
                font.pixelSize: header.font.pixelSize * .85
                text: qsTr("Permissions (Linux only)")
                visible: internal.showSnap()
                width: parent.width
                wrapMode: Text.WrapAtWordBoundaryOrAnywhere
            }
            Label {
                bottomPadding: 17
                font.pixelSize: header.font.pixelSize * .85
                onLinkActivated: Qt.openUrlExternally(link);
                text: qsTr("It's a good idea to check Waver's permissions right after installation. Waver's performance may suffer and some of its functions may be out of order if all permissions aren't enabled. Some distributions offer GUI tools to check application permissions (for example, in recent versions of Ubuntu, go to Settings -> Applications). However, the easiest way is to use the terminal. You can learn about Snap Store's command line interface on the <a href=\"https://snapcraft.io/docs/interface-management\">Snapcraft documentation</a>, and also on the <a href=\"https://ubuntu.com/blog/a-guide-to-snap-permissions-and-interfaces\">Ubuntu blog</a>.")
                width: parent.width
                wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                visible: internal.showSnap()

                MouseArea {
                    enabled: internal.showSnap()
                    anchors.fill: parent
                    acceptedButtons: Qt.NoButton
                    cursorShape: parent.hoveredLink ? Qt.PointingHandCursor : Qt.ArrowCursor
                }
            }
            Label {
                bottomPadding: 17
                font.pixelSize: header.font.pixelSize * .85
                text: qsTr("Or, you can just run the following command to enable all permissions in a sinlge step:")
                width: parent.width
                wrapMode: Text.WordWrap
                visible: internal.showSnap()
            }
            Label {
                id: snapCommand
                bottomPadding: 17
                font.family: "monospace"
                font.pixelSize: header.font.pixelSize * .85
                text: internal.snap_command_text
                visible: internal.showSnap()
                width: parent.width
                wrapMode: Text.WrapAtWordBoundaryOrAnywhere

                MouseArea {
                    enabled: internal.showSnap()
                    anchors.fill: parent
                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        copier.selectAll();
                        copier.copy();
                        snapCommandCopied.visible = true;
                    }
                }
            }
            ToolTip {
                id: snapCommandCopied
                delay: 0
                timeout: 2500
                text: qsTr("Copied to clipboard")
                visible: false
                y: snapCommand.y + 5
                x: snapCommand.x + 5
            }
            Label {
                bottomPadding: 31
                font.pixelSize: header.font.pixelSize * .85
                onLinkActivated: Qt.openUrlExternally(link);
                text: qsTr("Note that permissions do not apply to <a href=\"https://code.launchpad.net/~waver-developers/+archive/ubuntu/waver-testing\">beta testing versions</a>.")
                width: parent.width
                wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                visible: internal.showSnap()

                MouseArea {
                    enabled: internal.showSnap()
                    anchors.fill: parent
                    acceptedButtons: Qt.NoButton
                    cursorShape: parent.hoveredLink ? Qt.PointingHandCursor : Qt.ArrowCursor
                }
            }

            Label {
                bottomPadding: 17
                font.bold: true
                font.italic: true
                font.pixelSize: header.font.pixelSize * .85
                text: qsTr("Connecting to your Ampache server")
                width: parent.width
                wrapMode: Text.WrapAtWordBoundaryOrAnywhere
            }
            Label {
                bottomPadding: 17
                font.pixelSize: header.font.pixelSize * .85
                text: qsTr("Click Servers in the menu. Enter the server's URL, your user name, and your password. Click Add. Waver attempts to connect to the server immediately.")
                width: parent.width
                wrapMode: Text.WrapAtWordBoundaryOrAnywhere
            }
            Label {
                bottomPadding: 17
                font.pixelSize: header.font.pixelSize * .85
                text: qsTr("Waver supports connecting to multiple servers simultaneously.")
                width: parent.width
                wrapMode: Text.WrapAtWordBoundaryOrAnywhere
            }
            Label {
                bottomPadding: 31
                font.pixelSize: header.font.pixelSize * .85
                text: qsTr("Passwords are stored securely. If secure storage fails, you will be prompted to enter your password when Waver starts. Secure storage can fail for example if application permissions are not enabled.")
                width: parent.width
                wrapMode: Text.WrapAtWordBoundaryOrAnywhere
            }

            Label {
                bottomPadding: 17
                font.bold: true
                font.italic: true
                font.pixelSize: header.font.pixelSize * .85
                text: qsTr("Exploring and playing content")
                width: parent.width
                wrapMode: Text.WrapAtWordBoundaryOrAnywhere
            }
            Label {
                bottomPadding: 17
                font.pixelSize: header.font.pixelSize * .85
                text: qsTr("Use the tree view on the left side to explore and play content. Double-click on an artist, an album, a track, a playlist, a smart playlist, or (if your Ampache server supports it) a radio station, to play it immediately. Right-click to access more options like Play Next and Enqueue.")
                width: parent.width
                wrapMode: Text.WrapAtWordBoundaryOrAnywhere
            }
            Label {
                bottomPadding: 17
                font.pixelSize: header.font.pixelSize * .85
                text: qsTr("Playing an artist does NOT play each and every track of the artist. Instead, it asks the server to generate a random list. The number of tracks in the random list can be set on the Shuffle page of Options.")
                width: parent.width
                wrapMode: Text.WrapAtWordBoundaryOrAnywhere
            }
            Label {
                bottomPadding: 31
                font.pixelSize: header.font.pixelSize * .85
                text: qsTr("When you first click on some of the nodes in the tree view, for example Browse or Playlist, Waver downloads and caches a list of these items. Depending on the number of items and the speed of your connection, this might take anywhere from a few seconds to several minutes. The next time you click the same item, the cached list will be used. To download the list of items again, for example if you add new artists or create new playlists on your server, right-click the node and select Refresh from the pop-up menu.")
                width: parent.width
                wrapMode: Text.WrapAtWordBoundaryOrAnywhere
            }

            Label {
                bottomPadding: 17
                font.bold: true
                font.italic: true
                font.pixelSize: header.font.pixelSize * .85
                text: qsTr("Playlist")
                width: parent.width
                wrapMode: Text.WrapAtWordBoundaryOrAnywhere
            }
            Label {
                bottomPadding: 31
                font.pixelSize: header.font.pixelSize * .85
                text: qsTr("Tracks that are enqueued to play appear in the playlist. Double-click a track in the playlist to play it immediately, right-click to access more options. Clicking the small circle in front of the track's title will select/deselect the track, which then allows the Move To Top and the Remove options to act on the selected tracks collectively. Tracks in the playlist can also be reordered using drag-and-drop operations.")
                width: parent.width
                wrapMode: Text.WrapAtWordBoundaryOrAnywhere
            }

            Label {
                bottomPadding: 17
                font.bold: true
                font.italic: true
                font.pixelSize: header.font.pixelSize * .85
                text: qsTr("Shuffle")
                width: parent.width
                wrapMode: Text.WrapAtWordBoundaryOrAnywhere
            }
            Label {
                bottomPadding: 17
                font.pixelSize: header.font.pixelSize * .85
                text: qsTr("When there are no more tracks in the playlist, Waver automatically starts Shuffle (this can be turned off in Options). To refine which tracks to be included in Shuffle, you can checkmark tags under Shuffle in the tree view. Use right-click to select/deselect a tag. Selecting no tags has the same effect as selecting all tags.")
                width: parent.width
                wrapMode: Text.WrapAtWordBoundaryOrAnywhere
            }
            Label {
                bottomPadding: 17
                font.pixelSize: header.font.pixelSize * .85
                text: qsTr("Waver specifically includes your favorite tracks in Shuffle. In Options, you can choose between Rare, Normal, or Frequent modes. To mark a track as favorite, click the star on the toolbar while the track plays.")
                width: parent.width
                wrapMode: Text.WrapAtWordBoundaryOrAnywhere
            }
            Label {
                bottomPadding: 31
                font.pixelSize: header.font.pixelSize * .85
                text: qsTr("There are also two special types of shuffle in the tree view, Favorites and Never Played. These must be started manually by double-clicking them.")
                width: parent.width
                wrapMode: Text.WrapAtWordBoundaryOrAnywhere
            }

            Label {
                bottomPadding: 17
                font.bold: true
                font.italic: true
                font.pixelSize: header.font.pixelSize * .85
                text: qsTr("Keyboard Shortcuts")
                width: parent.width
                wrapMode: Text.WrapAtWordBoundaryOrAnywhere
            }
            Label {
                bottomPadding: 31
                font.pixelSize: header.font.pixelSize * .85
                text: qsTr("Pause/Play: Space\nSeek: Left/Right Arrow\nPrevious/Next: Page Down/Up\nSwitch Focus: Tab\nMove Selection: Down/Up Arrow\nActions: Enter")
                width: parent.width
                wrapMode: Text.WrapAtWordBoundaryOrAnywhere
            }
        }
    }
}
