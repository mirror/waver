import QtQml 2.3
import QtQuick 2.12

Item {
    property bool  borderVisible: true
    property color borderColor: "#666666"

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

        lPeak.width = scaleBackground.height * l;
        rPeak.width = scaleBackground.height * r;
    }


    Rectangle {
        anchors.fill: parent
        border.color: borderColor
        color: "transparent"
        visible: borderVisible
    }

    Rectangle {
        id: scaleBackground

        width: parent.height - 10
        height: parent.width - 10
        anchors.centerIn: parent
        rotation: 90
        gradient: Gradient {
            GradientStop { position: 1.0;  color: "darkgreen" }
            GradientStop { position: 0.33; color: "green" }
            GradientStop { position: 0.2;  color: "orange" }
            GradientStop { position: 0;    color: "red" }
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
}
