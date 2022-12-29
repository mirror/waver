//import QtQml 2.3
//import QtQuick 2.12
//import QtQuick.Controls 2.3
//import QtQuick.Layouts 1.3

import QtQml
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts


Dialog {
    title: qsTr("Server Password")
    modal: true
    focus: true
    standardButtons: Dialog.Ok | Dialog.Cancel


    signal setPassword(string id, string psw);

    onAccepted: {
        for (var i = 0; i < serverPswModel.count; i++) {
            setPassword(serverPswModel.get(i).server_id, serverPswModel.get(i).server_psw);
        }
        serverPswModel.clear();
    }
    onRejected: {
        serverPswModel.clear();
    }

    function promptAdd(serverId, serverName)
    {
        var newDict = {
            server_id: serverId,
            server_name: serverName,
            server_psw: ""
        }
        serverPswModel.append(newDict);
    }

    ListModel {
        id: serverPswModel
    }


    ListView {
        anchors.fill: parent
        model: serverPswModel

        delegate: Item {
            height: serverPsw.height + 5
            width: parent.width

            Label {
                id: serverName
                anchors.left: parent.left
                anchors.top: parent.top
                rightPadding: 13
                text: server_name
            }
            TextField {
                id: serverPsw
                anchors.verticalCenter: serverName.verticalCenter
                anchors.left: serverName.right
                anchors.right: parent.right
                echoMode: TextInput.PasswordEchoOnEdit
                onEditingFinished: server_psw = text
            }
        }
    }
}
