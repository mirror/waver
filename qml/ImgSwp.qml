import QtQml 2.3
import QtQuick 2.12

Item {
    property url   defaultImage: ""
    property int   swapDuration: 750
    property bool  borderVisible: true
    property color borderColor: "#666666"


    function swapImage(newImage)
    {
        if (internal.tempImage) {
            internal.tempImage = newImage;
            return;
        }

        if (state === "IMG1") {
            if (newImage === img1.source.toString()) {
                return;
            }

            img2.source = "";
            img2.source = newImage;
            return;
        }

        if (newImage === img2.source.toString()) {
            return;
        }

        img1.source = "";
        img1.source = newImage;
    }

    function swapTempImage(newImage)
    {
        if (state === "IMG1") {
            if (!internal.tempImage) {
                internal.tempImage = img1.source;
            }

            img2.source = "";
            img2.source = newImage;

            tempTimer.restart();
            return;
        }

        if (!internal.tempImage) {
            internal.tempImage = img2.source;
        }

        img1.source = "";
        img1.source = newImage;

        tempTimer.restart();
    }

    QtObject {
        id: internal

        property string tempImage: ""
    }

    Timer {
        id: tempTimer
        interval: 2500

        onTriggered: {
            if (internal.tempImage) {
                var img = internal.tempImage
                internal.tempImage = ""
                swapImage(img);
            }
        }
    }


    state: "IMG1"


    Image {
        id: img1
        anchors.fill: parent
        asynchronous: true
        cache: true
        opacity: 1
        source: defaultImage
        smooth: true

        onStatusChanged: {
            if (img1.status === Image.Error) {
                img1.source = defaultImage;
                return;
            }
            if (img1.status === Image.Ready) {
                parent.state = "IMG1";
            }
        }
    }

    Image {
        id: img2
        anchors.fill: parent
        asynchronous: true
        cache: true
        opacity: 0
        smooth: true

        onStatusChanged: {
            if (img2.status === Image.Error) {
                img2.source = defaultImage;
                return;
            }
            if (img2.status === Image.Ready) {
                parent.state = "IMG2";
            }
        }
    }

    Rectangle {
        anchors.fill: parent
        border.color: borderColor
        color: "transparent"
        visible: borderVisible
    }


    states: [
        State {
            name: "IMG1"
            PropertyChanges {
                target: img1
                opacity: 1
            }
            PropertyChanges {
                target: img2
                opacity: 0
            }
        },
        State {
            name: "IMG2"
            PropertyChanges {
                target: img1
                opacity: 0
            }
            PropertyChanges {
                target: img2
                opacity: 1
            }
        }
    ]

    transitions: [
        Transition {
            NumberAnimation {
                property: "opacity"
                easing.type: Easing.InOutQuad
                duration: swapDuration
            }
        }
    ]
}
