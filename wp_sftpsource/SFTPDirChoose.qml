/*
    This file is part of Waver

    Copyright (C) 2018-2019 Peter Papp <peter.papp.p@gmail.com>

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


import QtQuick 2.7;import QtQuick.Controls 2.0;import QtGraphicalEffects 1.0;import QtQuick.Dialogs 1.2;

Item {
    id: settings
    anchors.fill: parent

    signal done(variant results);
    signal apply(variant results);

    ListModel {
        id: dirsModel

        ListElement{}
    }

    Label {
        id: labelLabel
        text: "Select directory"
        anchors.left: parent.left
        anchors.leftMargin: 8
        anchors.right: parent.right
        anchors.rightMargin: 8
        anchors.top: parent.top
        anchors.topMargin: 8
    }

    Label {
        id: hostLabel
        text: "<user@host>"
        anchors.left: parent.left
        anchors.leftMargin: 8
        anchors.right: parent.right
        anchors.rightMargin: 8
        anchors.top: labelLabel.bottom
        anchors.topMargin: 8
    }

    Label {
        id: currentDirLabel
        text: "<current_dir>"
        font.bold: true
        elide: Text.ElideLeft
        anchors.left: parent.left
        anchors.leftMargin: 8
        anchors.right: parent.right
        anchors.rightMargin: 8
        anchors.top: hostLabel.bottom
        anchors.topMargin: 8
    }

    ListView {
        id: dirsList
        clip: true
        focus: true
        anchors.left: parent.left
        anchors.leftMargin: 8
        anchors.right: parent.right
        anchors.rightMargin: 8
        anchors.top: currentDirLabel.bottom
        anchors.topMargin: 24
        anchors.bottom: doneButton.top
        anchors.bottomMargin: 8

        ScrollBar.vertical: ScrollBar { }

        model: dirsModel

        delegate:
            Text {
                text: name
                padding: 6

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: dirsList.currentIndex = index
                    onDoubleClicked: {
                        var retval = {
                            button: "dir_selector_open",
                            client: "<clientId>",
                            full_path: dirsModel.get(dirsList.currentIndex).full_path
                        }
                        done(JSON.stringify(retval));
                    }
                }
            }

        highlight: Rectangle {
            color: "#DDDDDD"
            border.color: "#666666"
            radius: 3
        }
    }

    Button {
        id: doneButton
        text: qsTr("Done")
        font.pointSize: 10
        anchors.right: parent.right
        anchors.rightMargin: 8
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 8

        onClicked: {
            var retval = {
                button: "dir_selector_done",
                client: "<clientId>",
                full_path: currentDirLabel.text
            }
            done(JSON.stringify(retval));
        }
    }
}
