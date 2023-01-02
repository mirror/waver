import QtQml 2.3
import QtQuick 2.12

Item {
    property bool  borderVisible: true
    property color borderColor: "#666666"
    property color replayGainColor: "#AAAAAA"

    function setPeak(l, r)
    {
        if (l < 0) {
            l = 0;
        }
        if (l > 1) {
            l = 1;
        }
        if (r < 0) {
            r = 0;
        }
        if (r > 1) {
            r = 1;
        }

        lPeak.width = scaleBackground.width * l;
        rPeak.width = scaleBackground.width * r;
    }

    function setReplayGain(g)
    {
        if (g < 0) {
            g = 0;
        }
        if (g > 1) {
            g = 1;
        }
        internal.replayGainValue = g
    }


    QtObject {
        id: internal

        property double replayGainValue: 0
    }


    Rectangle {
        anchors.fill: parent
        border.color: borderColor
        color: "transparent"
        visible: borderVisible
    }

    Rectangle {
        id: scaleBackground

        width: parent.width - 10
        height: parent.height - 10
        anchors.centerIn: parent
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0;    color: "darkgreen" }
            GradientStop { position: 0.2;  color: "green" }
            GradientStop { position: 0.33; color: "orange" }
            GradientStop { position: 1;    color: "red" }
        }
    }

    Rectangle {
        width: parent.width - 2
        height: 5
        anchors.centerIn: parent
    }

    Rectangle {
        id: lPeak

        anchors.top: parent.top
        anchors.right: parent.right
        anchors.topMargin: 5
        anchors.rightMargin: 5
        height: 5
        width: 0
    }

    Rectangle {
        id: rPeak

        anchors.bottom: parent.bottom
        anchors.right: parent.right
        anchors.bottomMargin: 5
        anchors.rightMargin: 5
        height: 5
        width: 0
    }

    Rectangle {
        color: replayGainColor
        anchors.verticalCenter: parent.verticalCenter
        height: 3
        width: 3
        x: internal.replayGainValue * scaleBackground.width + scaleBackground.x;
    }
}
