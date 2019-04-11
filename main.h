#include <string>
#include "mbed.h"
#include "Watchdog.h"
#include "GSM_MQTT.h"
#include "events/EventQueue.h"
#include "SDBlockDevice.h"
#include "FATFileSystem.h"
#include "TextLCD.h"

TextLCD lcd(PC_6, PC_4, PA_8, PA_7, PB_15, PB_13);

SDBlockDevice sd(PB_5, PB_4, PB_3, PC_7);
FATFileSystem fs("fs");

using namespace std;

static EventQueue ev_queue(100 * EVENTS_EVENT_SIZE);

DigitalOut simPower(PC_9);
DigitalOut simEn(PB_7, 1);
DigitalOut led0(PC_8);

AnalogIn S0(PA_0);
AnalogIn S1(PC_2);
AnalogIn S2(PC_0);
AnalogIn S3(PC_3);
AnalogIn S4(PC_1);
AnalogIn S5(PA_1);

RawSerial sim(PA_9, PA_10, 9600);
RawSerial pc(PA_2, PA_3, 9600);

Watchdog wd;
float sensorData[6];
void GSM_MQTT::OnConnect(void){}

GSM_MQTT MQTT(10);

extern char* username;
extern char* clientID;

const int PING_FAIL = -1;
const int CONNECTION_CLOSED = -2;
const int WATCHDOG = -3;
int lcdSensorLine = 0;
bool sdAvailable;
