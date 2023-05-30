import QtQuick 2.0
import QtWebEngine 1.0

    // WebEngineView {
    // width: 1024
    // height: 750
    // visible: true
    //     url: "https://www.testufo.com/"
    // }

Rectangle {
    color: "red"

    ColorAnimation on color { to: "yellow"; duration: 1000 }

    SequentialAnimation on color {
            loops: Animation.Infinite
            ColorAnimation { from: "red"; to: "yellow"; duration: 1000 }
            ColorAnimation { from: "yellow"; to: "red"; duration: 1000 }
        }

    Text {
        anchors.centerIn: parent
        text: "Hello, World!"
    }
}