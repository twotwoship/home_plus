#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSerialPort>
#include <QElapsedTimer>
#include <QTime>
#include <QList>
#include <QVector>
#include <QPointF>
#include <QDateTime>
#include <QByteArray> // [UART] rxBuffer/프레임 파싱용
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QDateTimeAxis>
#include <QtCharts/QValueAxis>

QT_CHARTS_USE_NAMESPACE

class QTimer;
class QListWidgetItem;
class QListWidget;
class QLabel;

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
    // [UART] UART 담당(팀원 3,4)이 추가한 신호. 다른 모듈이 구독할 수 있도록 공개.
    // 온도/습도/조도는 5초 주기(sensorFlushIntervalMs)로, 거리(현관문)는 실시간으로 emit 된다.
    void temperatureUpdated(double value);
    void humidityUpdated(double value);
    void distanceUpdated(int value);
    void illuminanceUpdated(int value);
    void m4ConnectionStatusChanged(bool isConnected); // [UART] 포트 open 여부가 아니라 실제 M4 응답 여부
    void loopbackStatusReceived(QString message);     // [UART] L/D/S/A 명령에 대한 M4의 에코 피드백

public slots:
    // 센서 값 표시 인터페이스. 파싱된 값을 받아 화면에 반영한다.
    void updateTemperature(double celsiusValue);
    void updateHumidity(double percentValue);
    // 초음파 거리값을 받아 doorClosedThresholdCm 기준으로 현관문 열림/닫힘을 판정해 표시한다.
    void updateDistance(double centimeterValue);
    void updateIlluminance(double percentValue);

    // [UART] 시리얼 포트 열기/닫기. 연결/해제 버튼과 외부 호출 양쪽에서 재사용
    void initSerialPort(const QString &portName);
    void closeSerialPort();

    // [UART] 제어 명령 전송 함수. 다른 모듈에서도 직접 호출 가능하도록 공개 슬롯으로 노출
    void sendLedCommand(bool on);
    void sendDcMotorCommand(bool on);
    void sendStepperCommand(int percent);
    void sendAlarmCommand(bool on);

protected:
    // 온도/습도/조도 카드 클릭을 감지하기 위한 이벤트 필터
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void onRefreshPorts();         // 새로고침 버튼: 사용 가능한 COM 포트 목록 갱신
    void onConnectClicked();       // 연결 버튼: 선택한 포트로 initSerialPort() 호출
    void onDisconnectClicked();    // 해제 버튼: closeSerialPort() 호출
    void onSerialReadyRead();      // [UART] 시리얼 데이터 수신 -> 버퍼링 + 5byte 프레임 파싱
    void onSerialErrorOccurred(QSerialPort::SerialPortError error); // 케이블 분리 등 물리적 단절 감지
    void onWatchdogTimeout();      // 1초마다 응답 없음(타임아웃) 여부 점검
    void onAlarmSetClicked();      // 설정 버튼: 선택한 시각을 알람 목록에 추가
    void onAlarmClearClicked();    // 해제 버튼: 선택한 알람을 목록에서 제거
    void onAlarmCheckTimeout();    // 1초마다 현재 시각이 등록된 알람 시각인지 확인
    void onAlarmOffTimeout();      // 알람 울림 지속시간이 지나면 A0000 전송 및 목록에서 자동 삭제
    void onChartSampleTimeout();   // 1분마다 최신 센서값을 기록 히스토리에 추가
    void onSensorFlushTimeout();   // [UART] 5초마다 누적된 온도/습도/조도 값을 화면과 외부 신호로 반영

private:
    enum class SensorType { None, Temperature, Humidity, Illuminance, Door };

    Ui::MainWindow *ui;
    void toggleSensorCard(QWidget *card, const QString &title); // 센서 카드 선택/해제 및 로그 제목 갱신
    void setConnectedUiState(bool connected);   // 연결 상태에 따라 포트 선택/버튼 활성화 상태 전환
    void setConnStatusText(const QString &text, bool connected); // 상단 연결 상태 표시(점+텍스트) 갱신
    void teardownConnection(const QString &reason); // 포트를 닫고 연결 끊김 상태로 UI 갱신
    void resetSensorLabels(); // 실내 환경 카드를 초기 상태(--)로 되돌림
    void sendCommand(const QByteArray &frame);  // 연결 중일 때만 5byte 명령 프레임 전송
    void refreshAlarmListWidget();              // alarmTimes를 기준으로 알람 목록 UI 다시 그리기
    SensorType sensorTypeForCard(QWidget *card) const;                        // 카드 위젯 → 센서 종류 변환
    void recordChartSample(QVector<QPointF> &history, bool hasValue, double value, qint64 nowMs); // 기록 히스토리에 1건 추가 (최근 N개 유지)
    void refreshChartDisplay();   // 선택된 센서의 기록을 그래프에 그리기 (Y축은 센서별 고정 범위). 현관문 선택 시엔 출입 로그 목록을 대신 표시
    void resetChartHistory();     // 재연결 시 기록 히스토리 초기화
    void refreshDoorLogWidget();  // doorOpenLog를 바탕으로 출입 로그 목록 위젯을 다시 그림
    void processIncomingFrame(const QByteArray &frame); // [UART] 5byte 프레임 1개 해석
    bool isFrameCommandChar(char cmd) const; // 프레임 재동기화용 - 유효한 명령 문자인지 확인

    QSerialPort *serialPort;      // M4와의 UART 통신에 사용하는 시리얼 포트
    QTimer *watchdogTimer;        // 논리적 단절(응답 없음) 감지용 타이머
    QElapsedTimer lastRxTimer;    // 마지막 수신 시각으로부터 경과 시간 측정

    QByteArray rxBuffer;          // [UART] 수신 프레임 조립 버퍼 (5byte 고정 프레임)
    bool linkConfirmed = false;   // [UART] 포트가 열린 것과 별개로, 실제로 M4로부터 유효 프레임을 받았는지 여부

    // [UART] 5초 주기 화면 반영을 위한 최신 센서값 임시 저장소 (온도/습도/조도)
    double pendingTemperature = 0.0;
    double pendingHumidity = 0.0;
    int pendingIlluminance = 0;
    QTimer *sensorFlushTimer;     // [UART] 5초 주기로 pendingX 값을 화면에 반영하는 타이머

    QTimer *alarmCheckTimer;      // 알람 시각 도달 여부를 주기적으로 확인하는 타이머
    QTimer *alarmOffTimer;        // 알람이 울린 뒤 A0000을 보낼 때까지 대기하는 싱글샷 타이머
    QList<QTime> alarmTimes;      // 등록된 알람 시각 목록
    QTime ringingAlarmTime;       // 현재 울리고 있는 알람 시각 (없으면 무효값)

    QChart *chart;                // 로그 영역에 표시되는 차트 객체
    QChartView *chartView;        // chart를 그리는 위젯 (chartContainer에 삽입)
    QLineSeries *chartSeries;     // 현재 선택된 센서의 기록을 그리는 선 그래프
    QDateTimeAxis *chartAxisX;    // X축: 실제 시각(HH:mm)
    QValueAxis *chartAxisY;       // Y축: 센서별 고정 범위
    QTimer *chartSampleTimer;     // 1분마다 최신값을 기록하는 타이머 (카드 선택 여부와 무관하게 상시 동작)
    QLabel *chartEmptyLabel;      // 선택은 됐지만 기록이 없을 때 표시하는 안내 라벨
    SensorType selectedSensor = SensorType::None; // 현재 로그 영역에 표시 중인 센서

    QListWidget *doorLogListWidget; // 현관문 카드 선택 시 표시되는 출입 로그 목록 (chartContainer에 삽입)
    QList<QDateTime> doorOpenLog;   // 닫힘 -> 열림으로 바뀐 시각 목록 (최신순으로 표시)
    bool doorStateKnown = false;    // 직전 문 상태를 알고 있는지 (연결 직후 첫 값은 전환으로 취급하지 않음)
    bool doorClosed = true;         // 직전에 판정된 문 상태

    double latestTemperature = 0.0;
    double latestHumidity = 0.0;
    double latestIlluminance = 0.0;
    bool hasTemperature = false;  // 온도값을 한 번이라도 받았는지
    bool hasHumidity = false;
    bool hasIlluminance = false;

    QVector<QPointF> temperatureHistory; // x: 시각(epoch ms), y: 값. 최근 chartHistoryMaxPoints개만 유지
    QVector<QPointF> humidityHistory;
    QVector<QPointF> illuminanceHistory;

    static constexpr int watchdogIntervalMs = 1000;
    // 논리적 단절(케이블은 연결되어 있지만 M4가 응답 없음) 판단 임계값.
    // 팀 내 UART 하트비트/타임아웃 정책이 확정되면 조정 필요.
    static constexpr int linkTimeoutMs = 10000;
    // 현관문 열림/닫힘 판정 임계 거리(cm). README상 판정 기준 미정 상태의 임시값이며
    // 팀 내 기준이 확정되면 조정 필요.
    static constexpr double doorClosedThresholdCm = 10.0;
    static constexpr int alarmCheckIntervalMs = 1000;
    // 알람 A0001 전송 후 A0000을 보내기까지의 지속 시간. 임의값이며 추후 변경 가능.
    static constexpr int alarmDurationMs = 1000;
    // [UART] 온도/습도/조도를 pendingX에서 화면으로 몰아서 반영하는 주기 (5초)
    static constexpr int sensorFlushIntervalMs = 5000;

    // TEMP TEST: 원래 60000(1분). 테스트 끝나면 60000으로 되돌릴 것.
    static constexpr int chartSampleIntervalMs = 10000; // 그래프 기록 샘플링 주기 (1분)
    // 그래프에 보관할 최대 샘플 개수. "일단 최근 N개만 유지"로 정한 임시값이며 추후 조정 가능.
    static constexpr int chartHistoryMaxPoints = 60;
    static constexpr int doorLogMaxEntries = 50; // 출입 로그에 보관할 최대 개수

    static constexpr double temperatureAxisMin = 0.0;
    static constexpr double temperatureAxisMax = 50.0;
    static constexpr double humidityAxisMin = 20.0;
    static constexpr double humidityAxisMax = 90.0;
    static constexpr double illuminanceAxisMin = 0.0;
    static constexpr double illuminanceAxisMax = 100.0;
};

#endif // MAINWINDOW_H