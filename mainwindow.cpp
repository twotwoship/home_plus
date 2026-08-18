#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QDebug>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    m_serialPort = new QSerialPort(this);

    // 10초 주기 UI 화면 리프레시 타이머 설정
    m_uiUpdateTimer = new QTimer(this);
    connect(m_uiUpdateTimer, &QTimer::timeout, this, &MainWindow::onUiUpdateTimerTimeout);
    m_uiUpdateTimer->start(10000); // 10초 설정

    // 5초 주기 M4 생존 감시 타임아웃 타이머 설정
    m_m4WatchdogTimer = new QTimer(this);
    connect(m_m4WatchdogTimer, &QTimer::timeout, this, &MainWindow::onM4WatchdogTimeout);

    // 프로그램 구동 초기 화면 초기화 문구 및 스타일 지정
    ui->lblStatus->setText("포트 연결 대기 중");
    ui->lblStatus->setStyleSheet("color: gray; font-weight: bold;");

    ui->lblTemperature->setText("온도: 대기 중");
    ui->lblHumidity->setText("습도: 대기 중");
    ui->lblDistance->setText("거리: 대기 중");
    ui->lblIlluminance->setText("조도: 대기 중");

    // 테스트 환경에 맞춰 COM 포트 강제 자동 개방 시도
    initSerialPort("COM3");
}

MainWindow::~MainWindow()
{
    closeSerialPort();
    delete ui;
}

void MainWindow::initSerialPort(QString portName)
{
    if(m_serialPort->isOpen()) m_serialPort->close();

    m_serialPort->setPortName(portName);
    m_serialPort->setBaudRate(QSerialPort::Baud115200);
    m_serialPort->setDataBits(QSerialPort::Data8);
    m_serialPort->setParity(QSerialPort::NoParity);
    m_serialPort->setStopBits(QSerialPort::OneStop);
    m_serialPort->setFlowControl(QSerialPort::NoFlowControl);

    if (m_serialPort->open(QIODevice::ReadWrite)) {
        qDebug() << "시리얼 통신 오픈 성공:" << portName;
        connect(m_serialPort, &QSerialPort::readyRead, this, &MainWindow::handleReadyRead);

        // 포트가 열리는 순간부터 5초 watchdog 타이머 기동
        m_m4WatchdogTimer->start(5000);
    } else {
        qDebug() << "포트 오픈 실패:" << m_serialPort->errorString();
        ui->lblStatus->setText("오류: COM 포트 연결 실패");
        ui->lblStatus->setStyleSheet("color: orange; font-weight: bold;");
    }
}

void MainWindow::closeSerialPort()
{
    if (m_serialPort->isOpen()) {
        m_serialPort->close();
        m_m4WatchdogTimer->stop();
        qDebug() << "시리얼 통신 포트 차단됨";

        ui->lblStatus->setText("포트 연결 해제 상태");
        ui->lblStatus->setStyleSheet("color: black;");
    }
}

// 명세 규격에 맞춘 송신 처리 메인 공용 함수 구조
void MainWindow::sendLedCommand(bool on)     { if (m_serialPort->isOpen()) m_serialPort->write(QString("L%1").arg(on ? 1 : 0, 4, 10, QChar('0')).toLatin1()); }
void MainWindow::sendDcMotorCommand(bool on) { if (m_serialPort->isOpen()) m_serialPort->write(QString("D%1").arg(on ? 1 : 0, 4, 10, QChar('0')).toLatin1()); }
void MainWindow::sendStepperCommand(int angle){ if (angle >= 0 && angle <= 9999 && m_serialPort->isOpen()) m_serialPort->write(QString("S%1").arg(angle, 4, 10, QChar('0')).toLatin1()); }
void MainWindow::sendAlarmCommand(bool on)   { if (m_serialPort->isOpen()) m_serialPort->write(QString("A%1").arg(on ? 1 : 0, 4, 10, QChar('0')).toLatin1()); }

// 실시간 수신 및 메모리 데이터 백업 처리 함수
void MainWindow::handleReadyRead()
{
    if (m_serialPort->isOpen()) {
        // 데이터가 유입되었으므로 5초 감시 타이머의 만료 시점을 초기 상태로 무조건 갱신
        m_m4WatchdogTimer->start(5000);

        // 유실 복구: 직전까지 연결 끊김 상태였다면 연결 완료 상태 정보로 즉시 변환
        if (!m_isM4Connected) {
            m_isM4Connected = true;
            qDebug() << "상태 알림: M4 보드 정상 연결 상태 감지";

            ui->lblStatus->setText("M4 보드 정상 연결됨");
            ui->lblStatus->setStyleSheet("color: green; font-weight: bold;");

            emit m4ConnectionStatusChanged(true);
        }
    }

    m_rxBuffer.append(m_serialPort->readAll());

    while (m_rxBuffer.length() >= 5)
    {
        QByteArray frame = m_rxBuffer.left(5);
        m_rxBuffer.remove(0, 5);

        char cmd = frame.at(0);
        bool conversionOk = false;
        int rawValue = frame.mid(1, 4).toInt(&conversionOk);

        if (!conversionOk) continue;

        // 파싱된 실제 값들을 전역 필드 멤버 변수에 즉시 동기화 적재
        switch (cmd) {
            case 'T': m_latestTemperature = rawValue / 10.0; break;
            case 'H': m_latestHumidity = rawValue / 10.0;    break;
            case 'U': m_latestDistance = rawValue;            break;
            case 'B': m_latestIlluminance = rawValue;         break;

            case 'L': emit loopbackStatusReceived(QString("LED 제어 피드백 수신: %1").arg(rawValue == 1 ? "ON" : "OFF")); break;
            case 'D': emit loopbackStatusReceived(QString("DC모터 제어 피드백 수신: %1").arg(rawValue == 1 ? "ON" : "OFF")); break;
            case 'S': emit loopbackStatusReceived(QString("스테핑 모터 제어 피드백 수신: %1도").arg(rawValue)); break;
            case 'A': emit loopbackStatusReceived(QString("알람 제어 피드백 수신: %1").arg(rawValue == 1 ? "ON" : "OFF")); break;
        }
    }
}

// 10초 타이머 주기에 도달했을 때만 누적된 최종 결과 데이터를 UI 텍스트 화면에 표출
void MainWindow::onUiUpdateTimerTimeout()
{
    if (m_isM4Connected) {
        qDebug() << "10초 주기 정기 리프레시: UI 라벨 화면 데이터 변경 실행";

        // 각 센서 규격 문자열 포맷 조합 후 화면 제어 레이어 반영
        ui->lblTemperature->setText(QString("온도: %1 C").arg(m_latestTemperature, 0, 'f', 1));
        ui->lblHumidity->setText(QString("습도: %1 %").arg(m_latestHumidity, 0, 'f', 1));
        ui->lblDistance->setText(QString("거리: %1 cm").arg(m_latestDistance));
        ui->lblIlluminance->setText(QString("조도: %1 %").arg(m_latestIlluminance));

        // 백엔드 외부 연동용 최종 시그널 방출 유지
        emit temperatureUpdated(m_latestTemperature);
        emit humidityUpdated(m_latestHumidity);
        emit distanceUpdated(m_latestDistance);
        emit illuminanceUpdated(m_latestIlluminance);
    }
}

// 5초 동안 실시간 데이터 수신 이벤트가 무반응일 때 처리 로직
void MainWindow::onM4WatchdogTimeout()
{
    if (m_isM4Connected) {
        m_isM4Connected = false;
        qDebug() << "경고: M4 장치 응답 타임아웃 발생";

        // 상태 표시창을 빨간색 오류 상태 텍스트로 즉각 제어
        ui->lblStatus->setText("오류: M4 연결 끊김 (5초 이상 무응답)");
        ui->lblStatus->setStyleSheet("color: red; font-weight: bold;");

        // 장비가 끊겼으므로 기존 센서 라벨 문구들을 초기화 유도
        ui->lblTemperature->setText("온도: 연결 유실");
        ui->lblHumidity->setText("습도: 연결 유실");
        ui->lblDistance->setText("거리: 연결 유실");
        ui->lblIlluminance->setText("조도: 연결 유실");

        emit m4ConnectionStatusChanged(false);
    }
}

// 푸시 버튼을 통한 모의 센서 데이터 대량 인입 주입 테스트 시나리오
void MainWindow::on_btnTest_clicked()
{
    qDebug() << "UI 테스트 버튼 액션 감지: 가상 센서값 생성 처리 시작";
    if(!m_serialPort->isOpen()) return;

    // 명세에 표기된 프로토콜 예시값을 버퍼로 전송하여 자가 루프백 테스트 유도
    m_serialPort->write("T0235"); m_serialPort->flush(); // 온도 23.5도 주입
    m_serialPort->write("H0678"); m_serialPort->flush(); // 습도 67.8퍼센트 주입
    m_serialPort->write("U0125"); m_serialPort->flush(); // 초음파 거리 125센티미터 주입
    m_serialPort->write("B0075"); m_serialPort->flush(); // 조도 75퍼센트 주입

    qDebug() << "자가 주입 결과: 내부 적재 완료됨. 최대 10초 이내에 UI 라벨 수치가 변경됩니다.";
}
