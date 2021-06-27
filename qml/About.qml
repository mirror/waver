import QtQml 2.3
import QtQuick 2.12
import QtQuick.Controls 2.3
import QtQuick.Layouts 1.3

Dialog {
    title: qsTr("Waver")
    modal: true
    focus: true
    standardButtons: Dialog.Ok

    QtObject {
        id: internal

        function isBeta()
        {
            var versionParts = Qt.application.version.split(".");
            return (versionParts[versionParts.length - 1] % 2 == 0);
        }

        function currentYear()
        {
            var date = new Date();
            return date.getFullYear();
        }
    }

    Flickable {
        x: 10
        y: 10
        width: parent.width - 20
        height: parent.height - 20
        clip: true
        contentHeight: aboutColumn.height

        ScrollBar.vertical: ScrollBar { }

        Column {
            id: aboutColumn
            width: parent.width

            Label {
                bottomPadding: 31
                font.italic: true
                text: "Open Source Ampache Client"
                width: parent.width
                wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                horizontalAlignment: Text.AlignHCenter
            }
            Row {
                width: parent.width
                bottomPadding: 13

                Label {
                    id: ver
                    text: qsTr("Version ") + Qt.application.version + " - " + (internal.isBeta() ? qsTr("Beta testing") : qsTr("Release"));
                    width: parent.width / 2 - 1
                    wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                }
                Label {
                    text: "Copyright 2017-" + internal.currentYear() + " Peter Papp"
                    width: parent.width / 2
                    wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                }
            }
            Row {
                width: parent.width
                bottomPadding: 31

                Label {
                    onLinkActivated: Qt.openUrlExternally(link);
                    text: "<a href=\"https://launchpad.net/waver\">launchpad.net/waver</a>"
                    width: parent.width / 2 - 1
                    wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                }
                Label {
                    onLinkActivated: Qt.openUrlExternally(link);
                    text: "Built on the <a href=\"https://www.qt.io/\">Qt framework</a>."
                    width: parent.width / 2
                    wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                }
            }
            Label {
                bottomPadding: 31
                font.bold: true
                text: qsTr("Waver does not collect or send personal information of any kind.")
                width: parent.width
                wrapMode: Text.WrapAtWordBoundaryOrAnywhere
            }
            Label {
                bottomPadding: 31
                text: qsTr("This is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.\n\nThis software is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.")
                width: parent.width
                wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                font.pixelSize: ver.font.pixelSize * .85
            }

            Label {
                bottomPadding: 17
                font.bold: true
                font.pixelSize: ver.font.pixelSize * .85
                text: qsTr("Licenses")
                width: parent.width
                wrapMode: Text.WrapAtWordBoundaryOrAnywhere
            }

            Label {
                bottomPadding: 17
                font.pixelSize: ver.font.pixelSize * .85
                onLinkActivated: Qt.openUrlExternally(link);
                text: "<a href=\"https://launchpad.net/waver\">Waver</a> is <a href=\"https://bazaar.launchpad.net/~waver-developers/waver/trunk/files\">open source</a> software under the <a href=\"https://www.gnu.org/licenses/gpl-3.0.en.html\">GPL v3</a> license."
                width: parent.width
                wrapMode: Text.WrapAtWordBoundaryOrAnywhere
            }
            Label {
                bottomPadding: 31
                font.pixelSize: ver.font.pixelSize * .85
                onLinkActivated: Qt.openUrlExternally(link);
                text: "The <a href=\"https://launchpad.net/waveriir\">Waver IIR</a> library is <a href=\"https://bazaar.launchpad.net/~waver-developers/waveriir/trunk/files\">open source</a> software under the <a href=\"https://www.gnu.org/licenses/gpl-3.0.en.html\">GPL v3</a> license."
                width: parent.width
                wrapMode: Text.WrapAtWordBoundaryOrAnywhere
            }

            Label {
                bottomPadding: 17
                font.bold: true
                font.pixelSize: ver.font.pixelSize * .85
                text: qsTr("Third Party Licenses")
                width: parent.width
                wrapMode: Text.WrapAtWordBoundaryOrAnywhere
            }

            Label {
                bottomPadding: 17
                font.pixelSize: ver.font.pixelSize * .85
                onLinkActivated: Qt.openUrlExternally(link);
                text: "<a href=\"https://www.qt.io/\">Qt</a> is dual-licensed under commercial and <a href=\"https://github.com/qt\">open source</a> licenses. Here, the <a href=\"https://www.gnu.org/licenses/gpl-3.0.en.html\">GPL v3</a> licensed version is being used, unmodified, by ways of dynamic linking."
                width: parent.width
                wrapMode: Text.WrapAtWordBoundaryOrAnywhere
            }
            Label {
                bottomPadding: 17
                font.pixelSize: ver.font.pixelSize * .85
                onLinkActivated: Qt.openUrlExternally(link);
                text: "<a href=\"https://taglib.org\">TagLib</a> is <a href=\"https://github.com/taglib/taglib\">open source</a> software under the <a href=\"https://www.gnu.org/licenses/lgpl-3.0.html\">LGPL v3</a> license. Waver uses it unmodified, by ways of dynamic linking."
                width: parent.width
                wrapMode: Text.WrapAtWordBoundaryOrAnywhere
            }
            Label {
                bottomPadding: 31
                font.pixelSize: ver.font.pixelSize * .85
                onLinkActivated: Qt.openUrlExternally(link);
                text: "<a href=\"https://github.com/frankosterfeld/qtkeychain\">QtKeychain</a> is <a href=\"https://github.com/frankosterfeld/qtkeychain\">open source</a> software that uses a <a href=\"https://github.com/frankosterfeld/qtkeychain/blob/master/COPYING\">custom</a> license. Waver uses it unmodified, by ways of dynamic linking."
                width: parent.width
                wrapMode: Text.WrapAtWordBoundaryOrAnywhere
            }

            Label {
                bottomPadding: 17
                font.bold: true
                font.pixelSize: ver.font.pixelSize * .85
                text: qsTr("Miscellaneous")
                width: parent.width
                wrapMode: Text.WrapAtWordBoundaryOrAnywhere
            }

            Label {
                font.pixelSize: ver.font.pixelSize * .85
                onLinkActivated: Qt.openUrlExternally(link);
                text: "Waver contains an implementation of <a href=\"https://wiki.hydrogenaud.io/index.php?title=ReplayGain_specification\">ReplayGain</a>."
                width: parent.width
                wrapMode: Text.WrapAtWordBoundaryOrAnywhere
            }
        }
    }
}
