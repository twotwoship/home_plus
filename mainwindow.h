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
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QDateTimeAxis>
#include <QtCharts/QValueAxis>

QT_CHARTS_USE_NAMESPACE

class QTimer;
class QListWidgetItem;
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

public slots:
    // 센서 값 표시 인터페이스. 실제 UART 프레임 파싱(팀원 3,4 담당)이
    // 값을 뽑아낸 뒤 이 함수들을 호출/연결해서 화면에 반영한다.
    void updateTemperature(double celsiusValue);
    void updateHumidity(double percentValue);
    // 초음파 거리값을 받아 doorClosedThresholdCm 기준으로 현관문 열림/닫힘을 판정해 표시한다.
    void updateDistance(double centimeterValue);
    void updateIlluminance(double percentValue);

protected:
    // 온도/습도/조도 카드 클릭을 감지하기 위한 이벤트 필터
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void onRefreshPorts();         // 새로고침 버튼: 사용 가능한 COM 포트 목록 갱신
    void onConnectClicked();       // 연결 버튼: 선택한 포트로 시리얼 연결 시도
    void onDisconnectClicked();    // 해제 버튼: 시리얼 연결 종료
    void onSerialReadyRead();      // 시리얼 데이터 수신 시 호출 (프레임 파싱은 팀원 3,4 담당)
    void onSerialErrorOccurred(QSerialPort::SerialPortError error); // 케이블 분리 등 물리적 단절 감지
    void onWatchdogTimeout();      // 1초마다 응답 없음(타임아웃) 여부 점검
    void onAlarmSetClicked();      // 설정 버튼: 선택한 시각을 알람 목록에 추가
    void onAlarmClearClicked();    // 해제 버튼: 선택한 알람을 목록에서 제거
    void onAlarmCheckTimeout();    // 1초마다 현재 시각이 등록된 알람 시각인지 확인
    void onAlarmOffTimeout();      // 알람 울림 지속시간이 지나면 A0000 전송 및 목록에서 자동 삭제
    void onChartSampleTimeout();   // 1분마다 최신 센서값을 기록 히스토리에 추가

private:
    enum class SensorType { None, Temperature, Humidity, Illuminance };

    Ui::MainWindow *ui;
    void toggleSensorCard(QWidget *card, const QString &title); // 센서 카드 선택/해제 및 로그 제목 갱신
    void setConnectedUiState(bool connected);   // 연결 상태에 따라 포트 선택/버튼 활성화 상태 전환
    void setConnStatusText(const QString &text, bool connected); // 상단 연결 상태 표시(점+텍스트) 갱신
    void teardownConnection(const QString &reason); // 포트를 닫고 연결 끊김 상태로 UI 갱신
    void sendCommand(const QByteArray &frame);  // 연결 중일 때만 5byte 명령 프레임 전송
    void refreshAlarmListWidget();              // alarmTimes를 기준으로 알람 목록 UI 다시 그리기
    SensorType sensorTypeForCard(QWidget *card) const;                        // 카드 위젯 → 센서 종류 변환
    void recordChartSample(QVector<QPointF> &history, bool hasValue, double value, qint64 nowMs); // 기록 히스토리에 1건 추가 (최근 N개 유지)
    void refreshChartDisplay();   // 선택된 센서의 기록을 그래프에 그리기 (Y축은 센서별 고정 범위)
    void resetChartHistory();     // 재연결 시 기록 히스토리 초기화

    QSerialPort *serialPort;      // M4와의 UART 통신에 사용하는 시리얼 포트
    QTimer *watchdogTimer;        // 논리적 단절(응답 없음) 감지용 타이머
    QElapsedTimer lastRxTimer;    // 마지막 수신 시각으로부터 경과 시간 측정

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
    static constexpr int alarmDurationMs = 5000;

    static constexpr int chartSampleIntervalMs = 60000; // 그래프 기록 샘플링 주기 (1분)
    // 그래프에 보관할 최대 샘플 개수. "일단 최근 N개만 유지"로 정한 임시값이며 추후 조정 가능.
    static constexpr int chartHistoryMaxPoints = 60;

    static constexpr double temperatureAxisMin = 0.0;
    static constexpr double temperatureAxisMax = 50.0;
    static constexpr double humidityAxisMin = 20.0;
    static constexpr double humidityAxisMax = 90.0;
    static constexpr double illuminanceAxisMin = 0.0;
    static constexpr double illuminanceAxisMax = 100.0;
};

#endif // MAINWINDOW_H