import QtQuick
import QtQuick.Effects

Item {
    id: root

    property string name: ""
    property color color: "#ffffff"
    property int size: 16

    width: size
    height: size
    implicitWidth: size
    implicitHeight: size

    Image {
        id: sourceImage
        anchors.fill: parent
        source: root.name.length > 0
                ? Qt.resolvedUrl("../../assets/icons/" + root.name + ".svg")
                : ""
        sourceSize: Qt.size(root.size * 2, root.size * 2)
        fillMode: Image.PreserveAspectFit
        smooth: true
        visible: false
    }

    MultiEffect {
        anchors.fill: parent
        source: sourceImage
        colorization: 1.0
        colorizationColor: root.color
        opacity: root.color.a
    }
}
