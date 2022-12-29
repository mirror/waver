//import QtQml 2.3
//import QtQuick 2.12
//import QtQuick.Controls 2.3
//import QtQuick.Layouts 1.3

import QtQml
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

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
            return (versionParts[1] % 2 == 0);
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
                    text: "Copyright (C) 2017-" + internal.currentYear() + " Peter Papp"
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

                    MouseArea {
                        anchors.fill: parent
                        acceptedButtons: Qt.NoButton
                        cursorShape: parent.hoveredLink ? Qt.PointingHandCursor : Qt.ArrowCursor
                    }
                }
                Label {
                    onLinkActivated: Qt.openUrlExternally(link);
                    text: "Built on the <a href=\"https://www.qt.io/\">Qt framework</a>"
                    width: parent.width / 2
                    wrapMode: Text.WrapAtWordBoundaryOrAnywhere

                    MouseArea {
                        anchors.fill: parent
                        acceptedButtons: Qt.NoButton
                        cursorShape: parent.hoveredLink ? Qt.PointingHandCursor : Qt.ArrowCursor
                    }
                }
            }

            Label {
                bottomPadding: 17
                font.bold: true
                font.italic: true
                font.pixelSize: ver.font.pixelSize * .85
                text: qsTr("Privacy")
                width: parent.width
                wrapMode: Text.WrapAtWordBoundaryOrAnywhere
            }
            Label {
                bottomPadding: 17
                font.bold: true
                font.pixelSize: ver.font.pixelSize * .85
                text: qsTr("Waver does not collect or send personal information of any kind.")
                width: parent.width
                wrapMode: Text.WrapAtWordBoundaryOrAnywhere
            }
            Label {
                bottomPadding: 31
                font.pixelSize: ver.font.pixelSize * .85
                onLinkActivated: Qt.openUrlExternally(link);
                text:  {
                    var privacyLabel = qsTr("However, if you installed Waver from an app store, usage statistics and/or error reports might be collected. Please refer to the following web pages for more information:<br>");
                    if ((Qt.platform.os === "windows") || (Qt.platform.os === "winrt")) {
                        privacyLabel += qsTr("<a href=\"https://privacy.microsoft.com/privacystatement\">Microsoft Privacy Statement</a><br><a href=\"https://privacy.microsoft.com\">Privacy at Microsoft</a>");
                    }
                    else {
                        privacyLabel += qsTr("<a href=\"https://ubuntu.com/legal/data-privacy/snap-store\">Privacy notice - Snaps and Snap store</a><br><a href=\"https://ubuntu.com/legal/data-privacy\">Canonical Data Privacy</a>");
                    }
                    return privacyLabel;
                }
                width: parent.width
                wrapMode: Text.WrapAtWordBoundaryOrAnywhere

                MouseArea {
                    anchors.fill: parent
                    acceptedButtons: Qt.NoButton
                    cursorShape: parent.hoveredLink ? Qt.PointingHandCursor : Qt.ArrowCursor
                }
            }

            Label {
                bottomPadding: 17
                font.bold: true
                font.italic: true
                font.pixelSize: ver.font.pixelSize * .85
                text: qsTr("Disclaimer")
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
                font.italic: true
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

                MouseArea {
                    anchors.fill: parent
                    acceptedButtons: Qt.NoButton
                    cursorShape: parent.hoveredLink ? Qt.PointingHandCursor : Qt.ArrowCursor
                }
            }
            Label {
                bottomPadding: (Qt.platform.os === "windows") || (Qt.platform.os === "winrt") ? 17 : 31
                font.pixelSize: ver.font.pixelSize * .85
                onLinkActivated: Qt.openUrlExternally(link);
                text: "The <a href=\"https://launchpad.net/waveriir\">Waver IIR</a> library is <a href=\"https://bazaar.launchpad.net/~waver-developers/waveriir/trunk/files\">open source</a> software under the <a href=\"https://www.gnu.org/licenses/gpl-3.0.en.html\">GPL v3</a> license."
                width: parent.width
                wrapMode: Text.WrapAtWordBoundaryOrAnywhere

                MouseArea {
                    anchors.fill: parent
                    acceptedButtons: Qt.NoButton
                    cursorShape: parent.hoveredLink ? Qt.PointingHandCursor : Qt.ArrowCursor
                }
            }
            Label {
                bottomPadding: 31
                font.pixelSize: ver.font.pixelSize * .85
                onLinkActivated: Qt.openUrlExternally(link);
                text: "If you installed Waver from the Windows Store, then its <a href=\"https://www.microsoft.com/store/standard-application-license-terms\">Standard Application License Terms</a> also apply."
                visible: (Qt.platform.os === "windows") || (Qt.platform.os === "winrt")
                width: parent.width
                wrapMode: Text.WrapAtWordBoundaryOrAnywhere

                MouseArea {
                    anchors.fill: parent
                    acceptedButtons: Qt.NoButton
                    cursorShape: parent.hoveredLink ? Qt.PointingHandCursor : Qt.ArrowCursor
                    enabled: (Qt.platform.os === "windows") || (Qt.platform.os === "winrt")
                }
            }

            Label {
                bottomPadding: 17
                font.bold: true
                font.italic: true
                font.pixelSize: ver.font.pixelSize * .85
                text: qsTr("Third Party Licenses")
                width: parent.width
                wrapMode: Text.WrapAtWordBoundaryOrAnywhere
            }
            Label {
                bottomPadding: 17
                font.pixelSize: ver.font.pixelSize * .85
                onLinkActivated: Qt.openUrlExternally(link);
                text: "<a href=\"https://www.qt.io/\">Qt</a> is dual-licensed under commercial and <a href=\"https://github.com/qt\">open source</a> licenses. Version 5.15.2 Copyright (C) 2015 The Qt Company Ltd. Here, the <a href=\"https://www.gnu.org/licenses/gpl-3.0.en.html\">GPL v3</a> licensed version is being used, unmodified, by ways of dynamic linking."
                width: parent.width
                wrapMode: Text.WrapAtWordBoundaryOrAnywhere

                MouseArea {
                    anchors.fill: parent
                    acceptedButtons: Qt.NoButton
                    cursorShape: parent.hoveredLink ? Qt.PointingHandCursor : Qt.ArrowCursor
                }
            }
            Label {
                bottomPadding: 17
                font.pixelSize: ver.font.pixelSize * .85
                onLinkActivated: Qt.openUrlExternally(link);
                text: "<a href=\"https://taglib.org\">TagLib</a> is <a href=\"https://github.com/taglib/taglib\">open source</a> software under the <a href=\"https://www.gnu.org/licenses/lgpl-3.0.html\">LGPL v3</a> license. Version 1.12 Copyright (C) 2002-2016 Rupert Daniel, Urs Fleisch, Michael Helmling, Allan Sandfeld Jensen, Tsuda Kageyu, Serkan Kalyoncu, Lukas Krejci, Lukáš Lalinský, Martin Nilsson, Alex Novichkov, Ismael Orenstein, Mathias Panzenböck, Anton Sergunov, Aaron VonderHaar, Scott Wheeler. Waver uses it unmodified, by ways of dynamic linking."
                width: parent.width
                wrapMode: Text.WrapAtWordBoundaryOrAnywhere

                MouseArea {
                    anchors.fill: parent
                    acceptedButtons: Qt.NoButton
                    cursorShape: parent.hoveredLink ? Qt.PointingHandCursor : Qt.ArrowCursor
                }
            }
            Label {
                bottomPadding: (Qt.platform.os === "windows") || (Qt.platform.os === "winrt") ? 17 : 31
                font.pixelSize: ver.font.pixelSize * .85
                onLinkActivated: Qt.openUrlExternally(link);
                text: "<a href=\"https://github.com/frankosterfeld/qtkeychain\">QtKeychain</a> is <a href=\"https://github.com/frankosterfeld/qtkeychain\">open source</a> software under the <a href=\"https://directory.fsf.org/wiki/License:BSD-3-Clause\">Modified BSD</a> license. Version 0.12.0 Copyright (C) 2011-2018 Frank Osterfeld, David Faure, Mathias Hasselmann, Stephen Kelly, Kitware Inc., Nikita Krupen'ko, Alex Merry, Alexander Neundorf, François Revol. Waver uses it unmodified, by ways of dynamic linking."
                width: parent.width
                wrapMode: Text.WrapAtWordBoundaryOrAnywhere

                MouseArea {
                    anchors.fill: parent
                    acceptedButtons: Qt.NoButton
                    cursorShape: parent.hoveredLink ? Qt.PointingHandCursor : Qt.ArrowCursor
                }
            }
            Label {
                bottomPadding: 31
                font.pixelSize: ver.font.pixelSize * .85
                onLinkActivated: Qt.openUrlExternally(link);
                text: "<a href=\"https://github.com/mohabouje/WinToast\">WinToast</a> is <a href=\"https://github.com/mohabouje/WinToast\">open source</a> software under the <a href=\"https://mit-license.org/\">MIT</a> license. Version 1.2.0 Copyright (C) 2016 Mohammed Boujemaoui Boulaghmoudi. Waver uses it unmodified, by including its entire source code (as recommended by the author)."
                width: parent.width
                wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                visible: (Qt.platform.os === "windows") || (Qt.platform.os === "winrt")

                MouseArea {
                    enabled: (Qt.platform.os === "windows") || (Qt.platform.os === "winrt")
                    anchors.fill: parent
                    acceptedButtons: Qt.NoButton
                    cursorShape: parent.hoveredLink ? Qt.PointingHandCursor : Qt.ArrowCursor
                }
            }

            Label {
                bottomPadding: 17
                font.bold: true
                font.italic: true
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

                MouseArea {
                    anchors.fill: parent
                    acceptedButtons: Qt.NoButton
                    cursorShape: parent.hoveredLink ? Qt.PointingHandCursor : Qt.ArrowCursor
                }
            }
        }
    }
}
