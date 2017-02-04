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


import QtQuick 2.7;import QtQuick.Controls 2.0;import QtGraphicalEffects 1.0;import QtQuick.Dialogs 1.2;

Item {
    id: settings
    anchors.fill: parent

    signal done(variant results);
    signal apply(variant results);

    TextMetrics {
        id: textMetrics
        font.pointSize: 8
        text: "Át a gyárakon"
    }

    ListView {
        id: dirs
        focus: true
        anchors.left: parent.left
        anchors.leftMargin: 8
        anchors.right: addButton.left
        anchors.rightMargin: 8
        anchors.top: parent.top
        anchors.topMargin: 8
        anchors.bottom: doneButton.top
        anchors.bottomMargin: 8
        clip: true;

        ScrollBar.vertical: ScrollBar { }

        delegate:
            Text {
                text: dirpath
                font.pointSize: 8
                elide: Text.ElideLeft
                height: textMetrics.height + (6 * 2)
                width: dirs.width - (dirs.anchors.leftMargin + dirs.anchors.rightMargin)
                padding: 6

                MouseArea {
                    anchors.fill: parent
                    onClicked: dirs.currentIndex = index
                }
            }

        model:
            ListModel { id: dirsModel }

        highlight: Rectangle {
            color: "#DDDDDD"
            border.color: "#666666"
            radius: 3
        }
    }


    FileDialog {
        id: fileDialog
        selectExisting: true
        selectFolder: true
        selectMultiple: false
        onAccepted: {
            var path = fileDialog.folder.toString();
            path = path.replace("file://", "");

            var already = false;
            for(var i = 0; i < dirsModel.count; i++) {
                if (dirsModel.get(i).dirpath === path) {
                    already = true;
                }
            }

            if (!already) {
                dirsModel.append({
                    dirpath: path
                });
            }

            fileDialog.close();
        }
    }


    Button {
        id: addButton
        text: qsTr("Add")
        font.pointSize: 10
        anchors.right: parent.right
        anchors.rightMargin: 8
        anchors.top: parent.top
        anchors.topMargin: 8
        onClicked: {
            fileDialog.open();
        }
    }

    Button {
        id: removeButton
        text: qsTr("Remove")
        font.pointSize: 10
        anchors.right: parent.right
        anchors.rightMargin: 8
        anchors.top: addButton.bottom
        anchors.topMargin: 8
        onClicked: {
            if (dirsModel.count > 1) {
                dirsModel.remove(dirs.currentIndex);
            }
        }
    }

    Button {
        id: doneButton
        text: qsTr("OK")
        font.pointSize: 10
        anchors.right: parent.right
        anchors.rightMargin: 8
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 8
        onClicked: {
            var retval = [];
            for(var i = 0; i < dirsModel.count; i++) {
                retval.push(dirsModel.get(i).dirpath);
            }
            done(JSON.stringify(retval));
        }
    }

    Button {
        id: cancelButton
        text: qsTr("Cancel")
        font.pointSize: 10
        anchors.left: parent.left
        anchors.leftMargin: 8
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 8
        onClicked: done(0);
    }

}
