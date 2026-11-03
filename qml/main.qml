import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12
import Qt.labs.platform 1.1 as Platform

ApplicationWindow {
    id: window
    visible: true
    width: 1200
    height: 720
    minimumWidth: 960
    minimumHeight: 600
    title: qsTr("Баланс белого — коррекция видео")

    readonly property int algoGreyWorld: 0
    readonly property int algoWhitePatch: 1
    readonly property int algoPerfectReflector: 2
    readonly property int algoShadesOfGrey: 3
    readonly property int algoGreyEdge: 4

    property int currentAlgorithm: 0
    property bool suppressSeek: false

    palette {
        window: "#1e1f24"
        windowText: "#e8e8ea"
        base: "#26272d"
        text: "#e8e8ea"
        button: "#33353d"
        buttonText: "#e8e8ea"
        highlight: "#4a7dff"
        highlightedText: "#ffffff"
        toolTipBase: "#33353d"
        toolTipText: "#e8e8ea"
    }

    Platform.FileDialog {
        id: fileDialog
        title: qsTr("Открыть видео")
        nameFilters: ["Видео (*.mp4 *.avi *.mkv *.mov *.webm *.m4v *.wmv)", "Все файлы (*)"]
        onAccepted: processor.openFile(file.toString().replace(/^file:\/\//, ""))
    }

    Connections {
        target: processor
        onFrameReady: videoImage.source = image
        onErrorOccurred: errorPopup.text = message; errorPopup.open()
        onPositionChanged: {
            if (!positionSlider.pressed) {
                suppressSeek = true
                positionSlider.value = processor.position
                suppressSeek = false
            }
        }
    }

    Popup {
        id: errorPopup
        property alias text: errorLabel.text
        x: Math.round((window.width - width) / 2)
        y: Math.round((window.height - height) / 2)
        modal: true
        padding: 16
        background: Rectangle { color: "#33353d"; radius: 6 }
        contentItem: Label { id: errorLabel; text: "" }
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 12

        // ---------------- Левая часть: видео и управление ----------------
        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 10

            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: "black"
                radius: 6

                Image {
                    id: videoImage
                    anchors.fill: parent
                    anchors.margins: 4
                    fillMode: Image.PreserveAspectFit
                    asynchronous: true
                }

                Label {
                    anchors.centerIn: parent
                    visible: !processor.opened
                    text: qsTr("Откройте видеофайл, чтобы начать")
                    color: "#888a92"
                    font.pixelSize: 16
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 10

                Button {
                    text: qsTr("Открыть видео…")
                    onClicked: fileDialog.open()
                }

                Button {
                    id: playButton
                    enabled: processor.opened
                    text: processor.playing ? qsTr("Пауза") : qsTr("Играть")
                    onClicked: processor.playing ? processor.pause() : processor.play()
                }

                Slider {
                    id: positionSlider
                    Layout.fillWidth: true
                    enabled: processor.opened
                    from: 0
                    to: Math.max(1, processor.frameCount - 1)
                    stepSize: 1
                    onMoved: {
                        if (!suppressSeek)
                            processor.seekFrame(value)
                    }
                }

                Label {
                    text: processor.opened
                          ? (processor.position + " / " + processor.frameCount)
                          : "-- / --"
                    color: "#a9abb3"
                }
            }
        }

        // ---------------- Правая часть: настройки баланса белого ----------------
        Rectangle {
            Layout.preferredWidth: 340
            Layout.fillHeight: true
            color: "#26272d"
            radius: 8

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 16
                spacing: 12

                Label {
                    text: qsTr("Алгоритм баланса белого")
                    font.bold: true
                    font.pixelSize: 16
                }

                ComboBox {
                    id: algoCombo
                    Layout.fillWidth: true
                    model: [
                        qsTr("Серый мир (Grey World)"),
                        qsTr("Белое пятно (White Patch)"),
                        qsTr("Идеальный отражатель (Perfect Reflector)"),
                        qsTr("Оттенки серого (Shades of Grey)"),
                        qsTr("Серая граница (Grey Edge)")
                    ]
                    onCurrentIndexChanged: currentAlgorithm = currentIndex
                }

                Label {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    text: {
                        switch (currentAlgorithm) {
                        case algoGreyWorld:
                            return qsTr("Среднее по кадру считается нейтрально-серым; каждый канал масштабируется так, чтобы средние совпали. Надёжен на цветных сценах.")
                        case algoWhitePatch:
                            return qsTr("Самый яркий пикел принимается за белый; каналы растягиваются по своим максимумам. Хорош, когда в кадре есть белый объект.")
                        case algoPerfectReflector:
                            return qsTr("Как White Patch, но опирается на среднее верхних процентов самых ярких пикселей — устойчивее к шуму и выбросам.")
                        case algoShadesOfGrey:
                            return qsTr("Обобщение «серого мира» нормой Минковского степени p: p=1 — Grey World, p→∞ — White Patch. p=4…10 обычно самый естественный результат.")
                        case algoGreyEdge:
                            return qsTr("Метод van de Weijer et al.: статистика считается по производным (границам объектов), а не по яркостям — лучше сохраняет насыщенные цвета.")
                        default:
                            return ""
                        }
                    }
                    color: "#a9abb3"
                    font.pixelSize: 12
                }

                // --- Сила коррекции (у всех алгоритмов) ---
                Label { text: qsTr("Сила коррекции: ") + strengthSlider.value + "%" }
                Slider {
                    id: strengthSlider
                    Layout.fillWidth: true
                    from: 0
                    to: 100
                    value: 100
                    stepSize: 1
                }

                // --- Процент ярких пикселей (Perfect Reflector) ---
                Label {
                    visible: currentAlgorithm === algoPerfectReflector
                    text: qsTr("Верхние % ярких пикселей: ") + percentileSlider.value.toFixed(1)
                }
                Slider {
                    id: percentileSlider
                    visible: currentAlgorithm === algoPerfectReflector
                    Layout.fillWidth: true
                    from: 0.5
                    to: 10.0
                    value: 1.0
                    stepSize: 0.1
                }

                // --- Показатель Минковского p (Shades of Grey, Grey Edge) ---
                Label {
                    visible: currentAlgorithm === algoShadesOfGrey || currentAlgorithm === algoGreyEdge
                    text: qsTr("Показатель Минковского p: ") + pnormSlider.value.toFixed(1)
                }
                Slider {
                    id: pnormSlider
                    visible: currentAlgorithm === algoShadesOfGrey || currentAlgorithm === algoGreyEdge
                    Layout.fillWidth: true
                    from: 1
                    to: 20
                    value: 6
                    stepSize: 0.5
                }

                // --- Порядок производной (Grey Edge) ---
                Label {
                    visible: currentAlgorithm === algoGreyEdge
                    text: qsTr("Порядок производной: ") + derivSlider.value
                }
                Slider {
                    id: derivSlider
                    visible: currentAlgorithm === algoGreyEdge
                    Layout.fillWidth: true
                    from: 0
                    to: 2
                    value: 1
                    stepSize: 1
                }

                Item { Layout.fillHeight: true }

                Label {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    text: qsTr("Текущие множители  B: %1  G: %2  R: %3")
                        .arg(processor.gainB.toFixed(3))
                        .arg(processor.gainG.toFixed(3))
                        .arg(processor.gainR.toFixed(3))
                    color: "#a9abb3"
                    font.pixelSize: 12
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10

                    Button {
                        Layout.fillWidth: true
                        enabled: processor.opened
                        text: qsTr("Применить к видео")
                        highlighted: true
                        onClicked: processor.applyWhiteBalance(
                            currentAlgorithm,
                            strengthSlider.value / 100.0,
                            percentileSlider.value,
                            pnormSlider.value,
                            derivSlider.value)
                    }

                    Button {
                        text: qsTr("Сброс")
                        enabled: processor.opened
                        onClicked: processor.resetWhiteBalance()
                    }
                }

                Label {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    text: qsTr("Коррекция вычисляется один раз по текущему кадру и фиксируется на всё видео — оттенки не «плавают» во время воспроизведения.")
                    color: "#7d7f87"
                    font.pixelSize: 11
                    font.italic: true
                }
            }
        }
    }
}
