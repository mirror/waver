/*
    This file is part of Waver

    Copyright (C) 2018 Peter Papp <peter.papp.p@gmail.com>

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

    Label {
        id: pswLabel
        text: "Enter password"
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
        anchors.top: pswLabel.bottom
        anchors.topMargin: 8
    }

    TextField {
        id: psw
        echoMode: TextInput.Password
        passwordMaskDelay: 500
        anchors.left: parent.left
        anchors.leftMargin: 8
        anchors.right: parent.right
        anchors.rightMargin: 8
        anchors.top: hostLabel.bottom
        anchors.topMargin: 8
    }

    Label {
        id: fingerprint
        text: "<fingerprint_display>"
        wrapMode: Text.WordWrap
        anchors.left: parent.left
        anchors.leftMargin: 8
        anchors.right: parent.right
        anchors.rightMargin: 8
        anchors.top: psw.bottom
        anchors.topMargin: 8
    }

    Label {
        id: explanation
        text: "<explanation>"
        wrapMode: Text.WordWrap
        anchors.left: parent.left
        anchors.leftMargin: 8
        anchors.right: parent.right
        anchors.rightMargin: 8
        anchors.top: fingerprint.bottom
        anchors.topMargin: 24
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
                button: "psw",
                client: "<clientId>",
                fingerprint: "<fingerprint>",
                psw: psw.text,
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

        onClicked: {
            var retval = {
                button: "psw_cancel",
                client: "<clientId>",
            }
            done(JSON.stringify(retval));
        }
    }
}
