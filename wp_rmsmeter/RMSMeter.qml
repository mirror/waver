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


import QtQuick 2.7;import QtQuick.Controls 2.0;import QtQuick.Window 2.2;import WaverRMSMeter 1.0;

Window {
    id: rmsMeter;
    width: 450;
    height: 60;
    title: "Waver RMS & Peak meter"

    Rectangle {
        width: parent.height - 12
        height: parent.width - 12
        anchors.centerIn: parent
        rotation: 90
        gradient: Gradient {
            GradientStop { position: 1.0;  color: "darkgreen" }
            GradientStop { position: 0.33; color: "green" }
            GradientStop { position: 0.2;  color: "yellow" }
            GradientStop { position: 0;    color: "red" }
        }
    }

    Rectangle {
        width: parent.width - 2
        height: 6
        anchors.centerIn: parent
    }

    RMSVisual {
        x: 6
        y: 6
        width: parent.width - 12
        height: parent.height / 2 - 9
        channel: 0
    }

    RMSVisual {
        x: 6
        y: parent.height / 2 + 3
        width: parent.width - 12
        height: parent.height / 2 - 9
        channel: 1
    }
}
