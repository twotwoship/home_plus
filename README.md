# home_plus
home iot

온도 습도 측정함수
int temp_measurement(void); int 단위 235678(23.5도 67.8%)
초음파거리 측정함수
int ultra_sonic_measurement(void); int 단위 cm
조도 측정함수
int lumen__measurement(void); int 단위 %

전등 통제 함수
void led_control(int); / int 단위 0 off 1 on
DC 모터 통제 함수
void dc_motor_control(int); / int 단위 0 off 1 on
스탭 모터 통제 함수
void step_motor_control(int); / int 단위 0 ~ 100
알람 통제 함수
void alarm_control(int); / int 단위 0 off 1 on
