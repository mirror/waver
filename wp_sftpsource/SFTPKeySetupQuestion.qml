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

    Label {
        id: question
        text: "Do you want to set up passwordless public key authentication for this server?"
        wrapMode: Text.WordWrap
        anchors.left: parent.left
        anchors.leftMargin: 8
        anchors.right: parent.right
        anchors.rightMargin: 8
        anchors.top: parent.top
        anchors.topMargin: 8
    }

    Label {
        id: host
        text: "<user@host>"
        anchors.left: parent.left
        anchors.leftMargin: 8
        anchors.right: parent.right
        anchors.rightMargin: 8
        anchors.top: question.bottom
        anchors.topMargin: 8
    }

    Label {
        id: explanation
        text: "Do not do this on a public computer! Answer yes only if this is your private computer."
        wrapMode: Text.WordWrap
        anchors.left: parent.left
        anchors.leftMargin: 8
        anchors.right: parent.right
        anchors.rightMargin: 8
        anchors.top: host.bottom
        anchors.topMargin: 24
    }

    Button {
        id: yesButton
        text: qsTr("Yes")
        font.pointSize: 10
        anchors.right: parent.right
        anchors.rightMargin: 8
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 8

        onClicked: {
            var retval = {
                button: "key_setup_yes",
                client: "<clientId>",
            }
            done(JSON.stringify(retval));
        }
    }

    Button {
        id: noButton
        text: qsTr("No")
        font.pointSize: 10
        anchors.left: parent.left
        anchors.leftMargin: 8
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 8

        onClicked: {
            var retval = {
                button: "key_setup_no",
                client: "<clientId>",
            }
            done(JSON.stringify(retval));
        }
    }
}
