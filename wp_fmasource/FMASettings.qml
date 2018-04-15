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

    Flickable {
        id: checkboxes
        anchors.right: parent.right
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.topMargin: 6
        anchors.bottom: allCheck.top
        anchors.bottomMargin: 6
        contentHeight: replace_content_height
        clip: true

        CheckBox {}

        ScrollBar.vertical: ScrollBar { }
    }

    Button {
        id: doneButton
        text: qsTr("OK")
        font.pointSize: 10
        anchors.right: parent.right
        anchors.rightMargin: 6
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 6
        onClicked: {
            var retval = {
                selected: [],
                denied: []
            };
            replace_checkboxes_to_retval;
            done(JSON.stringify(retval));
        }
    }

    Button {
        id: cancelButton
        text: qsTr("Cancel")
        font.pointSize: 10
        anchors.left: parent.left
        anchors.leftMargin: 6
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 6
        onClicked: done(0);
    }

    CheckBox {
        id: allCheck
        anchors.right: parent.right
        anchors.rightMargin: 6
        anchors.bottom: cancelButton.top
        anchors.bottomMargin: 6
        onClicked: { replace_checkboxes_to_all }
    }
}
