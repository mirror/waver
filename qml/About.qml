import QtQml 2.3
import QtQuick 2.12
import QtQuick.Controls 2.3
import QtQuick.Layouts 1.3

Dialog {
    title: qsTr("Waver")
    modal: true
    focus: true
    standardButtons: Dialog.Ok

    Flickable {
        anchors.fill: parent
        clip: true
        contentHeight: aboutColumn.height

        ScrollBar.vertical: ScrollBar { }

        Column {
            id: aboutColumn
            width: parent.width

            Label {
                bottomPadding: 13
                font.italic: true
                text: "Open Source Ampache Client"
                width: parent.width
                wrapMode: Text.WrapAtWordBoundaryOrAnywhere
            }
            Label {
                id: ver
                bottomPadding: 13
                text: qsTr("Version ") + Qt.application.version
                width: parent.width
                wrapMode: Text.WrapAtWordBoundaryOrAnywhere
            }
            Label {
                bottomPadding: 13
                text: "Copyright 2017-2021 Peter Papp"
                width: parent.width
                wrapMode: Text.WrapAtWordBoundaryOrAnywhere
            }
            Label {
                bottomPadding: 13
                onLinkActivated: Qt.openUrlExternally(link);
                text: "<a href=\"https://launchpad.net/waver\">launchpad.net/waver</a>"
                width: parent.width
                wrapMode: Text.WrapAtWordBoundaryOrAnywhere
            }
            Label {
                bottomPadding: 31
                font.bold: true
                text: qsTr("Waver does not collect or send personal information of any kind.")
                width: parent.width
                wrapMode: Text.WrapAtWordBoundaryOrAnywhere
            }
            Label {
                bottomPadding: 31
                onLinkActivated: Qt.openUrlExternally(link);
                text: "Built on the <a href=\"https://www.qt.io/\">Qt framework</a>."
                width: parent.width
                wrapMode: Text.WrapAtWordBoundaryOrAnywhere
            }
            Label {
                text: qsTr("This is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.\n\nThis software is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.\n\nYou should have received a copy of the GNU General Public License (GPL.TXT) along with this software. If not, see http://www.gnu.org/licenses/")
                width: parent.width
                wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                font.pixelSize: ver.font.pixelSize * .85
            }
        }
    }
}
