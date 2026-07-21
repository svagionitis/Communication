import QtQuick import QtQuick.Controls import QtQuick.Layouts import CommunicationGui

    Window{id:root width: 900 height: 650 visible: true title: "Communication Library - Qt6 QML GUI Client" color: "#1e1e2e"

           CommunicationBridge{id:bridge mode:modeCombo.currentText host:hostField.text port:parseInt(portField.text) serialDevice:deviceField.text baudRate:parseInt(baudField.text) flowControl:flowCombo.currentText }

                                                                                                      ColumnLayout{anchors.fill:parent anchors.margins: 16 spacing: 16

                                                                                                                   // Top Control Header Panel
                                                                                                                   Rectangle{Layout.fillWidth: true implicitHeight: 120 color: "#252538" radius: 8 border.color: "#3b3b54"

                                                                                                                             GridLayout{anchors.fill:parent anchors.margins: 12 columns: 4 rowSpacing: 10 columnSpacing: 12

                                                                                                                                        Label{text: "Protocol Mode:";
color : "#cdd6f4";
font.bold : true
}
ComboBox {
    id: modeCombo model: ["TCP", "UDP", "SERIAL"] currentIndex: 0 Layout.fillWidth: true enabled: !bridge.isConnected
}

Label
{
text:
    "Status:";
color:
    "#cdd6f4";
    font.bold : true
}
RowLayout
{
spacing:
    8 Rectangle
    {
    width:
        14;
    height:
        14;
    radius:
        7 color : bridge.isConnected ? "#40a02b" : "#e64553"
    }
    Label
    {
    text:
        bridge.isConnected                          ? "CONNECTED"
        : "DISCONNECTED" color : bridge.isConnected ? "#40a02b"
                                                    : "#e64553" font.bold : true
    }
}

// TCP/UDP settings
Label {text: "Host / IP:" color: "#cdd6f4" visible: modeCombo.currentText != = "SERIAL"} TextField {
    id: hostField text: "127.0.0.1" Layout.fillWidth: true
        visible: modeCombo.currentText != = "SERIAL" enabled: !bridge.isConnected
}

Label {text: "Port:" color: "#cdd6f4" visible: modeCombo.currentText != = "SERIAL"} TextField {
    id: portField text: "8080" Layout.fillWidth: true visible: modeCombo.currentText != = "SERIAL"
                                                                                          enabled: !bridge.isConnected
}

// Serial settings
Label {text: "Serial Device:" color: "#cdd6f4" visible: modeCombo.currentText == = "SERIAL"} TextField {
    id: deviceField text: "/dev/ttyUSB0" Layout.fillWidth: true
        visible: modeCombo.currentText == = "SERIAL" enabled: !bridge.isConnected
}

Label {text: "Baud Rate:" color: "#cdd6f4" visible: modeCombo.currentText == = "SERIAL"} ComboBox
{
    Label {text: "Flow Control:" color: "#cdd6f4" visible: modeCombo.currentText == = "SERIAL"} ComboBox
    {
    id:
        flowCombo model : ["None", "Hardware (RTS/CTS)",
                           "Software (XON/XOFF)"] currentIndex : 0 Layout.fillWidth : true visible
            : modeCombo.currentText == = "SERIAL" enabled : !bridge.isConnected
    }
}
}

// Action Buttons Row
RowLayout
{
    Layout.fillWidth : true spacing : 12

                       Button
    {
    text:
        bridge.isConnected ? "Disconnect" : "Connect" highlighted : true onClicked:
        {
            if (bridge.isConnected) {
                bridge.disconnectChannel();
            } else {
                bridge.connectChannel();
            }
        }
    }

    Button {text: "Clear Output Logs" onClicked: bridge.clearLogs()}

    Item
    {
        Layout.fillWidth : true
    }
}

// Log Output Console Area
Rectangle
{
    Layout.fillWidth : true Layout.fillHeight : true color : "#11111b" radius : 8 border.color : "#3b3b54"

                                                                                                 ScrollView
    {
        anchors.fill : parent anchors.margins : 8

                       TextArea
        {
        id:
            logConsole text : bridge.logOutput readOnly : true font.family
                : "Monospace" font.pixelSize : 13 color : "#a6adc8" wrapMode : Text.WrapAnywhere selectByMouse : true
        }
    }
}

// Transmit Message Bar
RowLayout
{
    Layout.fillWidth
        : true spacing : 12

          TextField {
              id: inputField placeholderText: "Type message payload to transmit..." Layout.
              fillWidth: true font.pixelSize: 14 onAccepted: sendBtn.clicked()
          }

          Button
    {
    id:
        sendBtn text : "Send Data" highlighted : true enabled
            : bridge.isConnected&& inputField.text.length > 0 onClicked:
        {
            bridge.sendMessage(inputField.text);
            inputField.clear();
        }
    }
}
}
}
