# HomePlus Qt 클라이언트 구조

담당: 팀원2 (이호윤) · 대상: 팀원 1, 3, 4 · 범위: `mainwindow.h` / `mainwindow.cpp` / `mainwindow.ui`

이 문서는 현재 Qt 클라이언트가 무엇을 하고, 화면이 어떻게 구성되어 있고, UART 담당(팀원 3, 4)이 어디에 코드를 꽂아 넣으면 되는지를 정리한 것입니다.

---

## 1. 파일 구성

| 파일 | 역할 |
| --- | --- |
| `main.cpp` | 애플리케이션 엔트리포인트, `MainWindow` 생성 |
| `mainwindow.h` | `MainWindow` 클래스 선언 — 슬롯, 멤버 변수, 상수 |
| `mainwindow.cpp` | 실제 동작 구현 |
| `mainwindow.ui` | Qt Designer로 만든 화면 레이아웃 (XML) |
| `home_plus.pro` | qmake 프로젝트 파일. `QT += widgets serialport` |

---

## 2. 화면 구성

```
MainWindow
├─ headerBar                         (포트 선택 · 연결 상태)
│   ├─ appTitleLabel  "HOMEPLUS"
│   ├─ portCombo                     COM 포트 목록
│   ├─ refreshButton  "새로고침"
│   ├─ connectButton  "연결"
│   ├─ disconnectButton "해제"
│   └─ connStatusLabel               ●연결됨 / ●연결 안 됨
│
└─ bodySplitter
    ├─ mainColumn
    │   ├─ sensorGroupBox "실내 환경"
    │   │   ├─ temperatureCard  (클릭 가능) → temperatureValueLabel
    │   │   ├─ humidityCard     (클릭 가능) → humidityValueLabel
    │   │   ├─ illuminanceCard  (클릭 가능) → illuminanceValueLabel
    │   │   └─ doorCard                     → doorStateLabel (열림/닫힘)
    │   └─ detailGroupBox "로그"
    │       ├─ chartTitleLabel           카드 선택 안내 문구
    │       └─ chartContainer            (현재 비어있음, 그래프 기능은 보류)
    │
    └─ sidePanel
        ├─ controlGroupBox "장치 제어"
        │   ├─ 전등  ledOnButton / ledOffButton
        │   ├─ 제습  dehumidifierOnButton / dehumidifierOffButton
        │   └─ 창문  windowCloseButton / window25/50/75/100Button
        └─ alarmGroupBox "알람"
            ├─ alarmTimeEdit           시각 선택
            ├─ alarmSetButton "설정" / alarmClearButton "해제"
            └─ alarmListWidget         등록된 알람 목록 (스크롤, 다중 선택 가능)
```

온도/습도/조도 카드는 클릭하면 선택 표시되고 `chartTitleLabel`이 바뀌지만, 그래프 자체는 **아직 미구현 상태로 보류**되어 있습니다(로그 영역은 빈 틀만 있음).

---

## 3. 연결 관리 (팀원2 담당 영역)

- `onRefreshPorts()` — `QSerialPortInfo::availablePorts()`로 COM 포트 목록 갱신
- `onConnectClicked()` — 선택한 포트를 **115200bps, 8N1, 흐름제어 없음**으로 오픈
- `onDisconnectClicked()` — 사용자가 직접 연결 해제
- `onSerialErrorOccurred()` — `ResourceError` / `DeviceNotFoundError` 발생 시 물리적 단절로 판단
- `watchdogTimer` — 1초마다(`watchdogIntervalMs`) 마지막 수신 이후 `linkTimeoutMs`(10초) 초과 여부 확인 → 논리적 단절(응답 없음) 판단
- `teardownConnection()` — 위 두 가지 단절 상황에서 공통으로 포트를 닫고 UI를 "연결 끊김" 상태로 되돌리는 함수

즉 **포트 열기/닫기, 연결 상태 감시**까지는 완성되어 있고, **수신 데이터 자체의 해석(프레임 파싱)은 포함되어 있지 않습니다.**

---

## 4. 센서 값 표시 — UART 팀이 호출할 인터페이스

아래 4개는 `public slots`로 열려 있는, 이미 완성된 "표시 담당" 함수입니다. UART 파싱 코드가 값을 뽑아낸 뒤 그대로 호출하면 됩니다.

```cpp
void updateTemperature(double celsiusValue);   // 온도 카드 갱신
void updateHumidity(double percentValue);      // 습도 카드 갱신
void updateDistance(double centimeterValue);   // doorClosedThresholdCm(10cm) 기준으로 열림/닫힘 판정 후 표시
void updateIlluminance(double percentValue);   // 조도 카드 갱신
```

**아직 비어 있는 부분**: `onSerialReadyRead()`가 `serialPort->readAll()`로 받은 바이트를 워치독 타이머 갱신에만 쓰고 버립니다. `T0235`처럼 들어온 프레임을 해석해서 위 4개 함수를 호출하는 코드가 UART 담당자가 채워야 할 부분입니다.

```cpp
void MainWindow::onSerialReadyRead()
{
    // 프레임 파싱은 팀원 3,4(UART·시스템 통합) 담당.
    serialPort->readAll();
    lastRxTimer.restart();
}
```

---

## 5. 장치 제어 — 이미 완성된 부분

버튼을 누르면 `sendCommand()`로 5byte 프레임(명령 1byte + 데이터 4byte)을 바로 전송합니다. **ACK/ERR 응답 대기 없이 전송만** 합니다.

| 버튼 | 전송 프레임 |
| --- | --- |
| 전등 ON / OFF | `L0001` / `L0000` |
| 제습 ON / OFF | `D0001` / `D0000` |
| 창문 닫기 / 25% / 50% / 75% / 100% | `S0000` / `S0025` / `S0050` / `S0075` / `S0100` |
| 알람 시작 / 종료 | `A0001` / `A0000` (버튼이 아니라 알람 타이머가 자동 전송) |

`sendCommand()`는 포트가 열려 있을 때만 전송하고, 닫혀 있으면 아무 것도 하지 않습니다.

---

## 6. 알람 기능

- 여러 개의 알람을 동시에 등록 가능 (`QList<QTime> alarmTimes`)
- **설정**: `alarmTimeEdit`의 시각을 목록에 추가 (중복 무시, 자동 정렬) → `alarmListWidget`에 표시
- **감시**: `alarmCheckTimer`가 1초마다(정각일 때만) 현재 시각과 일치하는 알람이 있는지 확인
- **울림**: 일치하면 `A0001` 전송 → 목록 항목에 "(울리는 중)" 표시 → `alarmDurationMs`(5초) 후 `A0000` 전송 + 해당 알람 목록에서 자동 삭제 (1회성)
- **해제**: 목록에서 항목을 선택하고 해제 버튼을 누르면 그 알람만 제거. 울리는 중인 알람을 해제하면 즉시 `A0000` 전송

---

## 7. 주요 상수 (`mainwindow.h`)

| 상수 | 값 | 의미 |
| --- | --- | --- |
| `watchdogIntervalMs` | 1000ms | 연결 상태 점검 주기 |
| `linkTimeoutMs` | 10000ms | 이 시간 동안 수신이 없으면 논리적 단절로 판단 |
| `doorClosedThresholdCm` | 10.0cm | 이 값 이하면 현관문 "닫힘" 판정 — **임시값, 팀 확정 필요** |
| `alarmCheckIntervalMs` | 1000ms | 알람 시각 확인 주기 |
| `alarmDurationMs` | 5000ms | 알람이 울리는 지속 시간 (A0001→A0000 간격) |

---

## 8. UART 담당(팀원 3,4)이 추가로 구현해야 할 것

1. **수신 프레임 파싱** — `onSerialReadyRead()`에서 5byte 단위로 버퍼링·분리
2. **잘못된/깨진 프레임 처리** — 형식이 안 맞으면 폐기하고 재동기화
3. **파싱 결과를 4번 항목의 슬롯에 전달** — `updateTemperature/Humidity/Distance/Illuminance` 호출
4. **명령 전송에 대한 ACK/ERR 응답 처리** — 지금은 쏘기만 하고 확인하지 않음

나머지(연결 관리, 장치 제어 명령 전송, 알람, 화면 표시)는 이미 동작하는 상태입니다.
