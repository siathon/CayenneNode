#include "GSM_MQTT.h"
#include "Watchdog.h"
#include "TextLCD.h"
#include <string>

unsigned long previousMillis = 0;
bool stringComplete = false;  // whether the string is complete
extern GSM_MQTT MQTT;
extern DigitalOut simEn;
extern DigitalOut simPower;
string MQTT_HOST = "mqtt.mydevices.com";
string MQTT_PORT = "1883";
char* clientID = "ae767cc0-56de-11e9-898b-1503dc6ccc3b";
char* username = "990606a0-4e2b-11e9-9622-9b9aeccba453";
char* password = "763560d0aa760ffea41c3da7bf3d8dfdd9bf3a56";

extern TextLCD lcd;
extern Watchdog wd;
extern RawSerial sim;
extern RawSerial pc;
extern void logError(int type);
extern void updateChannelsLCD();
extern void readSensors();
Ticker pingTicker;
Ticker simTicker;
// void serialEvent();

char buffer[300];
bool receiving = false, newPacket = false, processingPacket = false;
int indx = 0;
string receivedOutput = "", expectedOutput = "", receivedData = "";
int receivedOutputLen = 0, receivedDataLen = 0, expectedOutputLen = 0, expectedDataLen = 0;
bool gotExpectedOutput = true, gotExpectedData = true, simRegistered = false, expectingData = false;
char c;
int sendCmdAndWaitForResp(char *cmd, char *resp, int dataSize, int timeout);

void simRx(){
    c = sim.getc();
    if (c == '\r' || c == '\n') {
        c = '|';
    }
    if (gotExpectedOutput && gotExpectedData) {
        return;
    }
    else if (!gotExpectedOutput) {
        if (c == '|' && receivedOutput[receivedOutputLen] == '|') {
            return;
        }
        if (expectedOutput[receivedOutputLen] == c) {
            receivedOutput += c;
            receivedOutputLen++;
        }
        gotExpectedOutput = expectedOutputLen == receivedOutputLen;
    }
    else if (!gotExpectedData) {
        if (c == '|' && receivedData[receivedDataLen] == '|') {
            return;
        }
        receivedData += c;
        receivedDataLen++;
        gotExpectedData = receivedDataLen == expectedDataLen;
    }
}

void setReceiveParam(string output, int dataLen){
    receivedOutput = "";
    receivedData = "";
    expectedOutput = output;
    expectedDataLen = dataLen;
    receivedOutputLen = 0;
    receivedDataLen = 0;
    expectedOutputLen = output.length();
    gotExpectedOutput = false;
    gotExpectedData = false;
    if (expectedOutputLen == 0) {
        gotExpectedOutput = true;
    }
    if (expectedDataLen == 0) {
        gotExpectedData = true;
    }
    expectingData = true;
}

void sendCmd(char *cmd){
    sim.puts(cmd);
}

int sendCmdAndWaitForResp(char *cmd, char *resp, int dataSize, int timeout){
    setReceiveParam(resp, dataSize);
    sendCmd(cmd);
    while (!gotExpectedData || !gotExpectedOutput){
        timeout -= 1;
        wait_ms(1);
        if (timeout == 0) {
            if (dataSize == -1) {
                expectingData = false;
                while (sim.readable()) {
                    sim.getc();
                }
                return 0;
            }
            expectingData = false;
            while (sim.readable()) {
                sim.getc();
            }
            return -1;
        }
    }
    expectingData = false;
    while (sim.readable()) {
        sim.getc();
    }
    return 0;
}

void simStop(){
    simEn = 0;
}

void simStart(){
    simRegistered = false;
    simEn = 0;
    wait_ms(1000);
    simEn = 1;
    wait_ms(1000);
    int w = 0;
    while(1){
        pc.printf("Registering sim800 on network...");
        simPower = 0;
        wait_ms(1000);
        simPower = 1;
        wait(5);
        while(1){
            if(sendCmdAndWaitForResp("AT+CREG?\r\n", "+CREG: 0,1", 0, 1000) == 0){
                simRegistered = true;
                break;
            }
            else{
                pc.printf("%s\n", receivedOutput.c_str());

            }
            if(w == 20){
                w = 0;
                break;
            }
            wait(1);
            w++;
        }
        if(simRegistered){
            pc.printf("Done\n");
            wait(2);
            break;
        }
        else{
            pc.printf("Failed!\n");
        }
    }
    pc.printf("ATE0\n");
    if(sendCmdAndWaitForResp("ATE0\r\n", "OK", 0, 1000) == 0){
        pc.printf("OK\n");
    }else{
        pc.printf("Error: %s", receivedOutput.c_str());
    }
}

void checkSim(){
    if(sendCmdAndWaitForResp("+++AT+CREG?\r\n", "+CREG: 0,1", 0, 1) == 0){
        simRegistered = true;
        return;
    }
    simStop();
    simStart();
}

int simInit(){
    pc.printf("AT+CREG?\n");
    if(sendCmdAndWaitForResp("AT+CREG?\r\n", "+CREG: 0,1", 0, 1000) == 0){
        pc.printf("OK\n");
    }else{
        pc.printf("%s\n", receivedOutput);
        return -1;
    }
    pc.printf("AT+CGATT?\n");
    if(sendCmdAndWaitForResp("AT+CGATT?\r\n", "+CGATT: 1", 0, 1000) == 0){
        pc.printf("OK\n");
    }else{
        pc.printf("%s\n", receivedOutput);
        return -1;
    }

    pc.printf("AT+CIPSHUT\n");
    if(sendCmdAndWaitForResp("AT+CIPSHUT\r\n", "SHUT OK", 0, 1000) == 0){
        pc.printf("OK\n");
    }else{
        pc.printf("%s\n", receivedOutput);
        return -1;
    }
    pc.printf("AT+SAPBR=3,1,Contype,GPRS\n");
    if(sendCmdAndWaitForResp("AT+SAPBR=3,1,Contype,GPRS\r\n", "OK", 0, 1000) == 0){
        pc.printf("OK\n");
    }else{
        pc.printf("%s\n", receivedOutput);

        return -1;
    }
    pc.printf("AT+SAPBR=3,1,APN,www\n");
    if(sendCmdAndWaitForResp("AT+SAPBR=3,1,APN,www\r\n", "OK", 0, 1000) == 0){
        pc.printf("OK\n");
    }else{
        pc.printf("%s\n", receivedOutput);
        return -1;
    }
    return 0;
}

int gprsStart(){
    pc.printf("AT+SAPBR=1,1\n");
    if(sendCmdAndWaitForResp("AT+SAPBR=1,1\r\n", "OK", 0, 10000) == 0){
        pc.printf("OK\n");
    }else{
        pc.printf("Error: %s", receivedOutput.c_str());
        return -1;
    }
    return 0;
}

int gprsStop(){
    pc.printf("AT+SAPBR=0,1\n");
    if(sendCmdAndWaitForResp("AT+SAPBR=0,1\r\n", "OK", 0, 10000) == 0){
        pc.printf("OK\n");
    }else{
        pc.printf("Error: %s", receivedOutput.c_str());
        return -1;
    }
    return 0;
}

int httpInit(){
    pc.printf("AT+HTTPINIT\n");
    if(sendCmdAndWaitForResp("AT+HTTPINIT\r\n", "OK", 0, 1000) == 0){
        pc.printf("OK\n");
    }else{
        pc.printf("%s\n", receivedOutput);
        return -1;
    }

    pc.printf("AT+HTTPPARA=CID,1\n");
    if(sendCmdAndWaitForResp("AT+HTTPPARA=CID,1\r\n", "OK", 0, 1000) == 0){
        pc.printf("OK\n");
    }else{
        pc.printf("%s\n", receivedOutput);
        return -1;
    }

    pc.printf("AT+HTTPPARA=URL, optimems.net/Hajmi/settings.php\n");
    if(sendCmdAndWaitForResp("AT+HTTPPARA=URL, optimems.net/Hajmi/settings.php\r\n", "OK", 0, 1000) == 0){
        pc.printf("OK\n");
    }else{
        pc.printf("%s\n", receivedOutput);
        return -1;
    }

    pc.printf("AT+HTTPACTION=0\n");
    if(sendCmdAndWaitForResp("AT+HTTPACTION=0\r\n", "+HTTPACTION: 0,200,43", 0, 10000) == 0){
        pc.printf("OK\n");
    }else{
        pc.printf("%s\n", receivedOutput);
        return -1;
    }
    return 0;
}

string readTimeStamp(){
    pc.printf("AT+HTTPREAD\n");
    if(sendCmdAndWaitForResp("AT+HTTPREAD\r\n", "+HTTPREAD:", -1, 3000) == 0){
        pc.printf("OK\n");
    }else{
        pc.printf("%s\n", receivedOutput);
        return "-1";
    }
    pc.printf("%s - %s\n", receivedData, receivedOutput);
    int i=receivedData.find("#");
    string s = receivedData.substr(i+1, 41);
    for (size_t i = 0; i < 4; i++) {
        int v = s.find("%");
        s = s.substr(v+1);
    }
    int v = s.find("%");
    s = s.substr(0, v);
    pc.printf("timeStamp = %s\n", s.c_str());
    pc.printf("AT+HTTPTERM\n");
    if(sendCmdAndWaitForResp("AT+HTTPTERM\r\n", "OK", 0, 1000) == 0){
        pc.printf("OK\n");
    } else{
        pc.printf("%s\n", receivedOutput);
        return "-1";
    }
    return s;
}

int stringToInt(string str){
    double t = 0;
    int l = str.length();
    for(int i = l-1; i >= 0; i--)
        t += (str[i] - '0') * pow(10.0, l - i - 1);
    return (int)t;
}

void tcpRx(){
    if (processingPacket) {
        return;
    }
    c = sim.getc();
    buffer[indx] = c;
    indx++;
    if (!receiving) {
        newPacket = true;
    }
    receiving = true;
}

void checkSerial(){
    wd.Service();
    if (!newPacket) {
        return;
    }

    if (strstr(buffer, "CLOSED") != NULL) {
        MQTT.MQTT_Flag = false;
        MQTT.TCP_Flag = false;
        MQTT.pingFlag = false;
        lcd.locate(0, 1);
        lcd.printf("GSM READY ");
        logError(-2);
        wait(4);
        return;
    }

    MQTT.pingFlag = false;
    while (true) {
        receiving = false;
        wait_ms(10);
        if (!receiving) {
            newPacket = false;
            indx = 0;
            break;
        }
    }
    MQTT.length = 0;
    MQTT.index = 0;
    MQTT.inputString = "";
    c = buffer[MQTT.index];
    MQTT.index++;
    uint8_t ReceivedMessageType = c >> 4;
    uint8_t DUP = (c & DUP_Mask) >> DUP_Mask;
    uint8_t QoS = (c & QoS_Mask) >> QoS_Scale;
    uint8_t RETAIN = (c & RETAIN_Mask);
    for (size_t i = 0; i < 4; i++) {
        if ((ReceivedMessageType >= CONNECT) && (ReceivedMessageType <= DISCONNECT)){
            c = buffer[MQTT.index];
            MQTT.index++;
            MQTT.length += c & 0x7F;
            if ((c & 0x80) != 0x80) {
                break;
            }
        }
    }

    for (size_t i = 0; i < MQTT.length; i++) {
        MQTT.inputString += buffer[MQTT.index];
        MQTT.index++;
    }
    MQTT.parsePacket(ReceivedMessageType, DUP, QoS, RETAIN);
    MQTT.pingFlag = true;
}

void GSM_MQTT::parsePacket(uint8_t ReceivedMessageType, uint8_t DUP, uint8_t QoS, uint8_t RETAIN){
    MQTT.printMessageType(ReceivedMessageType);
    pc.printf(MQTT.inputString.c_str());
    switch (ReceivedMessageType) {
        case CONNECT:{
            break;
        }

        case CONNACK:{
            switch (MQTT.inputString[0]) {
                case 0x00:{
                    pc.printf("Connection Accepted\n");
                    MQTT.MQTT_Flag = true;
                    break;
                }
                case 0x01:{
                    pc.printf("Connection Refused: unacceptable protocol version\n");
                    break;
                }
                case 0x02:{
                    pc.printf("Connection Refused: identifier rejected\n");
                    break;
                }
                case 0x03:{
                    pc.printf("Connection Refused: server unavailable\n");
                    break;
                }
                case 0x04:{
                    pc.printf("Connection Refused: bad user name or password\n");
                    break;
                }
                case 0x05:{
                    pc.printf("Connection Refused: not authorized\n");
                    break;
                }
            }
            break;
        }
        case PUBLISH:{
            pc.printf("%d - %d - %d\n", DUP, QoS, RETAIN);
            while (true) {
                int idx = MQTT.inputString.find('/');
                if (idx == -1) {
                    break;
                }
                MQTT.inputString = MQTT.inputString.substr(idx+1);
            }
            int idx = MQTT.inputString.find(',');
            string msgID = MQTT.inputString.substr(0, idx);
            string value = MQTT.inputString.substr(idx+1,1);
            pc.printf("%s - %s\n", msgID.c_str(), value.c_str());
            sprintf(MQTT.Topic, "v1/%s/things/%s/data/2", username, clientID);
            sprintf(MQTT.Message, "%s", value);
            MQTT.publish(0, 0, 0, MQTT._generateMessageID(), MQTT.Topic, MQTT.Message);
            sprintf(MQTT.Topic, "v1/%s/things/%s/response", username, clientID);
            sprintf(MQTT.Message, "ok,%s", msgID);
            MQTT.publish(0, 0, 0, MQTT._generateMessageID(), MQTT.Topic, MQTT.Message);
            switch (QoS) {
                case 1:
                    publishACK(stringToInt(msgID));
                    break;
                case 2:
                    publishREC(stringToInt(msgID));
                    break;
            }
            break;
        }
        case PUBACK:{
            break;
        }
        case PUBREC:{
            int id = 0;
            id += (uint16_t)(MQTT.inputString[0] << 8);
            id += (uint8_t)MQTT.inputString[1];
            publishREL(0, id);
            break;
        }
        case PUBREL:{
            int id = 0;
            id += (uint16_t)(MQTT.inputString[0] << 8);
            id += (uint8_t)MQTT.inputString[1];
            publishCOMP(id);
            break;
        }
        case SUBSCRIBE:{
            break;
        }
        case SUBACK:{
            break;
        }
        case UNSUBSCRIBE:{
            break;
        }
        case UNSUBACK:{
            break;
        }
        case PINGREQ:{
            break;
        }
        case PINGRESP:{
            MQTT.MQTT_Flag = true;
            pingFailCount = 0;
            break;
        }
        case DISCONNECT:{
            break;
        }
    }

}

GSM_MQTT::GSM_MQTT(unsigned long KeepAlive){
    sim.attach(simRx);
    _KeepAliveTimeOut = KeepAlive;
    TCP_Flag = false;
    pingFlag = false;
    tcpATerrorcount = 0;
    pingFailCount = 0;
    MQTT_Flag = false;
    PublishIndex = 0;
    TopicLength = 0;
    MessageLength = 0;
    MessageFlag = false;
    modemStatus = 0;
    index = 0;
    length = 0, lengthLocal = 0;
    GSM_Response = 0;
    ConnectionAcknowledgement = NO_ACKNOWLEDGEMENT;
    _LastMessaseID = 0;
    _ProtocolVersion = 3;
    _PingPrevMillis = 0;
    _tcpStatus = 0;
    _tcpStatusPrev = 0;
}

void GSM_MQTT::begin(void){
    lcd.locate(0, 1);
    lcd.printf("NOT READY!");
    string s = "-1";
    while (s.compare("-1") == 0) {
        checkSim();
        simInit();
        gprsStart();
        httpInit();
        s = readTimeStamp();
        gprsStop();
    }
    set_time(stringToInt(s));
    readSensors();
    updateChannelsLCD();
    lcd.locate(0, 1);
    lcd.printf("GSM READY ");
    _tcpInit();
    readSensors();
    updateChannelsLCD();
    lcd.locate(0, 1);
    lcd.printf("CONNECTED ");
}

void GSM_MQTT::AutoConnect(void){
    pc.printf("Connecting...");
    connect(clientID, 1, 1, username, password, 1, 0, 0, 0, "", "");
    pc.printf("Done\n");
}

void GSM_MQTT::_tcpInit(void){
  switch (modemStatus){
    case 0:
        pc.printf("AT\n");
        if (sendCmdAndWaitForResp("AT\r\n", "OK", 0, 1000) == 0){
          modemStatus = 1;
        }
        else{
          modemStatus = 0;
          pc.printf("%s\n", receivedOutput.c_str());
          break;
        }

    case 1:
        if(sendCmdAndWaitForResp("ATE0\r\n", "OK", 0, 1000) == 0){
          modemStatus = 2;
        }
        else{
          modemStatus = 1;
          pc.printf("%s\n", receivedOutput.c_str());
          break;
        }

    case 2:
        pc.printf("AT+CREG?\n");
        if (sendCmdAndWaitForResp("AT+CREG?\r\n", "0,1", 0, 1000) == 0) {
            pc.printf("AT+CIPMUX=0\n");
            sendCmdAndWaitForResp("AT+CIPMUX=0\r\n", "", 0, 2000);
            pc.printf("AT+CIPMODE=1\n");
            sendCmdAndWaitForResp("AT+CIPMODE=1\r\n", "", 0, 2000);
            pc.printf("AT+CGATT?\n");
            if (sendCmdAndWaitForResp("AT+CGATT?\r\n", ": 1", 0, 4000) == 0) {
                pc.printf("AT+CGATT=1\n");
                sendCmdAndWaitForResp("AT+CGATT=1\r\n", "", 0, 2000);
            }
            modemStatus = 3;
            _tcpStatus = 2;
        }
        else{
          modemStatus = 2;
          break;
        }

    case 3:
        while (true) {
            if (_tcpStatus != 7){
                pc.printf("AT+CIPSTATUS\n");
                if (sendCmdAndWaitForResp("AT+CIPSTATUS\r\n", "STATE", -1, 1000) == 0) {
                    _tcpStatus = 1;
                }
                else{
                    pc.printf("%s\n", receivedData);
                    break;
                }
                if (receivedData.find(" INITIAL") != -1) {
                    _tcpStatus = 2;
                }
                else if (receivedData.find("START") != -1) {
                    _tcpStatus = 3;
                }
                else if (receivedData.find("IP CONFIG") != -1) {
                    _tcpStatus = 4;
                }
                else if (receivedData.find("GPRSACT") != -1) {
                    _tcpStatus = 4;
                }
                else if (receivedData.find("STATUS") != -1) {
                    _tcpStatus = 5;
                }
                else if (receivedData.find("TCP CONNECTING") != -1) {
                    _tcpStatus = 6;
                }
                else if (receivedData.find(" CONNECT OK") != -1) {
                    _tcpStatus = 7;
                }
            }

              if (_tcpStatusPrev == _tcpStatus){
                tcpATerrorcount++;
                if (tcpATerrorcount >= 10){
                  tcpATerrorcount = 0;
                  _tcpStatus = 7;
                }
              }
              else{
                _tcpStatusPrev = _tcpStatus;
                tcpATerrorcount = 0;
              }
            _tcpStatusPrev = _tcpStatus;
            pc.printf("TCP Status = %d\n", _tcpStatus);
            switch (_tcpStatus){
              case 2:
                pc.printf("AT+CSTT=\"mtnirancell\"\n");
                sendCmdAndWaitForResp("AT+CSTT=\"mtnirancell\"\r\n", "OK", 0, 5000);
                break;

              case 3:
                pc.printf("AT+CIICR\n");
                sendCmdAndWaitForResp("AT+CIICR\r\n", "OK", 0, 5000);
                break;

              case 4:
                pc.printf("AT+CIFSR\n");
                sendCmdAndWaitForResp("AT+CIFSR\r\n", ".", 0, 4000);
                break;

              case 5:
                char t[100];
                sprintf(t, "AT+CIPSTART=\"TCP\",\"%s\",\"%s\"\r\n", MQTT_HOST, MQTT_PORT);
                pc.printf("%s\n", t);
                if(sendCmdAndWaitForResp(t, "CONNECT", 0, 20000) == 0){
                    TCP_Flag = true;
                    pc.printf("MQTT.TCP_Flag = True\n");
                    sim.attach(tcpRx);
                    AutoConnect();
                    pingFlag = true;
                    tcpATerrorcount = 0;
                    return;
                }
                break;

              case 6:
              if(sendCmdAndWaitForResp("", "CONNECT", 0, 20000) == 0){
                  TCP_Flag = true;
                  pc.printf("MQTT.TCP_Flag = True\n");
                  sim.attach(tcpRx);
                  AutoConnect();
                  pingFlag = true;
                  tcpATerrorcount = 0;
                  return;
              }
                break;

              case 7:
                pc.printf("AT+CIPSHUT\n");
                sendCmdAndWaitForResp("AT+CIPSHUT\r\n", "OK", 0, 4000);
                modemStatus = 0;
                _tcpStatus = 2;
                break;
            }
        }
    }
}

void GSM_MQTT::_ping(void){
    if (pingFailCount >= 4) {
        MQTT.MQTT_Flag = false;
        MQTT.TCP_Flag = false;
        MQTT.pingFlag = false;
        lcd.locate(0, 1);
        lcd.printf("GSM READY ");
        logError(-1);
        wait(4);
        return;
    }
    pingFailCount++;
    if (available() && pingFlag){
        pc.printf("ping\n");
        sim.putc(char(PINGREQ * 16));
        _sendLength(0);
    }
}

void GSM_MQTT::_sendUTFString(char *str){
  int localLength = strlen(str);
  sim.putc(char(localLength / 256));
  sim.putc(char(localLength % 256));
  sim.printf("%s", str);
}

void GSM_MQTT::_sendLength(int len){
  bool  length_flag = false;
  while (length_flag == false){
    if ((len / 128) > 0){
      sim.putc(char(len % 128 + 128));
      len /= 128;
    }
    else{
      length_flag = true;
      sim.putc(char(len));
    }
  }
}

void GSM_MQTT::connect(char *ClientIdentifier, char UserNameFlag, char PasswordFlag, char *UserName, char *Password, char CleanSession, char WillFlag, char WillQoS, char WillRetain, char *WillTopic, char *WillMessage){
  ConnectionAcknowledgement = NO_ACKNOWLEDGEMENT ;
  sim.putc(char(CONNECT * 16 ));
  char ProtocolName[7] = "MQIsdp";
  int localLength = (2 + strlen(ProtocolName)) + 1 + 3 + (2 + strlen(ClientIdentifier));
  if (WillFlag != 0){
    localLength = localLength + 2 + strlen(WillTopic) + 2 + strlen(WillMessage);
  }
  if (UserNameFlag != 0){
    localLength = localLength + 2 + strlen(UserName);

    if (PasswordFlag != 0){
      localLength = localLength + 2 + strlen(Password);
    }
  }
  _sendLength(localLength);
  _sendUTFString(ProtocolName);
  sim.putc(char(_ProtocolVersion));
  sim.putc(char(UserNameFlag * User_Name_Flag_Mask + PasswordFlag * Password_Flag_Mask + WillRetain * Will_Retain_Mask + WillQoS * Will_QoS_Scale + WillFlag * Will_Flag_Mask + CleanSession * Clean_Session_Mask));
  sim.putc(char(_KeepAliveTimeOut / 256));
  sim.putc(char(_KeepAliveTimeOut % 256));
  _sendUTFString(ClientIdentifier);
  if (WillFlag != 0){
    _sendUTFString(WillTopic);
    _sendUTFString(WillMessage);
  }
  if (UserNameFlag != 0){
    _sendUTFString(UserName);
    if (PasswordFlag != 0){
      _sendUTFString(Password);
    }
  }
}

void GSM_MQTT::publish(char DUP, char Qos, char RETAIN, unsigned int MessageID, char *Topic, char *Message){
  sim.putc(char(PUBLISH * 16 + DUP * DUP_Mask + Qos * QoS_Scale + RETAIN));
  int localLength = (2 + strlen(Topic));
  if (Qos > 0){
    localLength += 2;
  }
  localLength += strlen(Message);
  _sendLength(localLength);
  _sendUTFString(Topic);
  if (Qos > 0){
    sim.putc(char(MessageID / 256));
    sim.putc(char(MessageID % 256));
  }
  sim.printf("%s", Message);
}

void GSM_MQTT::publishACK(unsigned int MessageID){
  sim.putc(char(PUBACK * 16));
  _sendLength(2);
  sim.putc(char(MessageID / 256));
  sim.putc(char(MessageID % 256));
}

void GSM_MQTT::publishREC(unsigned int MessageID){
  sim.putc(char(PUBREC * 16));
  _sendLength(2);
  sim.putc(char(MessageID / 256));
  sim.putc(char(MessageID % 256));
}

void GSM_MQTT::publishREL(char DUP, unsigned int MessageID){
  sim.putc(char(PUBREL * 16 + DUP * DUP_Mask + 1 * QoS_Scale));
  _sendLength(2);
  sim.putc(char(MessageID / 256));
  sim.putc(char(MessageID % 256));
}

void GSM_MQTT::publishCOMP(unsigned int MessageID){
  sim.putc(char(PUBCOMP * 16));
  _sendLength(2);
  sim.putc(char(MessageID / 256));
  sim.putc(char(MessageID % 256));
}

void GSM_MQTT::subscribe(char DUP, unsigned int MessageID, char *SubTopic, char SubQoS){
  sim.putc(char(SUBSCRIBE * 16 + DUP * DUP_Mask + 1 * QoS_Scale));
  int localLength = 2 + (2 + strlen(SubTopic)) + 1;
  _sendLength(localLength);
  sim.putc(char(MessageID / 256));
  sim.putc(char(MessageID % 256));
  _sendUTFString(SubTopic);
  sim.putc(SubQoS);
}

void GSM_MQTT::unsubscribe(char DUP, unsigned int MessageID, char *SubTopic){
  sim.putc(char(UNSUBSCRIBE * 16 + DUP * DUP_Mask + 1 * QoS_Scale));
  int localLength = (2 + strlen(SubTopic)) + 2;
  _sendLength(localLength);

  sim.putc(char(MessageID / 256));
  sim.putc(char(MessageID % 256));

  _sendUTFString(SubTopic);
}

void GSM_MQTT::disconnect(void){
  sim.putc(char(DISCONNECT * 16));
  _sendLength(0);
  pingFlag = false;
}

//Messages
const char CONNECTMessage[] = {"Client request to connect to Server\r\n"};
const char CONNACKMessage[] = {"Connect Acknowledgment\r\n"};
const char PUBLISHMessage[] = {"Publish message\r\n"};
const char PUBACKMessage[] = {"Publish Acknowledgment\r\n"};
const char PUBRECMessage[] = {"Publish Received (assured delivery part 1)\r\n"};
const char PUBRELMessage[] = {"Publish Release (assured delivery part 2)\r\n"};
const char PUBCOMPMessage[] = {"Publish Complete (assured delivery part 3)\r\n"};
const char SUBSCRIBEMessage[] = {"Client Subscribe request\r\n"};
const char SUBACKMessage[] = {"Subscribe Acknowledgment\r\n"};
const char UNSUBSCRIBEMessage[] = {"Client Unsubscribe request\r\n"};
const char UNSUBACKMessage[] = {"Unsubscribe Acknowledgment\r\n"};
const char PINGREQMessage[] = {"PING Request\r\n"};
const char PINGRESPMessage[] = {"PING Response\r\n"};
const char DISCONNECTMessage[] = {"Client is Disconnecting\r\n"};

void GSM_MQTT::printMessageType(uint8_t Message){
  switch (Message){
    case CONNECT:
        pc.printf(CONNECTMessage);
        break;

    case CONNACK:
        pc.printf(CONNACKMessage);
        break;

    case PUBLISH:
      pc.printf(PUBLISHMessage);
        break;

    case PUBACK:
      pc.printf(PUBACKMessage);
        break;

    case  PUBREC:
      pc.printf(PUBRECMessage);

    case PUBREL:
      pc.printf(PUBRELMessage);
        break;

    case PUBCOMP:
      pc.printf(PUBCOMPMessage);
        break;

    case SUBSCRIBE:
      pc.printf(SUBSCRIBEMessage);
        break;

    case SUBACK:
      pc.printf(SUBACKMessage);
        break;

    case UNSUBSCRIBE:
      pc.printf(UNSUBSCRIBEMessage);
        break;

    case UNSUBACK:
      pc.printf(UNSUBACKMessage);
        break;

    case PINGREQ:
      pc.printf(PINGREQMessage);
        break;

    case PINGRESP:
      pc.printf(PINGRESPMessage);
        break;

    case DISCONNECT:
      pc.printf(DISCONNECTMessage);
        break;
  }
}

//Connect Ack
const char ConnectAck0[] = {"Connection Accepted\r\n"};
const char ConnectAck1[] = {"Connection Refused: unacceptable protocol version\r\n"};
const char ConnectAck2[] = {"Connection Refused: identifier rejected\r\n"};
const char ConnectAck3[] = {"Connection Refused: server unavailable\r\n"};
const char ConnectAck4[] = {"Connection Refused: bad user name or password\r\n"};
const char ConnectAck5[] = {"Connection Refused: not authorized\r\n"};

void GSM_MQTT::printConnectAck(uint8_t Ack){
  switch (Ack){
    case 0:
      pc.printf(ConnectAck0);
        break;

    case 1:
      pc.printf(ConnectAck1);
        break;

    case 2:
      pc.printf(ConnectAck2);
        break;

    case 3:
      pc.printf(ConnectAck3);
        break;

    case 4:
      pc.printf(ConnectAck4);
        break;

    case 5:
      pc.printf(ConnectAck5);
        break;
  }
}

unsigned int GSM_MQTT::_generateMessageID(void){
  if (_LastMessaseID < 65535){
    return ++_LastMessaseID;
  }
  else{
    _LastMessaseID = 0;
    return _LastMessaseID;
  }
}

bool GSM_MQTT::available(void){
  return MQTT_Flag;
}

void GSM_MQTT::OnMessage(char *Topic, int TopicLength, char *Message, int MessageLength){
  /*
    This function is called whenever a message received from subscribed topics
    put your subscription publish codes here.
  */

  /*
     Topic        :Name of the topic from which message is coming
     TopicLength  :Number of characters in topic name
     Message      :The containing array
     MessageLength:Number of characters in message
  */
  pc.printf("Topic length = %d\n", TopicLength);
  pc.printf("Topic = %s\n", Topic);
  pc.printf("Message length = %d\n", MessageLength);
  pc.printf("Message = %s\n", Message);
}
