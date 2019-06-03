#include "main.h"
void publishDHT();
void ping(){
    MQTT._ping();
}

void subs(){
    MQTT.subscribe(0, MQTT._generateMessageID(), "v1/abab39c0-7d28-11e9-94e9-493d67fd755e/things/7a891260-7d2b-11e9-9636-f9904f7b864b/cmd/1", 0);
    MQTT.subscribe(0, MQTT._generateMessageID(), "v1/abab39c0-7d28-11e9-94e9-493d67fd755e/things/7a891260-7d2b-11e9-9636-f9904f7b864b/cmd/2", 0);
    MQTT.subscribe(0, MQTT._generateMessageID(), "v1/abab39c0-7d28-11e9-94e9-493d67fd755e/things/7a891260-7d2b-11e9-9636-f9904f7b864b/cmd/7", 0);
    MQTT.subscribe(0, MQTT._generateMessageID(), "v1/abab39c0-7d28-11e9-94e9-493d67fd755e/things/7a891260-7d2b-11e9-9636-f9904f7b864b/cmd/8", 0);
    MQTT.subscribe(0, MQTT._generateMessageID(), "v1/abab39c0-7d28-11e9-94e9-493d67fd755e/things/7a891260-7d2b-11e9-9636-f9904f7b864b/cmd/9", 0);

}

void enablePing(){
    MQTT.pingFlag = true;
}

void mapSeneorData() {
    for (size_t i = 0; i < 6; i++) {
        if (sensorMin[i] == sensorMax[i]) {
            sensorMax[i] += 0.01;
        }
        sensorData[i] = (rawSensorData[i] - sensorMin[i]) * (1.0 / (sensorMax[i] - sensorMin[i])) * 100;
        if (sensorData[i] > 100.0) {
            sensorData[i] = 100.0;
        }
        else if(sensorData[i] < 0.0){
            sensorData[i] = 0.0;
        }
    }
}

void readSensors(){
    rawSensorData[0] = 1.0 - S0.read();
    rawSensorData[1] = 1.0 - S1.read();
    rawSensorData[2] = 1.0 - S2.read();
    rawSensorData[3] = 1.0 - S3.read();
    rawSensorData[4] = 1.0 - S4.read();
    rawSensorData[5] = 1.0 - S5.read();
    mapSeneorData();
    if (dth.readData() == ERROR_NONE) {
        hmd = dth.ReadHumidity();
        tmp = dth.ReadTemperature(CELCIUS);
        // pc.printf("humidity = %2.2f - temp = %2.2f\n", hmd, tmp);
    }
}

void battLedOFF() {
    battLED = 1;
}

void checkBattery() {
    battery = ceil((battLevel.read() - 0.6) * 435);
    pc.printf("Battery level = %d%%\n", battery);
    if (powerSupply) {
        powerLED = 0;
        battLED = 1;
    }
    else{
        powerLED = 1;
        battLED = 0;
        ev_queue.call_in(100, battLedOFF);
    }
}

void publishSensorData(){
    if (MQTT.publishFailCount >= 10) {
        wait(4);
    }

    if (!MQTT.available() || !MQTT.TCP_Flag) {
        MQTT.publishFailCount++;
        return;
    }

    wd.Service();
    pc.printf("Publish channel %d = %.2f\n", publishState+1, sensorData[publishState]);
    sprintf(MQTT.Topic, "v1/%s/things/%s/data/%d", username, clientID, sensorCh[publishState]);
    sprintf(MQTT.Message, "soil_moist,p=%f", sensorData[publishState]);
    MQTT.publish(0, 1, 0, MQTT._generateMessageID(), MQTT.Topic, MQTT.Message);
    MQTT.publishFailCount++;
    publishState++;
    if (publishState == 6) {
        ev_queue.call_in(1000, publishDHT);
        publishState = 0;
    }
    else{
        ev_queue.call_in(1000, publishSensorData);
    }
}

void publishBatt(){
    wd.Service();
    pc.printf("Publish battery = %2d\n", battery);
    sprintf(MQTT.Topic, "v1/%s/things/%s/data/19", username, clientID);
    sprintf(MQTT.Message, "batt,p=%d", battery);
    MQTT.publish(0, 1, 0, MQTT._generateMessageID(), MQTT.Topic, MQTT.Message);
    MQTT.publishFailCount++;
    ev_queue.call_in(5000, publishSensorData);
}

void publishDHT(){
    wd.Service();
    pc.printf("Publish temperature = %2.2f\n", tmp);
    sprintf(MQTT.Topic, "v1/%s/things/%s/data/17", username, clientID);
    sprintf(MQTT.Message, "temp,c=%2.2f", tmp);
    MQTT.publish(0, 1, 0, MQTT._generateMessageID(), MQTT.Topic, MQTT.Message);
    MQTT.publishFailCount++;
    wd.Service();
    pc.printf("Publish humidity = %2.2f\n", hmd);
    sprintf(MQTT.Topic, "v1/%s/things/%s/data/18", username, clientID);
    sprintf(MQTT.Message, "rel_hum,p=%2.2f", hmd);
    MQTT.publish(0, 1, 0, MQTT._generateMessageID(), MQTT.Topic, MQTT.Message);
    MQTT.publishFailCount++;
    ev_queue.call_in(1000, publishBatt);
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
        pc.printf("Failed!\r\n");
    }
    else{
        pc.printf("Done\r\n");
    }
    time_t seconds = time(NULL);
    char tm[20];
    strftime(tm, 32, "%Y-%m-%d %H:%M:%S", localtime(&seconds));
    for (size_t i = 0; i < 6; i++) {
        pc.printf("write to SD: %d, %.2f\n", i+1, sensorData[i]);
        fprintf(f, "%s,", tm);
        fprintf(f, "%s,", clientID);
        fprintf(f, "%d,%.2f\r\n", i+1, sensorData[i]);
    }
    fclose(f);
    // pc.printf("Opening battery file...");
    // f = fopen("/fs/battery.csv", "a");
    // if (!f) {
    //     pc.printf("Failed!\r\n");
    // }
    // else{
    //     pc.printf("Done\r\n");
    // }
    // pc.printf("%s, %f\r\n", tm, battery);
    // fprintf(f, "%s, %f\r\n", tm, battery);
    // fclose(f);
}

void logError(int type){
    if (!sdAvailable) {
        return;
    }
    pc.printf("Opening log file...");
    FILE *f = fopen("/fs/errorLog.txt", "a");
    if (!f) {
        pc.printf("Failed!\r\n");
    }
    else{
        pc.printf("Done\r\n");
    }
    fprintf(f, "***************************************\r\n");
    time_t seconds = time(NULL);
    char time[20];
    strftime(time, 32, "%Y-%m-%d %H:%M:%S", localtime(&seconds));
    fprintf(f, "%s\r\n", time);
    switch (type) {
        case PING_FAIL:{
            pc.printf("Ping Fail Error\n");

            fprintf(f, "Ping fail error\r\n");
            break;
        }
        case CONNECTION_CLOSED:{
            pc.printf("TCP connection closed\n");
            fprintf(f, "TCP connection closed\r\n");
            break;
        }
        case WATCHDOG:{
            pc.printf("Watchdog caused reset\n");
            fprintf(f, "Watchdog caused reset\r\n");
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
        lcd.printf("CONNECTED   ");
    }
    else if (MQTT.TCP_Flag) {
        lcd.locate(0, 1);
        lcd.printf("GSM READY   ");
    }
    else{
        lcd.locate(0, 1);
        lcd.printf("NOT READY!  ");
    }

    lcd.locate(12, 1);
    lcd.printf("%3d%%", battery);
}

void onMessage(int channel, string msgID, string value){
    switch (channel) {
        wd.Service();
        case 1:{
            if (activeSensor == 0) {
                sprintf(MQTT.Topic, "v1/%s/things/%s/response", username, clientID);
                sprintf(MQTT.Message, "error,%s=Set_channel_number!", msgID);
                MQTT.publish(0, 0, 0, MQTT._generateMessageID(), MQTT.Topic, MQTT.Message);
                break;
            }
            sprintf(MQTT.Topic, "v1/%s/things/%s/response", username, clientID);
            sprintf(MQTT.Message, "ok,%s", msgID);
            MQTT.publish(0, 0, 0, MQTT._generateMessageID(), MQTT.Topic, MQTT.Message);
            float f = atof(value.c_str());
            pc.printf("channel %d min = %f\n", activeSensor, f);
            mn[activeSensor - 1] = f;
            break;
        }
        case 2:{
            if (activeSensor == 0) {
                sprintf(MQTT.Topic, "v1/%s/things/%s/response", username, clientID);
                sprintf(MQTT.Message, "error,%s=Set_channel_number!", msgID);
                MQTT.publish(0, 0, 0, MQTT._generateMessageID(), MQTT.Topic, MQTT.Message);
                break;
            }
            sprintf(MQTT.Topic, "v1/%s/things/%s/response", username, clientID);
            sprintf(MQTT.Message, "ok,%s", msgID);
            MQTT.publish(0, 0, 0, MQTT._generateMessageID(), MQTT.Topic, MQTT.Message);
            float f = atof(value.c_str());
            pc.printf("channel %d max = %f\n", activeSensor, f);
            mx[activeSensor - 1] = f;
            break;
        }
        case 9:{
            sprintf(MQTT.Topic, "v1/%s/things/%s/response", username, clientID);
            sprintf(MQTT.Message, "error,%s=Channel number not in range!", msgID);
            int v = stringToInt(value);
            if ( v >= 1 && v <= 6) {
                activeSensor = v;
                sprintf(MQTT.Message, "ok,%s", msgID);
                pc.printf("Set channel %d active\n", v);
            }
            MQTT.publish(0, 0, 0, MQTT._generateMessageID(), MQTT.Topic, MQTT.Message);

            sprintf(MQTT.Topic, "v1/%s/things/%s/data/8", username, clientID);
            sprintf(MQTT.Message, "%s", "1");
            MQTT.publish(0, 0, 0, MQTT._generateMessageID(), MQTT.Topic, MQTT.Message);

            sprintf(MQTT.Topic, "v1/%s/things/%s/data/7", username, clientID);
            sprintf(MQTT.Message, "%s", "1");
            MQTT.publish(0, 0, 0, MQTT._generateMessageID(), MQTT.Topic, MQTT.Message);
            pc.printf("Setting enabled\n");
            break;
        }
        case 7:{
            int v = stringToInt(value);
            if (v == 1 || activeSensor == 0) {
                sprintf(MQTT.Topic, "v1/%s/things/%s/data/7", username, clientID);
                sprintf(MQTT.Message, "%s", "0");
                MQTT.publish(0, 0, 0, MQTT._generateMessageID(), MQTT.Topic, MQTT.Message);
                sprintf(MQTT.Topic, "v1/%s/things/%s/response", username, clientID);
                sprintf(MQTT.Message, "error,%s=Set_channel_number!", msgID);
                MQTT.publish(0, 0, 0, MQTT._generateMessageID(), MQTT.Topic, MQTT.Message);
                break;
            }
            sprintf(MQTT.Topic, "v1/%s/things/%s/response", username, clientID);
            sprintf(MQTT.Message, "ok,%s", msgID);
            MQTT.publish(0, 0, 0, MQTT._generateMessageID(), MQTT.Topic, MQTT.Message);
            readSensors();
            sensorMin[activeSensor - 1] = rawSensorData[activeSensor - 1] - 0.001;
            pc.printf("Set channel %d minimum = %2.2f\n", activeSensor, rawSensorData[activeSensor - 1]);
            sprintf(MQTT.Topic, "v1/%s/things/%s/data/7", username, clientID);
            sprintf(MQTT.Message, "%s", "0");
            MQTT.publish(0, 0, 0, MQTT._generateMessageID(), MQTT.Topic, MQTT.Message);
            break;
        }
        case 8:{
            int v = stringToInt(value);
            if (v == 1 || activeSensor == 0) {
                sprintf(MQTT.Topic, "v1/%s/things/%s/data/8", username, clientID);
                sprintf(MQTT.Message, "%s", "0");
                MQTT.publish(0, 0, 0, MQTT._generateMessageID(), MQTT.Topic, MQTT.Message);
                sprintf(MQTT.Topic, "v1/%s/things/%s/response", username, clientID);
                sprintf(MQTT.Message, "error,%s=Set_channel_number!", msgID);
                MQTT.publish(0, 0, 0, MQTT._generateMessageID(), MQTT.Topic, MQTT.Message);
                break;
            }
            sprintf(MQTT.Topic, "v1/%s/things/%s/response", username, clientID);
            sprintf(MQTT.Message, "ok,%s", msgID);
            MQTT.publish(0, 0, 0, MQTT._generateMessageID(), MQTT.Topic, MQTT.Message);
            readSensors();
            sensorMax[activeSensor - 1] = rawSensorData[activeSensor - 1] + 0.001;
            pc.printf("Set channel %d maximum = %2.2f\n", activeSensor, rawSensorData[activeSensor - 1]);
            sprintf(MQTT.Topic, "v1/%s/things/%s/data/8", username, clientID);
            sprintf(MQTT.Message, "%s", "0");
            MQTT.publish(0, 0, 0, MQTT._generateMessageID(), MQTT.Topic, MQTT.Message);
            break;
        }
    }
}

void publishMinMax() {
    wd.Service();
    sprintf(MQTT.Topic, "v1/%s/things/%s/data/%d", username, clientID, minmaxCh[mxState]);
    if (mxswt) {
        sprintf(MQTT.Message, "soil_moist,p=%f", mn[mxState]);
        pc.printf("publish minmux channel %d = %f\n", mxState + 1, mn[mxState]);
        MQTT.publish(0, 1, 0, MQTT._generateMessageID(), MQTT.Topic, MQTT.Message);
    } else {
        sprintf(MQTT.Message, "soil_moist,p=%f", mx[mxState]);
        pc.printf("publish minmux channel %d = %f\n",mxState + 1, mx[mxState]);
        MQTT.publish(0, 1, 0, MQTT._generateMessageID(), MQTT.Topic, MQTT.Message);
    }
    mxState++;

    if (mxState == 6) {
        mxState = 0;
        mxCnt++;
    }
    else{
        ev_queue.call_in(1000, publishMinMax);
    }

    if (mxCnt == 6) {
        mxCnt = 0;
        mxswt = !mxswt;
    }
}

int main() {
    lcd.cls();
    initSD();
    if (wd.WatchdogCausedReset()) {
        logError(WATCHDOG);
    }

    checkBattery();
    readSensors();
    updateChannelsLCD();
    MQTT.begin();
    wd.Configured(3);
    wd.Service();
    ev_queue.call_every(10, checkSerial);
    ev_queue.call_in(1000, subs);
    ev_queue.call_every(1000, readSensors);
    ev_queue.call_every(4000, checkBattery);
    ev_queue.call_in(3000, publishSensorData);
    // ev_queue.call_every(6000, ping);
    ev_queue.call_every(10000, updateChannelsLCD);
    ev_queue.call_every(60000, writeSensorDataToSD);
    ev_queue.call_every(10000, publishMinMax);
    ev_queue.dispatch_forever();
}
