import QtQml 2.3
import QtQuick 2.12
import QtQuick.Controls 2.3
import QtQuick.Layouts 1.3

Dialog {
    title: qsTr("Server Password")
    modal: true
    focus: true
    standardButtons: Dialog.Ok | Dialog.Cancel

    function setIdName(id, formattedName)
    {
        internal.id = id;
        serverName.text = formattedName;
    }

    signal setPassword(string id, string psw);

    onAccepted: setPassword(internal.id, psw.text);
    onOpened  : psw.text = "";

    QtObject {
        id: internal
        property string id: "";
    }

    Label {
        id: serverName

        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        bottomPadding: 25
    }
    Label {
        id: pswLabel

        anchors.verticalCenter: psw.verticalCenter
        anchors.left: parent.left
        rightPadding: 15

        text: qsTr("Password")
    }
    TextField {
        id: psw

        anchors.top: serverName.bottom
        anchors.left: pswLabel.right
        anchors.right: parent.right

        echoMode: TextInput.PasswordEchoOnEdit
    }
}
