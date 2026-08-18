#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QEvent>
#include <QMouseEvent>
#include <QStyle>
#include <QSerialPortInfo>
#include <QTimer>
#include <QPainter>
#include <QLabel>
#include <algorithm>

// UI 초기화 및 시리얼/알람 관련 시그널-슬롯 연결
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , serialPort(new QSerialPort(this))
    , watchdogTimer(new QTimer(this))
    , alarmCheckTimer(new QTimer(this))
    , alarmOffTimer(new QTimer(this))
    , chart(new QChart())
    , chartSeries(new QLineSeries())
    , chartAxisX(new QDateTimeAxis())
    , chartAxisY(new QValueAxis())
    , chartSampleTimer(new QTimer(this))
{
    ui->setupUi(this);

    // 카드 3개에 마우스 클릭을 감지하도록 이벤트 필터 설치
    ui->temperatureCard->installEventFilter(this);
    ui->humidityCard->installEventFilter(this);
    ui->illuminanceCard->installEventFilter(this);

    // 포트 검색/연결/해제 버튼
    connect(ui->refreshButton, &QPushButton::clicked, this, &MainWindow::onRefreshPorts);
    connect(ui->connectButton, &QPushButton::clicked, this, &MainWindow::onConnectClicked);
    connect(ui->disconnectButton, &QPushButton::clicked, this, &MainWindow::onDisconnectClicked);

    // 시리얼 포트 수신/에러 시그널
    connect(serialPort, &QSerialPort::readyRead, this, &MainWindow::onSerialReadyRead);
    connect(serialPort, &QSerialPort::errorOccurred, this, &MainWindow::onSerialErrorOccurred);

    // 응답 없음(논리적 단절) 감시용 워치독 타이머
    watchdogTimer->setInterval(watchdogIntervalMs);
    connect(watchdogTimer, &QTimer::timeout, this, &MainWindow::onWatchdogTimeout);

    // 전등/제습/창문 제어 버튼 - 클릭 시 대응하는 UART 명령 전송
    connect(ui->ledOnButton, &QPushButton::clicked, this, [this] { sendCommand("L0001"); });
    connect(ui->ledOffButton, &QPushButton::clicked, this, [this] { sendCommand("L0000"); });
    connect(ui->dehumidifierOnButton, &QPushButton::clicked, this, [this] { sendCommand("D0001"); });
    connect(ui->dehumidifierOffButton, &QPushButton::clicked, this, [this] { sendCommand("D0000"); });
    connect(ui->windowCloseButton, &QPushButton::clicked, this, [this] { sendCommand("S0000"); });
    connect(ui->window25Button, &QPushButton::clicked, this, [this] { sendCommand("S0025"); });
    connect(ui->window50Button, &QPushButton::clicked, this, [this] { sendCommand("S0050"); });
    connect(ui->window75Button, &QPushButton::clicked, this, [this] { sendCommand("S0075"); });
    connect(ui->window100Button, &QPushButton::clicked, this, [this] { sendCommand("S0100"); });

    // 알람 설정/해제 버튼
    connect(ui->alarmSetButton, &QPushButton::clicked, this, &MainWindow::onAlarmSetClicked);
    connect(ui->alarmClearButton, &QPushButton::clicked, this, &MainWindow::onAlarmClearClicked);

    // 알람 시각 도달 여부를 1초마다 확인하는 타이머 (상시 동작)
    alarmCheckTimer->setInterval(alarmCheckIntervalMs);
    connect(alarmCheckTimer, &QTimer::timeout, this, &MainWindow::onAlarmCheckTimeout);
    alarmCheckTimer->start();

    // 알람이 울린 뒤 alarmDurationMs 후 자동으로 꺼지도록 하는 싱글샷 타이머
    alarmOffTimer->setSingleShot(true);
    connect(alarmOffTimer, &QTimer::timeout, this, &MainWindow::onAlarmOffTimeout);

    // 로그 영역 그래프 초기화 (X축: 실제 시각, Y축: 선택된 센서에 맞춰 갱신)
    chart->addSeries(chartSeries);
    chart->legend()->hide();
    chartAxisX->setFormat("HH:mm");
    chart->addAxis(chartAxisX, Qt::AlignBottom);
    chart->addAxis(chartAxisY, Qt::AlignLeft);
    chartSeries->attachAxis(chartAxisX);
    chartSeries->attachAxis(chartAxisY);

    chartView = new QChartView(chart, ui->chartContainer);
    chartView->setRenderHint(QPainter::Antialiasing);
    chartView->setVisible(false);
    ui->chartContainerLayout->addWidget(chartView);

    // 카드 선택은 됐지만 아직 기록이 없을 때 그래프 대신 보여줄 안내 라벨
    chartEmptyLabel = new QLabel(QStringLiteral("표시할 기록 없음"), ui->chartContainer);
    chartEmptyLabel->setAlignment(Qt::AlignCenter);
    chartEmptyLabel->setVisible(false);
    ui->chartContainerLayout->addWidget(chartEmptyLabel);

    // 카드 선택 여부와 무관하게 1분마다 상시 기록
    chartSampleTimer->setInterval(chartSampleIntervalMs);
    connect(chartSampleTimer, &QTimer::timeout, this, &MainWindow::onChartSampleTimeout);
    chartSampleTimer->start();

    // 초기 UI 상태: 미연결
    setConnectedUiState(false);
    setConnStatusText("연결 안 됨", false);
    onRefreshPorts();
}

// 열려 있는 포트를 정리하고 UI 리소스 해제
MainWindow::~MainWindow()
{
    if (serialPort->isOpen())
        serialPort->close();
    delete ui;
}

// 온도/습도/조도 카드 클릭을 감지해 해당 카드를 토글
bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::MouseButtonRelease) {
        if (watched == ui->temperatureCard) {
            toggleSensorCard(ui->temperatureCard, "온도");
            return true;
        } else if (watched == ui->humidityCard) {
            toggleSensorCard(ui->humidityCard, "습도");
            return true;
        } else if (watched == ui->illuminanceCard) {
            toggleSensorCard(ui->illuminanceCard, "조도");
            return true;
        }
    }
    return QMainWindow::eventFilter(watched, event);
}

// 클릭한 카드를 선택/해제하고, 선택된 카드에 맞춰 로그 제목 표시를 갱신
void MainWindow::toggleSensorCard(QWidget *card, const QString &title)
{
    bool wasSelected = card->property("selected").toBool();

    // 세 카드 전부 선택 해제
    for (QWidget *c : {ui->temperatureCard, ui->humidityCard, ui->illuminanceCard}) {
        c->setProperty("selected", false);
        c->style()->unpolish(c);
        c->style()->polish(c);
        c->update();
    }

    if (!wasSelected) {
        card->setProperty("selected", true);
        card->style()->unpolish(card);
        card->style()->polish(card);
        card->update();

        ui->chartTitleLabel->setText(title + " 기록");
        selectedSensor = sensorTypeForCard(card);
    } else {
        ui->chartTitleLabel->setText("센서 카드 선택 시 기록 표시");
        selectedSensor = SensorType::None;
    }

    refreshChartDisplay();
}

// 카드 위젯 포인터를 대응하는 센서 종류로 변환
MainWindow::SensorType MainWindow::sensorTypeForCard(QWidget *card) const
{
    if (card == ui->temperatureCard)
        return SensorType::Temperature;
    if (card == ui->humidityCard)
        return SensorType::Humidity;
    if (card == ui->illuminanceCard)
        return SensorType::Illuminance;
    return SensorType::None;
}

// 현재 시스템에 연결된 COM 포트 목록을 콤보박스에 다시 채움
void MainWindow::onRefreshPorts()
{
    const QString previouslySelected = ui->portCombo->currentText();

    ui->portCombo->clear();
    for (const QSerialPortInfo &info : QSerialPortInfo::availablePorts())
        ui->portCombo->addItem(info.portName());

    const int idx = ui->portCombo->findText(previouslySelected);
    if (idx >= 0)
        ui->portCombo->setCurrentIndex(idx);
}

// 선택된 포트를 115200bps 8N1로 열고, 성공 시 워치독을 시작
void MainWindow::onConnectClicked()
{
    const QString portName = ui->portCombo->currentText();
    if (portName.isEmpty())
        return;

    serialPort->setPortName(portName);
    serialPort->setBaudRate(QSerialPort::Baud115200);
    serialPort->setDataBits(QSerialPort::Data8);
    serialPort->setParity(QSerialPort::NoParity);
    serialPort->setStopBits(QSerialPort::OneStop);
    serialPort->setFlowControl(QSerialPort::NoFlowControl);

    if (!serialPort->open(QIODevice::ReadWrite)) {
        setConnStatusText("연결 실패: " + serialPort->errorString(), false);
        return;
    }

    lastRxTimer.start();
    watchdogTimer->start();
    resetChartHistory();

    setConnectedUiState(true);
    setConnStatusText("연결됨", true);
}

// 사용자가 직접 연결 해제 버튼을 눌렀을 때 포트를 닫음
void MainWindow::onDisconnectClicked()
{
    watchdogTimer->stop();
    if (serialPort->isOpen())
        serialPort->close();

    setConnectedUiState(false);
    setConnStatusText("연결 안 됨", false);
}

void MainWindow::onSerialReadyRead()
{
    // 프레임 파싱은 팀원 3,4(UART·시스템 통합) 담당.
    // 여기서는 연결 상태(워치독) 판단을 위해 수신 여부만 확인한다.
    serialPort->readAll();
    lastRxTimer.restart();
}

// QSerialPort가 보고하는 에러를 감지해 물리적 단절이면 연결을 정리
void MainWindow::onSerialErrorOccurred(QSerialPort::SerialPortError error)
{
    if (error == QSerialPort::NoError)
        return;

    // 케이블 분리 등 물리적 단절 - OS가 즉시 알려주는 경우
    if (error == QSerialPort::ResourceError || error == QSerialPort::DeviceNotFoundError)
        teardownConnection("장치가 분리됨");
}

// 1초마다 호출되어 마지막 수신 이후 linkTimeoutMs가 지났는지 확인
void MainWindow::onWatchdogTimeout()
{
    if (!serialPort->isOpen())
        return;

    // 케이블은 연결되어 있지만 일정 시간 데이터 수신이 없는 경우 - 논리적 단절로 판단
    if (lastRxTimer.elapsed() > linkTimeoutMs)
        teardownConnection("응답 없음 (타임아웃)");
}

// 워치독 타임아웃/장치 분리 등으로 연결이 끊겼을 때 공통으로 호출되는 정리 로직
void MainWindow::teardownConnection(const QString &reason)
{
    watchdogTimer->stop();
    if (serialPort->isOpen())
        serialPort->close();

    setConnectedUiState(false);
    setConnStatusText("연결 끊김: " + reason, false);
}

// 온도 업데이트 (실내 환경 카드는 즉시 반영, 그래프 기록용 최신값도 함께 저장)
void MainWindow::updateTemperature(double celsiusValue)
{
    ui->temperatureValueLabel->setText(QString::number(celsiusValue, 'f', 1) + " ℃");
    const bool isFirstSample = !hasTemperature;
    latestTemperature = celsiusValue;
    hasTemperature = true;

    // 첫 수신값은 다음 1분 주기를 기다리지 않고 바로 기록
    if (isFirstSample) {
        recordChartSample(temperatureHistory, true, celsiusValue, QDateTime::currentDateTime().toMSecsSinceEpoch());
        if (selectedSensor == SensorType::Temperature)
            refreshChartDisplay();
    }
}

// 습도 업데이트 (실내 환경 카드는 즉시 반영, 그래프 기록용 최신값도 함께 저장)
void MainWindow::updateHumidity(double percentValue)
{
    ui->humidityValueLabel->setText(QString::number(percentValue, 'f', 1) + " %");
    const bool isFirstSample = !hasHumidity;
    latestHumidity = percentValue;
    hasHumidity = true;

    if (isFirstSample) {
        recordChartSample(humidityHistory, true, percentValue, QDateTime::currentDateTime().toMSecsSinceEpoch());
        if (selectedSensor == SensorType::Humidity)
            refreshChartDisplay();
    }
}

// 초음파 거리 수신받아 현관문 열림/닫힘 판단 후 UI 업데이트
void MainWindow::updateDistance(double centimeterValue)
{
    const bool closed = centimeterValue <= doorClosedThresholdCm;
    ui->doorStateLabel->setText(closed ? QStringLiteral("닫힘") : QStringLiteral("열림"));
}

// 조도 업데이트 (실내 환경 카드는 즉시 반영, 그래프 기록용 최신값도 함께 저장)
void MainWindow::updateIlluminance(double percentValue)
{
    ui->illuminanceValueLabel->setText(QString::number(percentValue, 'f', 1) + " %");
    const bool isFirstSample = !hasIlluminance;
    latestIlluminance = percentValue;
    hasIlluminance = true;

    if (isFirstSample) {
        recordChartSample(illuminanceHistory, true, percentValue, QDateTime::currentDateTime().toMSecsSinceEpoch());
        if (selectedSensor == SensorType::Illuminance)
            refreshChartDisplay();
    }
}

// 알람 시각 선택 위젯의 시각을 목록에 추가 (중복이면 무시)
void MainWindow::onAlarmSetClicked()
{
    const QTime newTime = ui->alarmTimeEdit->time();
    if (alarmTimes.contains(newTime))
        return;

    alarmTimes.append(newTime);
    std::sort(alarmTimes.begin(), alarmTimes.end());
    refreshAlarmListWidget();
}

// 목록에서 선택한 알람을 삭제하고, 울리는 중이었다면 즉시 A0000 전송
void MainWindow::onAlarmClearClicked()
{
    QListWidgetItem *item = ui->alarmListWidget->currentItem();
    if (!item)
        return;

    const QTime selectedTime = item->data(Qt::UserRole).toTime();
    alarmTimes.removeOne(selectedTime);
    if (selectedTime == ringingAlarmTime) {
        alarmOffTimer->stop();
        ringingAlarmTime = QTime();
        sendCommand("A0000");
    }
    refreshAlarmListWidget();
}

// 1초마다 호출되어(정각일 때만) 현재 시각과 일치하는 알람이 있으면 울림
void MainWindow::onAlarmCheckTimeout()
{
    const QTime now = QTime::currentTime();
    if (now.second() != 0)
        return;

    for (const QTime &alarmTime : alarmTimes) {
        if (alarmTime.hour() == now.hour() && alarmTime.minute() == now.minute()) {
            ringingAlarmTime = alarmTime;
            sendCommand("A0001");
            ui->ledOnButton->click(); // 전등 ON 버튼을 실제로 눌러, 클릭 시그널 경로 그대로 L0001도 전송
            alarmOffTimer->start(alarmDurationMs);
            refreshAlarmListWidget();
            break;
        }
    }
}

// alarmDurationMs가 지나면 알람을 끄고, 울렸던 알람은 목록에서 자동 제거 (1회성)
void MainWindow::onAlarmOffTimeout()
{
    sendCommand("A0000");

    alarmTimes.removeOne(ringingAlarmTime);
    ringingAlarmTime = QTime();
    refreshAlarmListWidget();
}

// alarmTimes 내용을 바탕으로 알람 목록 위젯을 다시 그림 (울리는 중인 항목은 표시 추가)
void MainWindow::refreshAlarmListWidget()
{
    ui->alarmListWidget->clear();
    for (const QTime &alarmTime : alarmTimes) {
        const bool ringing = (alarmTime == ringingAlarmTime);
        const QString text = ringing ? alarmTime.toString("HH:mm") + " (울리는 중)"
                                      : alarmTime.toString("HH:mm");
        auto *item = new QListWidgetItem(text, ui->alarmListWidget);
        item->setData(Qt::UserRole, alarmTime);
    }
}

// 1분마다 호출되어, 값을 받아본 적 있는 센서의 최신값을 히스토리에 추가
void MainWindow::onChartSampleTimeout()
{
    const qint64 nowMs = QDateTime::currentDateTime().toMSecsSinceEpoch();
    recordChartSample(temperatureHistory, hasTemperature, latestTemperature, nowMs);
    recordChartSample(humidityHistory, hasHumidity, latestHumidity, nowMs);
    recordChartSample(illuminanceHistory, hasIlluminance, latestIlluminance, nowMs);

    if (selectedSensor != SensorType::None)
        refreshChartDisplay();
}

// 히스토리에 샘플 1건을 추가하고, chartHistoryMaxPoints를 넘으면 가장 오래된 샘플 삭제
void MainWindow::recordChartSample(QVector<QPointF> &history, bool hasValue, double value, qint64 nowMs)
{
    if (!hasValue)
        return;

    history.append(QPointF(static_cast<double>(nowMs), value));
    if (history.size() > chartHistoryMaxPoints)
        history.removeFirst();
}

// 선택된 센서의 히스토리를 그래프에 그리고, Y축을 해당 센서의 고정 범위로 설정.
// 카드가 선택되지 않았으면 그래프 자체를 숨기고, 기록이 없으면 안내 라벨을 대신 보여준다.
void MainWindow::refreshChartDisplay()
{
    if (selectedSensor == SensorType::None) {
        chartView->setVisible(false);
        chartEmptyLabel->setVisible(false);
        return;
    }

    QVector<QPointF> *history = nullptr;
    switch (selectedSensor) {
    case SensorType::Temperature:
        history = &temperatureHistory;
        chartAxisY->setRange(temperatureAxisMin, temperatureAxisMax);
        break;
    case SensorType::Humidity:
        history = &humidityHistory;
        chartAxisY->setRange(humidityAxisMin, humidityAxisMax);
        break;
    case SensorType::Illuminance:
        history = &illuminanceHistory;
        chartAxisY->setRange(illuminanceAxisMin, illuminanceAxisMax);
        break;
    case SensorType::None:
        return; // 위에서 이미 처리됨
    }

    if (history->isEmpty()) {
        chartSeries->clear();
        chartView->setVisible(false);
        chartEmptyLabel->setVisible(true);
        return;
    }

    chartEmptyLabel->setVisible(false);
    chartView->setVisible(true);

    chartSeries->clear();
    for (const QPointF &point : *history)
        chartSeries->append(point);

    QDateTime startTime = QDateTime::fromMSecsSinceEpoch(qint64(history->first().x()));
    QDateTime endTime = QDateTime::fromMSecsSinceEpoch(qint64(history->last().x()));
    if (startTime == endTime)
        endTime = startTime.addSecs(60);
    chartAxisX->setRange(startTime, endTime);
}

// 재연결 시 이전 기록을 모두 지우고 빈 그래프로 시작
void MainWindow::resetChartHistory()
{
    temperatureHistory.clear();
    humidityHistory.clear();
    illuminanceHistory.clear();
    hasTemperature = false;
    hasHumidity = false;
    hasIlluminance = false;
    refreshChartDisplay();
}

// 연결된 상태에서만 5byte 명령 프레임을 그대로 전송 (명령 1byte + 데이터 4byte)
void MainWindow::sendCommand(const QByteArray &frame)
{
    if (!serialPort->isOpen())
        return;

    serialPort->write(frame);
}

// 연결 여부에 따라 포트 선택/새로고침/연결/해제 버튼의 활성화 상태를 전환
void MainWindow::setConnectedUiState(bool connected)
{
    ui->portCombo->setEnabled(!connected);
    ui->refreshButton->setEnabled(!connected);
    ui->connectButton->setEnabled(!connected);
    ui->disconnectButton->setEnabled(connected);
}

// 상단 연결 상태 라벨에 색상 점 + 텍스트를 표시 (연결 시 초록, 아니면 빨강)
void MainWindow::setConnStatusText(const QString &text, bool connected)
{
    const QString dotColor = connected ? QStringLiteral("green") : QStringLiteral("red");
    ui->connStatusLabel->setText(
        QStringLiteral("<span style=\"color:%1;\">●</span> %2").arg(dotColor, text.toHtmlEscaped()));
}
