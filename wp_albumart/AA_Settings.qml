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


import QtQuick 2.7;
import QtQuick.Controls 2.0;

Item {
    id: settings
    anchors.fill: parent

    signal done(variant results);
    signal apply(variant results);

    CheckBox {
        id: check_always
        text: "Check even if other pictures already exist"
        tristate: false
        checked: check_always_value
        anchors.top: parent.top
        anchors.topMargin: 6
        anchors.left: parent.left
        anchors.leftMargin: 6
    }

    CheckBox {
        id: allow_loose_match
        text: "Allow loose matches"
        tristate: false
        checked: allow_loose_match_value
        anchors.top: check_always.bottom
        anchors.topMargin: 6
        anchors.left: parent.left
        anchors.leftMargin: 6
    }
    CheckBox {
        id: only_if_no_other_exist
        text: "Only if no other pictures exist already"
        tristate: false
        checked: only_if_no_other_exist_value
        anchors.top: allow_loose_match.bottom
        anchors.topMargin: 1
        anchors.left: parent.left
        anchors.leftMargin: 16
        enabled: (allow_loose_match.checkState == Qt.Checked)
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
            done(JSON.stringify({
                check_always:           (check_always.checkState           == Qt.Checked) ? true : false,
                allow_loose_match:      (allow_loose_match.checkState      == Qt.Checked) ? true : false,
                only_if_no_other_exist: (only_if_no_other_exist.checkState == Qt.Checked) ? true : false
            }));
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
