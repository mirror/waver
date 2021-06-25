import QtQml 2.3
import QtQuick 2.12
import QtQuick.Controls 2.3
import QtQuick.Layouts 1.3

Dialog {
    title: qsTr("Log")
    modal: true
    focus: true
    standardButtons: Dialog.Ok

    onClosed: {
        logLabel.text = "";
    }

    function setLog(logText)
    {
        logLabel.text = logText;
    }

    function addLog(newText)
    {
        logLabel.text = logLabel.text + newText;
    }

    ScrollView {
        x: 10
        y: 10
        width: parent.width - 20
        height: parent.height - 20

        clip: true

        Label {
            id: logLabel
        }
    }
}
