#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <FirebaseArduino.h>
#include <time.h>
#include <EEPROM.h>
extern "C" {
  #include <user_interface.h>
}

#define DATA_LENGTH           112

#define TYPE_MANAGEMENT       0x00
#define TYPE_CONTROL          0x01
#define TYPE_DATA             0x02
#define SUBTYPE_PROBE_REQUEST 0x04

#define FIREBASE_HOST "test3-2c05d.firebaseio.com"
#define FIREBASE_AUTH "yXxfi4ZFQAYUMTgrBnsM3o8RDBz4wwXYkfNxMk6S"

int timezone = 7 * 3600;                    //ตั้งค่า TimeZone ตามเวลาประเทศไทย
int dst = 0;                                //กำหนดค่า Date Swing Time
int count=0 ;

int ar_rssi;
int Rssi ;

int ads = 0;
int first = 0;
char addr2[] = "00:00:00:00:00:00"; //00:00:00:00:00:00
char addr3[] = "00:00:00:00:00:00"; //00:00:00:00:00:00
char currentTime[80];
char macList[400];
char rssiList[45];

struct RxControl {
 signed rssi:8; // signal intensity of packet
 unsigned rate:4;
 unsigned is_group:1;
 unsigned:1;
 unsigned sig_mode:2; // 0:is 11n packet; 1:is not 11n packet;
 unsigned legacy_length:12; // if not 11n packet, shows length of packet.
 unsigned damatch0:1;
 unsigned damatch1:1;
 unsigned bssidmatch0:1;
 unsigned bssidmatch1:1;
 unsigned MCS:7; // if is 11n packet, shows the modulation and code used (range from 0 to 76)
 unsigned CWB:1; // if is 11n packet, shows if is HT40 packet or not
 unsigned HT_length:16;// if is 11n packet, shows length of packet.
 unsigned Smoothing:1;
 unsigned Not_Sounding:1;
 unsigned:1;
 unsigned Aggregation:1;
 unsigned STBC:2;
 unsigned FEC_CODING:1; // if is 11n packet, shows if is LDPC packet or not.
 unsigned SGI:1;
 unsigned rxend_state:8;
 unsigned ampdu_cnt:8;
 unsigned channel:4; //which channel this packet in.
 unsigned:12;
};

struct SnifferPacket{
    struct RxControl rx_ctrl;
    uint8_t data[DATA_LENGTH];
    uint16_t cnt;
    uint16_t len;
};

// Declare each custom function (excluding built-in, such as setup and loop) before it will be called.
// https://docs.platformio.org/en/latest/faq.html#convert-arduino-file-to-c-manually
static void showMetadata(SnifferPacket *snifferPacket);
static void ICACHE_FLASH_ATTR sniffer_callback(uint8_t *buffer, uint16_t length);
static void printDataSpan(uint16_t start, uint16_t size, uint8_t* data);
static void getMAC(char *addr, uint8_t* data, uint16_t offset);


static void showMetadata(SnifferPacket *snifferPacket) {

  unsigned int frameControl = ((unsigned int)snifferPacket->data[1] << 8) + snifferPacket->data[0];

  uint8_t version      = (frameControl & 0b0000000000000011) >> 0;
  uint8_t frameType    = (frameControl & 0b0000000000001100) >> 2;
  uint8_t frameSubType = (frameControl & 0b0000000011110000) >> 4;
  uint8_t toDS         = (frameControl & 0b0000000100000000) >> 8;
  uint8_t fromDS       = (frameControl & 0b0000001000000000) >> 9;


  if (frameType != TYPE_MANAGEMENT ||
      frameSubType != SUBTYPE_PROBE_REQUEST)
        return;
    Serial.print(count);
    EEPROM.begin(4096);
    Serial.print(" RSSI: ");
    ar_rssi = snifferPacket->rx_ctrl.rssi;

    Rssi = ar_rssi;
    Serial.print(Rssi);
    EEPROM.put(ads, Rssi);
    EEPROM.commit();
    

    Serial.print(" ADS RSSI: ");
    Serial.print(ads);
    ads = ads + 4;
//   const char* few = "08:c5:e1:02:43:ce";

    char addr[] = "00:00:00:00:00:00";
    getMAC(addr, snifferPacket->data, 10);
    Serial.print(" Peer MAC: ");
    Serial.print(addr);
    Serial.println("");
    strcpy(addr2, addr);
    EEPROM.put(ads, addr);
    EEPROM.commit();
    Serial.print(" ADS MAC: ");
    Serial.print(ads);    
    ads = ads + 18;
    Serial.println("");  
    count = count +1;
  }

/**
 * Callback for promiscuous mode
 */
static void ICACHE_FLASH_ATTR sniffer_callback(uint8_t *buffer, uint16_t length) {
  struct SnifferPacket *snifferPacket = (struct SnifferPacket*) buffer;
  showMetadata(snifferPacket);
}

static void printDataSpan(uint16_t start, uint16_t size, uint8_t* data) {
  for(uint16_t i = start; i < DATA_LENGTH && i < start+size; i++) {
    Serial.write(data[i]);
  }
}

static void getMAC(char *addr, uint8_t* data, uint16_t offset) {
  sprintf(addr, "%02x:%02x:%02x:%02x:%02x:%02x", data[offset+0], data[offset+1], data[offset+2], data[offset+3], data[offset+4], data[offset+5]);
}

void get_time(){
    configTime(timezone, dst, "pool.ntp.org", "time.nist.gov");     //ดึงเวลาจาก Server
    while (!time(nullptr)) {
      delay(1000);
    }
    configTime(timezone, dst, "pool.ntp.org", "time.nist.gov");    //ดีงเวลาปัจจุบันจาก Server อีกครั้ง
    time_t now = time(nullptr);
    time_t sec = time(nullptr);
    struct tm* p_tm = localtime(&now);
    delay(500);
    Serial.print(ctime(&now));

    sec = sec - 25200;
    EEPROM.begin(4096);
    EEPROM.put(4000, sec);
    EEPROM.commit();
    
    char *macList = "";
   strcpy(currentTime, ctime(&now));

   Serial.print(currentTime);
   Serial.print("Time in sec : ");
   Serial.println(sec);
  
}
int i = 0;
void senddata(){
  
  Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH);
  if (Firebase.failed())
  {
    Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH);
    Serial.println(Firebase.error());
    delay(10);
    Serial.println("Error connecting firebase!");
    Serial.println("RESET ESP");
 
    ESP.reset();
    
  }
  
  int  adz = 0;
  int num0;
  int ms;
  char G[] = "00:00:00:00:00:00";
  EEPROM.get(4000,ms);
  while(adz<ads){
    EEPROM.get(adz,num0);
    adz = adz +4 ;   
    EEPROM.get(adz,G);
    StaticJsonBuffer<200> jsonBuffer;
    JsonObject& valueObject = jsonBuffer.createObject();
    valueObject["mac"] = G;
    valueObject["rssi"] = num0;
    valueObject["date"] = currentTime;
    valueObject["time"] = ms;
    valueObject["node"] = 1;
    Firebase.push("devices" , valueObject);
    delay(100);
    adz = adz + 18;
  }
  delay(1000);
  char *macList = "";
  
}

#define DISABLE 0
#define ENABLE  1

void promiscuous_mode()
{
  wifi_set_opmode(STATION_MODE);
  wifi_set_channel(1);
  wifi_promiscuous_enable(DISABLE);
  delay(10);
  wifi_set_promiscuous_rx_cb(sniffer_callback);
  delay(10);
  wifi_promiscuous_enable(ENABLE);
}

void sta_mode()
{
  wifi_promiscuous_enable(DISABLE);
  delay(10);
  const char* ssid = "isis";                  //ใส่ชื่อ SSID Wifi
  const char* password = "isisisis";          //ใส่รหัสผ่าน
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print("x");
    delay(100);
  }
  delay(100);
  Serial.println("");
  Serial.print("Time : ");
  get_time();
  delay(100);
  Serial.println("Send HERE !!");
  senddata();
  delay(100);
  Serial.println("");
  Serial.println("---------------------------------------จบ------------------------------------");
  WiFi.disconnect();
  count =0;
  ads = 0;
}

void setup() {
  Serial.begin(115200);
  const char* ssid = "isis";                  //ใส่ชื่อ SSID Wifi
  const char* password = "isisisis";          //ใส่รหัสผ่าน
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print("c");
  delay(100);
  }
  Serial.println("");
  configTime(timezone, dst, "pool.ntp.org", "time.nist.gov");     //ดึงเวลาจาก Server
  while (!time(nullptr)) {
      delay(1000);
    }
  configTime(timezone, dst, "pool.ntp.org", "time.nist.gov");    //ดีงเวลาปัจจุบันจาก Server อีกครั้ง
  time_t now = time(nullptr);
  delay(500);

  delay(10);
  WiFi.disconnect();
  count = 0;
  ads = 0;
}

void loop() {

  promiscuous_mode();     // set the WiFi chip to "promiscuous" mode aka monitor mode
  delay(8000);
  sta_mode();             // set esp8266 to sta mode for connect to the internet

  delay(500);
}
