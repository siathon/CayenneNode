#include "main.h"
void ping(){
    MQTT._ping();
}

void subs(){
    // MQTT.subscribe(0, MQTT._generateMessageID(), "v1/990606a0-4e2b-11e9-9622-9b9aeccba453/things/ddf15790-4f9a-11e9-a6b5-e30ec853fbf2/cmd/2", 0);

}

void enablePing(){
    MQTT.pingFlag = true;
}

void readSensors(){
    sensorData[0] = (1.0 - S0.read()) * 100;
    sensorData[1] = (1.0 - S1.read()) * 100;
    sensorData[2] = (1.0 - S2.read()) * 100;
    sensorData[3] = (1.0 - S3.read()) * 100;
    sensorData[4] = (1.0 - S4.read()) * 100;
    sensorData[5] = (1.0 - S5.read()) * 100;
}

void publishSensorData(){
    if (!MQTT.available()) {
        return;
    }
    for (size_t i = 0; i < 6; i++) {
        wd.Service();
        pc.printf("publish channel %d = %.2f\n", i+1, sensorData[i]);
        sprintf(MQTT.Topic, "v1/%s/things/%s/data/%d", username, clientID, i+1);
        sprintf(MQTT.Message, "soil_moist,p=%f", sensorData[i]);
        MQTT.publish(0, 0, 0, MQTT._generateMessageID(), MQTT.Topic, MQTT.Message);
        wait_ms(400);
    }
}

void initSD(){
    pc.printf("Mounting SD...");
    int err = fs.mount(&sd);
    if (err) {
        pc.printf("Failed!\n");
        sdAvailable = false;
    }
    else{
        pc.printf("Done\n");
        sdAvailable = true;
    }
}

void writeSensorDataToSD(){
    if (!sdAvailable) {
        return;
    }
    pc.printf("Opening data file...");
    FILE *f = fopen("/fs/data.csv", "a");
    if (!f) {
        pc.printf("Failed!\n");
    }
    else{
        pc.printf("Done\n");
    }
    time_t seconds = time(NULL);
    char time[20];
    strftime(time, 32, "%Y-%m-%d %H:%M:%S", localtime(&seconds));
    for (size_t i = 0; i < 6; i++) {
        pc.printf("write to SD: %d, %.2f\n", i+1, sensorData[i]);
        fprintf(f, "%s,", time);
        fprintf(f, "%s,", clientID);
        fprintf(f, "%d,%.2f\n", i+1, sensorData[i]);
    }
    fclose(f);
}

void logError(int type){
    if (!sdAvailable) {
        return;
    }
    pc.printf("Opening log file...");
    FILE *f = fopen("/fs/errorLog.txt", "a");
    if (!f) {
        pc.printf("Failed!\n");
    }
    else{
        pc.printf("Done\n");
    }
    fprintf(f, "***************************************\n");
    time_t seconds = time(NULL);
    char time[20];
    strftime(time, 32, "%Y-%m-%d %H:%M:%S", localtime(&seconds));
    fprintf(f, "%s\n", time);
    switch (type) {
        case PING_FAIL:{
            pc.printf("Ping Fail Error\n");

            fprintf(f, "Ping fail error\n");
            break;
        }
        case CONNECTION_CLOSED:{
            pc.printf("TCP connection closed\n");
            fprintf(f, "TCP connection closed\n");
            break;
        }
        case WATCHDOG:{
            pc.printf("Watchdog caused reset\n");
            fprintf(f, "Watchdog caused reset\n");
            break;
        }
    }
    fclose(f);
}

void updateChannelsLCD(){
    lcd.locate(0, 0);
    switch (lcdSensorLine) {
        case 0:{
            if (sensorData[0] < 0.1) {
                lcd.printf("1:OFF    ");
            }
            else{
                lcd.printf("1:%2.2f  ", sensorData[0]);
            }
            if (sensorData[1] < 0.1) {
                lcd.printf("2:OFF    ");
            }
            else{
                lcd.printf("2:%2.2f", sensorData[1]);
            }
            break;
        }
        case 1:{
            if (sensorData[2] < 0.1) {
                lcd.printf("3:OFF    ");
            }
            else{
                lcd.printf("3:%2.2f  ", sensorData[2]);
            }
            if (sensorData[3] < 0.1) {
                lcd.printf("4:OFF    ");
            }
            else{
                lcd.printf("4:%2.2f", sensorData[3]);
            }
            break;
        }
        case 2:{
            if (sensorData[4] < 0.1) {
                lcd.printf("5:OFF    ");
            }
            else{
                lcd.printf("5:%2.2f  ", sensorData[4]);
            }
            if (sensorData[5] < 0.1) {
                lcd.printf("6:OFF    ");
            }
            else{
                lcd.printf("6:%2.2f", sensorData[5]);
            }
            break;
        }
    }
    lcdSensorLine++;
    if (lcdSensorLine == 3) {
        lcdSensorLine = 0;
    }
    if (MQTT.available()) {
        lcd.locate(0, 1);
        lcd.printf("                ");
        lcd.locate(0, 1);
        lcd.printf("CONNECTED");
    }
}

int main() {
    int temp = 45;
    int timer = 0;
    initSD();
    if (wd.WatchdogCausedReset()) {
        logError(WATCHDOG);
    }
    lcd.cls();
    readSensors();
    updateChannelsLCD();
    MQTT.begin();
    wd.Configured(3);
    wd.Service();
    ev_queue.call_every(10, checkSerial);
    ev_queue.call_in(3000, enablePing);
    ev_queue.call_every(1000, readSensors);
    ev_queue.call_every(7000, publishSensorData);
    ev_queue.call_every(6000, ping);
    ev_queue.call_every(10000, updateChannelsLCD);
    ev_queue.call_every(600000, writeSensorDataToSD);
    ev_queue.dispatch_forever();
}
