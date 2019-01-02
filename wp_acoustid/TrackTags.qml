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


import QtQuick 2.7;import QtQuick.Controls 2.0;import QtQml 2.2;

Item {
    id: settings
    anchors.fill: parent

    signal done(variant results);
    signal apply(variant results);

    property int timeout: 17;

    Label {
        id: currentData
        text: "<currentPerformer>, <currentAlbum>, <currentTitle>, <currentYear>, <currentTrack>"
        wrapMode: Text.WordWrap
        font.pointSize: newPerformerLabel.font.pointSize * .75
        anchors.top: parent.top
        anchors.topMargin: 6
        anchors.left: parent.left
        anchors.leftMargin: 6
        anchors.right: parent.right
        anchors.rightMargin: 6
    }

    Label {
        id: newPerformerLabel
        text: "Performed by:"
        anchors.top: currentData.bottom
        anchors.topMargin: 18
        anchors.left: parent.left
        anchors.leftMargin: 6
        anchors.right: parent.right
        anchors.rightMargin: 6
    }

    TextField {
        id: newPerformer
        text: "<newPerformer>"
        anchors.top: newPerformerLabel.bottom
        anchors.topMargin: 6
        anchors.left: parent.left
        anchors.leftMargin: 6
        anchors.right: parent.right
        anchors.rightMargin: 6
        onTextEdited: {
            counter.running = false;
            timeout = -1;
            countdown.text = "";
        }
    }

    Label {
        id: newAlbumLabel
        text: "Album:"
        anchors.top: newPerformer.bottom
        anchors.topMargin: 6
        anchors.left: parent.left
        anchors.leftMargin: 6
        anchors.right: parent.right
        anchors.rightMargin: 6
    }

    TextField {
        id: newAlbum
        text: "<newAlbum>"
        anchors.top: newAlbumLabel.bottom
        anchors.topMargin: 6
        anchors.left: parent.left
        anchors.leftMargin: 6
        anchors.right: parent.right
        anchors.rightMargin: 6
        onTextEdited: {
            counter.running = false;
            timeout = -1;
            countdown.text = "";
        }
    }

    Label {
        id: newTitleLabel
        text: "Title:"
        anchors.top: newAlbum.bottom
        anchors.topMargin: 6
        anchors.left: parent.left
        anchors.leftMargin: 6
        anchors.right: parent.right
        anchors.rightMargin: 6
    }

    TextField {
        id: newTitle
        text: "<newTitle>"
        anchors.top: newTitleLabel.bottom
        anchors.topMargin: 6
        anchors.left: parent.left
        anchors.leftMargin: 6
        anchors.right: parent.right
        anchors.rightMargin: 6
        onTextEdited: {
            counter.running = false;
            timeout = -1;
            countdown.text = "";
        }
    }

    Label {
        id: newYearLabel
        text: "Year:"
        anchors.top: newTitle.bottom
        anchors.topMargin: 6
        anchors.left: parent.left
        anchors.leftMargin: 6
        anchors.right: parent.horizontalCenter
        anchors.rightMargin: 3
    }

    Label {
        id: newTrackLabel
        text: "Track:"
        anchors.top: newTitle.bottom
        anchors.topMargin: 6
        anchors.left: parent.horizontalCenter
        anchors.leftMargin: 3
        anchors.right: parent.right
        anchors.rightMargin: 6
    }

    TextField {
        id: newYear
        text: "<newYear>"
        anchors.top: newYearLabel.bottom
        anchors.topMargin: 6
        anchors.left: parent.left
        anchors.leftMargin: 6
        anchors.right: parent.horizontalCenter
        anchors.rightMargin: 3
        onTextEdited: {
            counter.running = false;
            timeout = -1;
            countdown.text = "";
        }
    }

    TextField {
        id: newTrack
        text: "<newTrack>"
        anchors.top: newTrackLabel.bottom
        anchors.topMargin: 6
        anchors.left: parent.horizontalCenter
        anchors.leftMargin: 3
        anchors.right: parent.right
        anchors.rightMargin: 6
        onTextEdited: {
            counter.running = false;
            timeout = -1;
            countdown.text = "";
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
            counter.running = false;
            timeout = -1;
            done(JSON.stringify({
                url: "<url>",
                action: "track_tags",
                performer: newPerformer.text,
                album: newAlbum.text,
                title: newTitle.text,
                year: newYear.text,
                track: newTrack.text,
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
        onClicked: {
            counter.running = false;
            timeout = -1;
            done(0);
        }
    }

    Label {
        id: countdown
        anchors.bottom: doneButton.top
        anchors.bottomMargin: 6
        anchors.right: parent.right
        anchors.rightMargin: 6
        anchors.left: parent.left
        anchors.leftMargin: 6
    }

    Timer {
        id: counter
        interval: 1000
        repeat: true
        running: true

        onTriggered: {
            if (timeout == 0) {
                counter.running = false
                timeout = -1;
                done(JSON.stringify({
                    url: "<url>",
                    action: "track_tags",
                    performer: newPerformer.text,
                    album: newAlbum.text,
                    title: newTitle.text,
                    year: newYear.text,
                    track: newTrack.text,
                }));
                return;
            }

            if (timeout > 0) {
                timeout = timeout - 1;
                countdown.text = "Track will be updated automatically in " + timeout + " seconds";
            }
        }
    }
}
