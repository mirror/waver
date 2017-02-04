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

    FileDialog {
        id: importDialog
        selectExisting: true
        selectFolder: false
        selectMultiple: false
        onAccepted: {
            var fileUrl = importDialog.fileUrl.toString();
            importDialog.close();
            done(JSON.stringify({ command: "import_scsv", file: fileUrl }));
        }
    }

    Button {
        id: importRhythmboxButton
        text: qsTr("Import from Rhythmbox")
        font.pointSize: 10
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.topMargin: 8
        onClicked: done(JSON.stringify({command: "import_rhythmbox"}));
    }

    Button {
        id: importCsvButton
        text: qsTr("Import from SCSV file*")
        font.pointSize: 10
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: importRhythmboxButton.bottom
        anchors.topMargin: 8
        onClicked: importDialog.open()
    }

    Label {
        anchors.right: parent.right
        anchors.rightMargin: 6
        anchors.left: parent.left
        anchors.leftMargin: 6
        anchors.bottom: cancelButton.top
        anchors.bottomMargin: 6
        font.pointSize: 8
        text: "*SemiColon Separated Values file format: category;name;url"
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
