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


import QtQuick 2.7;import QtQuick.Controls 2.0;

Item {
    id: settings
    anchors.fill: parent

    signal done(variant results);
    signal apply(variant results);

    Row {
        id :pre_amp_row
        anchors.right: parent.right
        anchors.rightMargin: 6
        anchors.left: parent.left
        anchors.leftMargin: 6
        anchors.top: parent.top
        anchors.topMargin: 6

        Label {
            text: "pre-Amp"
            anchors.verticalCenter: pre_amp.verticalCenter
        }
        Slider {
            id: pre_amp
            value: replace_pre_amp_value
            from: -12
            to: 6
        }
    }

    Flickable {
        anchors.right: parent.right
        anchors.left: parent.left
        anchors.top: pre_amp_row.bottom
        anchors.topMargin: 12
        anchors.bottom: doneButton.top
        anchors.bottomMargin: 6
        contentHeight: ((gain_31.height + 6) * 10) + (reset_button.height + 24)
        clip: true

        ScrollBar.vertical: ScrollBar { }

        Row {
            id: gain_31_row
            anchors.right: parent.right
            anchors.rightMargin: 6
            anchors.left: parent.left
            anchors.leftMargin: 6
            anchors.top: parent.top
            anchors.topMargin: 6

            Label {
                text: "31 Hz"
                width: 50
                anchors.verticalCenter: gain_31.verticalCenter
            }
            Slider {
                id: gain_31
                value: replace_gain_31_value
                from: -12
                to: 12
            }
        }

        Row {
            id: gain_62_row
            anchors.right: parent.right
            anchors.rightMargin: 6
            anchors.left: parent.left
            anchors.leftMargin: 6
            anchors.top: gain_31_row.bottom
            anchors.topMargin: 6

            Label {
                text: "62 Hz"
                width: 50
                anchors.verticalCenter: gain_62.verticalCenter
            }
            Slider {
                id: gain_62
                value: replace_gain_62_value
                from: -12
                to: 12
            }
        }

        Row {
            id :gain_125_row
            anchors.right: parent.right
            anchors.rightMargin: 6
            anchors.left: parent.left
            anchors.leftMargin: 6
            anchors.top: gain_62_row.bottom
            anchors.topMargin: 6

            Label {
                text: "125 Hz"
                width: 50
                anchors.verticalCenter: gain_125.verticalCenter
            }
            Slider {
                id: gain_125
                value: replace_gain_125_value
                from: -12
                to: 12
            }
        }

        Row {
            id: gain_250_row
            anchors.right: parent.right
            anchors.rightMargin: 6
            anchors.left: parent.left
            anchors.leftMargin: 6
            anchors.top: gain_125_row.bottom
            anchors.topMargin: 6

            Label {
                text: "250 Hz"
                width: 50
                anchors.verticalCenter: gain_250.verticalCenter
            }
            Slider {
                id: gain_250
                value: replace_gain_250_value
                from: -12
                to: 12
            }
        }

        Row {
            id: gain_500_row
            anchors.right: parent.right
            anchors.rightMargin: 6
            anchors.left: parent.left
            anchors.leftMargin: 6
            anchors.top: gain_250_row.bottom
            anchors.topMargin: 6

            Label {
                text: "500 Hz"
                width: 50
                anchors.verticalCenter: gain_500.verticalCenter
            }
            Slider {
                id: gain_500
                value: replace_gain_500_value
                from: -12
                to: 12
            }
        }

        Row {
            id: gain_1000_row
            anchors.right: parent.right
            anchors.rightMargin: 6
            anchors.left: parent.left
            anchors.leftMargin: 6
            anchors.top: gain_500_row.bottom
            anchors.topMargin: 6

            Label {
                text: "1 kHz"
                width: 50
                anchors.verticalCenter: gain_1000.verticalCenter
            }
            Slider {
                id: gain_1000
                value: replace_gain_1000_value
                from: -12
                to: 12
            }
        }

        Row {
            id: gain_2500_row
            anchors.right: parent.right
            anchors.rightMargin: 6
            anchors.left: parent.left
            anchors.leftMargin: 6
            anchors.top: gain_1000_row.bottom
            anchors.topMargin: 6

            Label {
                text: "2.5 kHz"
                width: 50
                anchors.verticalCenter: gain_2500.verticalCenter
            }
            Slider {
                id: gain_2500
                value: replace_gain_2500_value
                from: -12
                to: 12
            }
        }

        Row {
            id: gain_5000_row
            anchors.right: parent.right
            anchors.rightMargin: 6
            anchors.left: parent.left
            anchors.leftMargin: 6
            anchors.top: gain_2500_row.bottom
            anchors.topMargin: 6

            Label {
                text: "5 kHz"
                width: 50
                anchors.verticalCenter: gain_5000.verticalCenter
            }
            Slider {
                id: gain_5000
                value: replace_gain_5000_value
                from: -12
                to: 12
            }
        }

        Row {
            id: gain_10000_row
            anchors.right: parent.right
            anchors.rightMargin: 6
            anchors.left: parent.left
            anchors.leftMargin: 6
            anchors.top: gain_5000_row.bottom
            anchors.topMargin: 6

            Label {
                text: "10 kHz"
                width: 50
                anchors.verticalCenter: gain_10000.verticalCenter
            }
            Slider {
                id: gain_10000
                value: replace_gain_10000_value
                from: -12
                to: 12
            }
        }

        Row {
            id: gain_16000_row
            anchors.right: parent.right
            anchors.rightMargin: 6
            anchors.left: parent.left
            anchors.leftMargin: 6
            anchors.top: gain_10000_row.bottom
            anchors.topMargin: 6

            Label {
                text: "16 kHz"
                width: 50
                anchors.verticalCenter: gain_16000.verticalCenter
            }
            Slider {
                id: gain_16000
                value: replace_gain_16000_value
                from: -12
                to: 12
            }
        }

        Button {
            id: reset_button
            text: qsTr("Reset")
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: gain_16000_row.bottom
            anchors.topMargin: 12

            onClicked: {
                pre_amp.value    = 3;
                gain_31.value    = 6;
                gain_62.value    = 3;
                gain_125.value   = 1.5;
                gain_250.value   = 0;
                gain_500.value   = -1.5;
                gain_1000.value  = 0;
                gain_2500.value  = 3;
                gain_5000.value  = 6;
                gain_10000.value = 9;
                gain_16000.value = 12;
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
            var retval = {
                pre_amp: pre_amp.value,
                gain_31: gain_31.value,
                gain_62: gain_62.value,
                gain_125: gain_125.value,
                gain_250: gain_250.value,
                gain_500: gain_500.value,
                gain_1000: gain_1000.value,
                gain_2500: gain_2500.value,
                gain_5000: gain_5000.value,
                gain_10000: gain_10000.value,
                gain_16000: gain_16000.value
            };

            done(JSON.stringify(retval));
        }
    }

    Button {
        id: applyButton
        text: qsTr("Apply")
        font.pointSize: 10
        anchors.right: doneButton.left
        anchors.rightMargin: 8
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 8
        onClicked: {
            var retval = {
                pre_amp: pre_amp.value,
                gain_31: gain_31.value,
                gain_62: gain_62.value,
                gain_125: gain_125.value,
                gain_250: gain_250.value,
                gain_500: gain_500.value,
                gain_1000: gain_1000.value,
                gain_2500: gain_2500.value,
                gain_5000: gain_5000.value,
                gain_10000: gain_10000.value,
                gain_16000: gain_16000.value
            };

            apply(JSON.stringify(retval));
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
