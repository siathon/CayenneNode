#include <string>
#include "mbed.h"
#include "Watchdog.h"
#include "GSM_MQTT.h"
#include "events/EventQueue.h"
#include "SDBlockDevice.h"
#include "FATFileSystem.h"
#include "TextLCD.h"
#include <math.h>
#include "DHT.h"

TextLCD lcd(PC_6, PC_4, PA_8, PA_7, PB_15, PB_13);

SDBlockDevice sd(PB_5, PB_4, PB_3, PC_7);
FATFileSystem fs("fs");

DHT dth(PC_3, DHT22);

using namespace std;

static EventQueue ev_queue(100 * EVENTS_EVENT_SIZE);

DigitalOut simPower(PC_9);
DigitalOut simEn(PB_7, 1);
DigitalOut led0(PC_8);
DigitalOut battLED(PB_14, 1);
DigitalOut powerLED(PA_15, 1);

AnalogIn S0(PA_6);//PA_0
AnalogIn S1(PC_1);//PC_2
AnalogIn S2(PA_1);//PC_0
AnalogIn S3(PC_0);//PA_1
AnalogIn S4(PC_2);//PC_1
AnalogIn S5(PA_0);//PC_3

AnalogIn battLevel(PB_0);
DigitalIn powerSupply(PB_2);

RawSerial sim(PA_9, PA_10, 9600);
RawSerial pc(PA_2, PA_3, 9600);

Watchdog wd;
float rawSensorData[6];
float sensorData[6];
float sensorMin[6] = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
float sensorMax[6] = {1.0, 1.0, 1.0, 1.0, 1.0, 1.0};
float mn[6] = {5.0, 5.0, 5.0, 5.0, 5.0, 5.0};
float mx[6] = {100.0, 100.0, 100.0, 100.0, 100.0, 100.0};
int sensorCh[6] = {21, 22, 23, 24, 25, 26};
int minmaxCh[6] = {11, 12, 13, 14, 15, 16};
int battery;
float hmd = 30.0;
float tmp = 25.0;
void GSM_MQTT::OnConnect(void){}

GSM_MQTT MQTT(10);

extern char* username;
extern char* clientID;

const int PING_FAIL = -1;
const int CONNECTION_CLOSED = -2;
const int WATCHDOG = -3;
int lcdSensorLine = 0;
int activeSensor = 0;
bool sdAvailable;
int publishState = 0;
int mxState = 0;
int mxCnt = 0;
bool mxswt = true;

int stringToInt(string str){
    double t = 0;
    int l = str.length();
    for(int i = l-1; i >= 0; i--)
        t += (str[i] - '0') * pow(10.0, l - i - 1);
    return (int)t;
}
