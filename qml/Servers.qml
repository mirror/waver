import QtQml 2.3
import QtQuick 2.12
import QtQuick.Controls 2.3
import QtQuick.Layouts 1.3

Dialog {
    title: qsTr("Servers")
    modal: true
    focus: true
    standardButtons: Dialog.Close

    property var serversModel: []

    signal addServer(string host, string user, string psw);
    signal delServer(string id);

    onOpened: {
        host.text = "https://";
        user.text = "";
        psw.text  = "";
    }


    Item {
        id: existing

        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: parent.height * 0.33

        ComboBox {
            id: serversList

            anchors.left: parent.left
            anchors.right: deleteButton.left
            anchors.verticalCenter: parent.verticalCenter
            anchors.rightMargin: 13
            width: parent.width * 0.75

            enabled: serversModel.length > 0
            model: serversModel
            textRole: "title"
        }
        Button {
            id: deleteButton

            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter

            enabled: serversModel.length > 0
            text: qsTr("Delete")

            onClicked: {
                delServer(serversModel[serversList.currentIndex].id);
                close();
            }
        }
    }

    GridLayout {
        anchors.top: existing.bottom
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right

        //columns: 3

        Label {
            Layout.row: 1
            Layout.column: 1

            text: qsTr("Server")
        }
        TextField {
            id: host

            Layout.row: 1
            Layout.column: 2
            implicitWidth: parent.width / 2

            focus: true
            validator: RegExpValidator {
                regExp: /http[s]?:\/\/.*/;
            }
        }

        Label {
            Layout.row: 2
            Layout.column: 1

            text: qsTr("User")
        }
        TextField {
            id: user

            Layout.row: 2
            Layout.column: 2
            implicitWidth: parent.width / 2
        }

        Label {
            Layout.row: 3
            Layout.column: 1

            text: qsTr("Password")
        }
        TextField {
            id: psw

            Layout.row: 3
            Layout.column: 2
            implicitWidth: parent.width / 2

            echoMode: TextInput.PasswordEchoOnEdit
        }

        Button {
            id: addButton

            Layout.row: 3
            Layout.column: 3

            enabled: host.text.length && user.text.length && psw.text.length
            text: qsTr("Add")

            onClicked: {
                addServer(host.text, user.text, psw.text);
                close();
            }
        }
    }
}

