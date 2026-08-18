#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSerialPort>
#include <QByteArray>
#include <QTimer> // 타이머 제어를 위한 헤더 추가

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

signals:
    // UI 담당자가 받아서 화면에 텍스트나 그래프로 찍을 10초 주기 최종 신호들
    void temperatureUpdated(double value);
    void humidityUpdated(double value);
    void distanceUpdated(int value);
    void illuminanceUpdated(int value);

    // M4 보드 연결 상태 감시용 신호 (true: 정상 연결중, false: 5초간 통신 없음/끊김)
    void m4ConnectionStatusChanged(bool isConnected);
    void loopbackStatusReceived(QString message);

public slots:
    void initSerialPort(QString portName);
    void closeSerialPort();
    void sendLedCommand(bool on);
    void sendDcMotorCommand(bool on);
    void sendStepperCommand(int angle);
    void sendAlarmCommand(bool on);

private slots:
    void handleReadyRead();
    void on_btnTest_clicked();

    // [새로 추가] 10초마다 UI를 업데이트하기 위해 호출되는 함수
    void onUiUpdateTimerTimeout();

    // [새로 추가] M4 보드가 5초 동안 살아있는지 감시하는 함수
    void onM4WatchdogTimeout();

private:
    Ui::MainWindow *ui;
    QSerialPort *m_serialPort;
    QByteArray m_rxBuffer;

    // [중요] 실시간으로 들어오는 최신 센서 데이터를 임시 저장하는 내부 저장소 (버퍼 역할)
    double m_latestTemperature = 0.0;
    double m_latestHumidity = 0.0;
    int m_latestDistance = 0;
    int m_latestIlluminance = 0;

    // 타이머 객체 포인터 선언
    QTimer *m_uiUpdateTimer;  // 10초 주기 UI 갱신용
    QTimer *m_m4WatchdogTimer; // 5초 주기 생존 감시(Timeout)용

    bool m_isM4Connected = false; // 현재 M4 보드의 통신 연결 상태 플래그
};

#endif // MAINWINDOW_H




