#include <Wire.h>
#include <ESP8266WiFi.h> 
#include <PubSubClient.h>
#include <SPI.h>
#include <MFRC522.h> 

//  MQTT設定
const char* ssid ="";
const char*password =  "";
const char*mqttServer = "";
const int mqttPort =1883;
const char*mqttUser = "yourMQTTuser";
const char*mqttPassword = "yourMQTTpassword";
const char* mqttUserName = "cubie";  // 使用者名稱，隨意設定。
const char* mqttPwd = "你的MQTT API Key";  // MQTT密碼
const char* clientID = "A";      // 用戶端ID，隨意設定。
unsigned long prevMillis = 0;  // 暫存經過時間（毫秒）
const long interval = 100;  // 上傳資料的間隔時間，0.1秒。
String msgStr = "";      // 暫存MQTT訊息字串
const char* topic_state = "Feedback/UserID_State";

//  RFID設定
#define RST_PIN      0        // 讀卡機的重置腳位 RST-PIN für RC522 - RFID - SPI - Modul GPIO0
#define SS_PIN       2        // 晶片選擇腳位 SDA-PIN für RC522 - RFID - SPI - Modul GPIO2 
MFRC522 mfrc522(SS_PIN, RST_PIN);  // 建立MFRC522物件
MFRC522::MIFARE_Key key;  // 儲存金鑰

byte sector = 15;   // 指定讀寫的「區段」，可能值:0~15
byte block = 1;     // 指定讀寫的「區塊」，可能值:0~3
byte blockData[16] = "";
String _tempID = "";    //比較
const int State = 1;  

// 暫存讀取區塊內容的陣列，MIFARE_Read()方法要求至少要18位元組空間，來存放16位元組。
byte buffer[18];

MFRC522::StatusCode status;
WiFiClient espClient;
PubSubClient client(espClient);

void setup() {
  Serial.begin(115200);
  setup_wifi();
  client.setServer(mqttServer, 1883); 

  SPI.begin(); 
  mfrc522.PCD_Init();   // 初始化MFRC522讀卡機模組
  setup_key();

  client.setCallback(callback);
  while (!client.connected()){
    Serial.println("Connectingto MQTT...");
    if (client.connect("ESP32Client",mqttUser, mqttPassword )) {
      Serial.println("connected"); 
    }else {
      Serial.print("failedwith state ");
      Serial.print(client.state());
      delay(2000);
    }
  }
  client.subscribe("Topic/register");
}

void loop() {
  
  delay(100);

  if (!client.connected()) {
    reconnect();
  }
  
  client.loop();
  
  // 等待0.7秒
  if (millis() - prevMillis > interval) {
    prevMillis = millis();

    mfrc522.PCD_Init();   
    uid_and_changedata();  
  }   
}

void setup_wifi() {
  delay(10);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
}

void reconnect() {
  while (!client.connected()) {
    if (client.connect(clientID, mqttUserName, mqttPwd)) {
      Serial.println("MQTT connected");
    } 
    else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);  // 等5秒之後再重試
    }
  }
}

void setup_key() {
    for (byte i = 0; i < 6; i++) 
        { key.keyByte[i] = 0xFF;}
}
void callback(char*topic, byte* payload, unsigned int length) {
  Serial.print("Messagearrived in topic: ");
  Serial.println(topic);
  Serial.print("Message:");

  String str_UserID = "";
  for (int i = 0; i< length; i++) {
    //str_UserID.concat(String(payload[i]-'0'));
    //Serial.println(String(payload[i]-'0'));
    
    blockData[i] = char(payload[i]);
    str_UserID += blockData[i];
    Serial.println(char(payload[i]));
  }
  Serial.println(str_UserID);

  Serial.println();
  Serial.println("-----------------------");

}

void uid_and_changedata(){
    
    // 確認是否有新卡片
    if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
      byte *id = mfrc522.uid.uidByte;   // 取得卡片的UID
      byte idSize = mfrc522.uid.size;   // 取得UID的長度

      Serial.print("PICC type: ");      // 顯示卡片類型
      // 根據卡片回應的SAK值（mfrc522.uid.sak）判斷卡片類型
      MFRC522::PICC_Type piccType = mfrc522.PICC_GetType(mfrc522.uid.sak);
      Serial.println(mfrc522.PICC_GetTypeName(piccType));

      //Serial.print("UID Size: ");       // 顯示卡片的UID長度值
      //Serial.println(idSize);

      String str_Uid = "";
      for (byte i = 0; i < idSize; i++) {  // 逐一顯示UID碼
          str_Uid.concat(String(id[i], HEX));
          //Serial.print(id[i], HEX); Serial.print(" ");
      }
      Serial.println(str_Uid);

      writeBlock(sector, block, blockData);  // 區段編號、區塊編號、包含寫入資料的陣列
      readBlock(sector, block, buffer);      // 區段編號、區塊編號、存放讀取資料的陣列
      
      Serial.print(F("Read block: "));        // 顯示儲存讀取資料的陣列元素值
      
      String str_changedata = "";
      //  i正常為16，但僅6位，可改成６，否則後面都是0
      for (byte i = 0 ; i < 16 ; i++) {
          str_changedata.concat(String(buffer[i]-'0'));
      }
      Serial.println(str_changedata);
      Serial.println();

      _tempID = str_changedata;
      
      mfrc522.PICC_HaltA();  // 讓卡片進入停止模式

      if(_tempID == str_changedata){

            String payload = "{";
                   payload += "\"UserID\":";
                   payload += str_changedata;
                   payload += "}";

             byte arrSize = payload.length() + 1;
             char msg[arrSize];
             payload.toCharArray(msg, arrSize);
             client.publish(topic_state, msg);
      }

    } 
}

void writeBlock(byte _sector, byte _block, byte _blockData[]) {
  if (_sector < 0 || _sector > 15 || _block < 0 || _block > 3) {
    // 顯示「區段或區塊碼錯誤」，然後結束函式。
    Serial.println(F("Wrong sector or block number."));
    return;
  }

  if (_sector == 0 && _block == 0) {
    // 顯示「第一個區塊只能讀取」，然後結束函式。
    Serial.println(F("First block is read-only."));
    return;
  }

  byte blockNum = _sector * 4 + _block;  // 計算區塊的實際編號（0~63）
  byte trailerBlock = _sector * 4 + 3;   // 控制區塊編號

  // 驗證金鑰
  status = (MFRC522::StatusCode) mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, trailerBlock, &key, &(mfrc522.uid));
  // 若未通過驗證…
  if (status != MFRC522::STATUS_OK) {
    // 顯示錯誤訊息
    Serial.print(F("PCD_Authenticate() failed: "));
    Serial.println(mfrc522.GetStatusCodeName(status));
    return;
  }

  // 在指定區塊寫入16位元組資料
  status = (MFRC522::StatusCode) mfrc522.MIFARE_Write(blockNum, _blockData, 16);
  // 若寫入不成功…
  if (status != MFRC522::STATUS_OK) {
    // 顯示錯誤訊息
    Serial.print(F("MIFARE_Write() failed: "));
    Serial.println(mfrc522.GetStatusCodeName(status));
    return;
  }

  // 顯示「寫入成功！」
  Serial.println(F("Data was written."));
}


void readBlock(byte _sector, byte _block, byte _blockData[])  {
  if (_sector < 0 || _sector > 15 || _block < 0 || _block > 3) {
    // 顯示「區段或區塊碼錯誤」，然後結束函式。
    Serial.println(F("Wrong sector or block number."));
    return;
  }

  byte blockNum = _sector * 4 + _block;  // 計算區塊的實際編號（0~63）
  byte trailerBlock = _sector * 4 + 3;   // 控制區塊編號

  // 驗證金鑰
  status = (MFRC522::StatusCode) mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, trailerBlock, &key, &(mfrc522.uid));
  // 若未通過驗證…
  if (status != MFRC522::STATUS_OK) {
    // 顯示錯誤訊息
    Serial.print(F("PCD_Authenticate() failed: "));
    Serial.println(mfrc522.GetStatusCodeName(status));
    return;
  }

  byte buffersize = 18;
  status = (MFRC522::StatusCode) mfrc522.MIFARE_Read(blockNum, _blockData, &buffersize);

  // 若讀取不成功…
  if (status != MFRC522::STATUS_OK) {
    // 顯示錯誤訊息
    Serial.print(F("MIFARE_read() failed: "));
    Serial.println(mfrc522.GetStatusCodeName(status));
    return;
  }

  // 顯示「讀取成功！」
  Serial.println(F("Data was read."));
}
