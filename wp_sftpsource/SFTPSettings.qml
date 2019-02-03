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
        id: clientsModel

        ListElement{}
    }

    TextMetrics {
        id: textMetrics
        font.pointSize: 8
        text: "Át a gyárakon"
    }

    Label {
        id: variationLabel
        text: "Variation"
        anchors.left: parent.left
        anchors.leftMargin: 8
        anchors.verticalCenter: variation.verticalCenter
    }

    ComboBox {
        id: variation
        anchors.left: variationLabel.right
        anchors.leftMargin: 8
        anchors.right: variationButton.left
        anchors.rightMargin: 8
        anchors.top: parent.top
        anchors.topMargin: 8
        model: [ "Low", "Medium", "High", "Random" ]
        currentIndex: 9999
    }

    Button {
        id: variationButton
        text: qsTr("Set")
        font.pointSize: 10
        anchors.right: parent.right
        anchors.rightMargin: 8
        anchors.verticalCenter: variation.verticalCenter

        onClicked: {
            var retval = {
                button: "variation",
                variation: variation.currentText,
            }
            done(JSON.stringify(retval));
        }
    }

    Item {
        id: addFrame
        anchors.left: parent.left
        anchors.leftMargin: 8
        anchors.right: parent.right
        anchors.rightMargin: 8
        anchors.top: variation.bottom
        anchors.topMargin: 8
        height: host.height + user.height + 8 * 3

        Rectangle {
            anchors.fill: addFrame
            border.color: "#666666"
            radius: 3
        }

        Label {
            id: hostLabel
            text: 'Host[:port]'
            anchors.left: parent.left
            anchors.leftMargin: 8
            anchors.verticalCenter: host.verticalCenter
        }

        TextField {
            id: host
            anchors.left: hostLabel.right
            anchors.leftMargin: 8
            anchors.right: addFrame.right
            anchors.rightMargin: 8
            anchors.top: addFrame.top
            anchors.topMargin: 8
        }

        Label {
            id: userLabel
            text: 'User'
            anchors.left: parent.left
            anchors.leftMargin: 8
            anchors.verticalCenter: user.verticalCenter
        }

        TextField {
            id: user
            anchors.left: hostLabel.right
            anchors.leftMargin: 8
            anchors.right: addButton.left
            anchors.rightMargin: 8
            anchors.top: host.bottom
            anchors.topMargin: 8
        }

        Button {
            id: addButton
            text: qsTr("Add")
            font.pointSize: 10
            anchors.right: addFrame.right
            anchors.rightMargin: 8
            anchors.top: user.top
            anchors.bottom: user.bottom

            onClicked: {
                var retval = {
                    button: "add",
                    host: host.text,
                    user: user.text,
                }
                done(JSON.stringify(retval));
            }
        }
    }

    ListView {
        id: clients
        clip: true
        focus: true
        anchors.left: parent.left
        anchors.leftMargin: 8
        anchors.right: cancelButton.left
        anchors.rightMargin: 8
        anchors.top: addFrame.bottom
        anchors.topMargin: 8
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 8

        ScrollBar.vertical: ScrollBar { }

        model: clientsModel

        delegate:
            Text {
                text: "%1 on %2".arg(dir).arg(formatted_user_host)
                padding: 6
                width: clients.width
                wrapMode: Text.WrapAtWordBoundaryOrAnywhere

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: clients.currentIndex = index
                }
            }

        highlight: Rectangle {
            color: "#DDDDDD"
            border.color: "#666666"
            radius: 3
        }
    }

    Button {
        id: removeButton
        text: qsTr("Remove")
        font.pointSize: 10
        anchors.right: parent.right
        anchors.rightMargin: 8
        anchors.top: addFrame.bottom
        anchors.topMargin: 8
        onClicked: {
            var retval = {
                button: "remove",
                client_id: (clientsModel.count > 0 ? clientsModel.get(clients.currentIndex).client_id : -1)
            }
            done(JSON.stringify(retval));
        }
    }

    Button {
        id: cancelButton
        text: qsTr("Cancel")
        font.pointSize: 10
        anchors.right: parent.right
        anchors.rightMargin: 8
        anchors.top: removeButton.bottom
        anchors.topMargin: 8
        onClicked: {
            var retval = {
                button: "cancel_all",
            }
            done(JSON.stringify(retval));
        }
    }
}
