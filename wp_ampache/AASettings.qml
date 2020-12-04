/*
    This file is part of Waver

    Copyright (C) 2017-2019 Peter Papp <peter.papp.p@gmail.com>

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
        id: urlLabel
        text: 'URL (includig protocol)'
        anchors.left: parent.left
        anchors.leftMargin: 8
        anchors.verticalCenter: url.verticalCenter
    }

    TextField {
        id: url
        text: "<https://server.com>"
        anchors.left: urlLabel.right
        anchors.leftMargin: 8
        anchors.right: parent.right
        anchors.rightMargin: 8
        anchors.top: parent.top
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
        text: "<username>"
        anchors.left: userLabel.right
        anchors.leftMargin: 8
        anchors.right: parent.right
        anchors.rightMargin: 8
        anchors.top: url.bottom
        anchors.topMargin: 8
    }

    Label {
        id: pswLabel
        text: 'Password'
        anchors.left: parent.left
        anchors.leftMargin: 8
        anchors.verticalCenter: psw.verticalCenter
    }

    TextField {
        id: psw
        text: "<password>"
        echoMode: TextInput.PasswordEchoOnEdit
        anchors.left: pswLabel.right
        anchors.leftMargin: 8
        anchors.right: parent.right
        anchors.rightMargin: 8
        anchors.top: user.bottom
        anchors.topMargin: 8
    }

    Label {
        id: explanation
        text: "Do not do this on a public computer! Your password will be stored in plain text format. Do this only if this is your private computer."
        wrapMode: Text.WordWrap
        anchors.left: parent.left
        anchors.leftMargin: 8
        anchors.right: parent.right
        anchors.rightMargin: 8
        anchors.top: psw.bottom
        anchors.topMargin: 24
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
        anchors.right: parent.right
        anchors.rightMargin: 8
        anchors.top: explanation.bottom
        anchors.topMargin: 24
        model: [ "Low", "Medium", "High", "Random" ]
        currentIndex: 9999
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
                url: url.text,
                user: user.text,
                psw: psw.text,
                variation: variation.currentText
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
            done(0);
        }
    }
}
