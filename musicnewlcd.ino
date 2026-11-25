
#define DEBUG 0
#define GROOVE_LCD 0

#include "var.h"
#include <Adafruit_VS1053.h>
#define RAW_BUFFER_LENGTH                                                      \
  100 // Default is 750 on Mega! Saves ~650 bytes RAM. 100 supports NEC/Sony/RC5
      // up to 48 bits.
#define EXCLUDE_EXOTIC_PROTOCOLS // Save ~500 bytes flash + some RAM
#define NO_LED_FEEDBACK_CODE     // Save ~100 bytes flash
#include <IRremote.h>
#include <SD.h>
#include <SPI.h>

#if GROOVE_LCD
#include "rgb_lcd.h"
#else
#include <LiquidCrystal_I2C.h>
#endif

// Forward declarations
void SerialsPrint(String text, uint8_t output);
void SerialsPrint(const char *text, uint8_t output);
void SerialsPrint(int text, uint8_t output);
void SerialsPrintln(String text, uint8_t output);
void SerialsPrintln(const char *text, uint8_t output);
void SerialsPrintln(int text, uint8_t output);
bool sendATCommand(String cmd, uint8_t retry, uint32_t timeout, const char *acc,
                   const char *err);
bool sendATCommand(const char *cmd, uint8_t retry, uint32_t timeout,
                   const char *acc, const char *err);
bool sendATCommandNoPrint(String cmd, uint8_t retry, uint32_t timeout,
                          const char *acc, const char *err);
bool sendATCommandNoPrint(const char *cmd, uint8_t retry, uint32_t timeout,
                          const char *acc, const char *err);
bool gsmReadSerial(uint32_t timeout, const char *accept, const char *reject);
bool timeFromString(String dateTime);
String printTime(uint8_t ngay, uint8_t thang, uint8_t nam, uint8_t gio,
                 uint8_t phut, uint8_t giay);
uint8_t gsmReset();
void initGsm();
void detectGsmModule();
void xulylenh(String cmd);
void xulyhongngoai(int cmd);
String checkLichNhac();
void sendBufferedSMS(String phone);
void ngayPhucSinh(uint8_t nam);
void ngayAmLich(uint8_t ngay, uint8_t thang, uint8_t nam);
void weekday(uint8_t ngay, uint8_t thang, uint8_t nam);
uint8_t music_ls(Print *pr);
uint8_t xulyFile(uint8_t cmdCode, String cmd, int line_check);
uint8_t sdCopy(const char *srcName, const char *desName);
uint8_t validDateTime(uint8_t ngay, uint8_t thang, uint8_t nam, uint8_t gio,
                      uint8_t phut, uint8_t giay);

#define gsm Serial2

// GSM Module type
#define GSM_UNKNOWN 0
#define GSM_SIM800 1
#define GSM_A7680 2
uint8_t gsmModuleType = GSM_UNKNOWN;

const char *ver = "MUSICSYSTEM 3.1";

// VS1053 Shield pins
#define SHIELD_RESET 8
#define SHIELD_CS 6
#define SHIELD_DCS 7
#define DREQ_PIN 2
#define CARDCS 9

#if GROOVE_LCD
rgb_lcd lcd;
#else
LiquidCrystal_I2C lcd(0x27, 16, 2);
#endif

Adafruit_VS1053_FilePlayer musicPlayer = Adafruit_VS1053_FilePlayer(
    SHIELD_RESET, SHIELD_CS, SHIELD_DCS, DREQ_PIN, CARDCS);
uint8_t currentVolume = 2; // VS1053 volume: 0=loudest, 254=silence
bool isMP3, mode, printMode;

// Check if filename is a music file (replaces isFnMusic from SFEMP3Shield)
bool isFnMusic(const char *filename) {
  int len = strlen(filename);
  if (len < 5)
    return false;
  const char *ext = filename + len - 4;
  return (strcasecmp(ext, ".mp3") == 0 || strcasecmp(ext, ".ogg") == 0 ||
          strcasecmp(ext, ".wav") == 0 || strcasecmp(ext, ".mid") == 0);
}

// Helper: play track by number (track001.mp3, track002.mp3, etc.)
bool playTrackByNumber(uint8_t num) {
  char trackName[16];
  sprintf(trackName, "/track%03d.mp3", num);
  IrReceiver.stop(); // Pause IR timer to protect SD card SPI
  bool ok = musicPlayer.startPlayingFile(trackName);
  IrReceiver.start(); // Resume IR timer
  return ok;
}

// Helper: play MP3 by filename
bool playMP3File(String filename) {
  filename.trim();
  if (filename[0] != '/')
    filename = "/" + filename;
  IrReceiver.stop(); // Pause IR timer to protect SD card SPI
  bool ok = musicPlayer.startPlayingFile(filename.c_str());
  IrReceiver.start(); // Resume IR timer
  return ok;
}

// SD rename helper (SD library may not have rename)
bool sdRename(const char *oldName, const char *newName) {
  File src = SD.open(oldName, FILE_READ);
  if (!src)
    return false;
  SD.remove(newName);
  File dst = SD.open(newName, FILE_WRITE);
  if (!dst) {
    src.close();
    return false;
  }
  while (src.available())
    dst.write(src.read());
  src.close();
  dst.close();
  SD.remove(oldName);
  return true;
}

String gsmBuffer;
bool have_sms, master, restart;

// SMS buffering: accumulate text then send in 160-char parts
String smsBuffer;
bool smsBuffering = false;

// Detect which GSM module is connected (SIM800C or A7680C)
void detectGsmModule() {
  sendATCommand("ATI", 3, 2000, "OK", "ERROR");
  if (gsmBuffer.indexOf("SIM800") > -1) {
    gsmModuleType = GSM_SIM800;
    SerialsPrintln("Phat hien module: SIM800C", P_SERIAL | P_SERIAL1);
  } else if (gsmBuffer.indexOf("A7680") > -1) {
    gsmModuleType = GSM_A7680;
    SerialsPrintln("Phat hien module: A7680C", P_SERIAL | P_SERIAL1);
  } else {
    gsmModuleType = GSM_UNKNOWN;
    SerialsPrintln("Khong nhan dien duoc module GSM", P_SERIAL | P_SERIAL1);
  }
}
String phone_master = "949847098";
String phone_second = "";

// uint8_t ngay, thang, nam, gio , phut, giay;
// uint8_t ngay_am , thang_am;
// String tuan;
// uint8_t ngay_phuc_sinh , thang_phuc_sinh;
// uint8_t ngay_thang_thien , thang_thang_thien;
// uint8_t ngay_hien_xuong , thang_hien_xuong;

uint8_t lastReceiveTimeSignal;

uint8_t ngay_check, thang_check, nam_check, gio_check, phut_check, giay_check;
uint8_t ngay_old, thang_old, nam_old, gio_old, phut_old;
uint8_t ngay_am_check, thang_am_check;
String tuan_check;
uint8_t ngay_phuc_sinh_check, thang_phuc_sinh_check;
uint8_t ngay_thang_thien_check, thang_thang_thien_check;
uint8_t ngay_hien_xuong_check, thang_hien_xuong_check;

const uint8_t daysInMonth[13] = {29, 31, 28, 31, 30, 31, 30,
                                 31, 31, 30, 31, 30, 31};
const char *thu[7] = {"CN", "T2", "T3", "T4", "T5", "T6", "T7"};

void setup() {

  // init Serial
  Serial.begin(9600);
  Serial1.begin(9600);
  gsm.begin(38400);

  // init LCD
#if GROOVE_LCD
  lcd.begin(16, 2);
#else
  lcd.init();
  lcd.backlight();
#endif

  lcd.clear();
  lcd.setCursor(0, 0);

  // Init Pin Relay
  pinMode(BUTTON, INPUT_PULLUP);
  pinMode(RELAY, OUTPUT);
  digitalWrite(RELAY, LOW);

  // delay(5000);
  // VERSION
  SerialsPrintln(ver, P_SERIAL | P_SERIAL1);
  SerialsPrintln("", P_SERIAL | P_SERIAL1);

#if DEBUG
  SerialsPrintln("PHIEN BAN HO TRO DEBUG\n\n", P_SERIAL | P_SERIAL1);
#endif

  // init variable
  restart = 0;
  mode = AUTO;
  lastReceiveTimeSignal = 7;
  printMode = 0;
  master = 1;

  // init IR Receiver
  lcd.print("KHOI DONG.");
  SerialsPrintln("Dang khoi dong hong ngoai....\n", P_SERIAL | P_SERIAL1);
  IrReceiver.begin(IR_RECEIVE_PIN);
  SerialsPrint("Hong ngoai san sang tai chan ", P_SERIAL | P_SERIAL1);
  SerialsPrintln(IR_RECEIVE_PIN, P_SERIAL | P_SERIAL1);

  // Init MP3 (VS1053)
  lcd.print(".");
  SerialsPrintln("\nDang khoi dong module MP3....", P_SERIAL | P_SERIAL1);
  if (musicPlayer.begin()) {
    if (!SD.begin(CARDCS)) {
      SerialsPrintln("Khoi dong the nhac THAT BAI", P_SERIAL | P_SERIAL1);
    } else {
      SerialsPrintln("Khoi dong the nhac THANH CONG", P_SERIAL | P_SERIAL1);
    }
    musicPlayer.setVolume(currentVolume, currentVolume);
    musicPlayer.useInterrupt(VS1053_FILEPLAYER_PIN_INT);
    SerialsPrintln("Mp3 khoi dong thanh cong.", P_SERIAL | P_SERIAL1);

    isMP3 = 1;
  } else {
    SerialsPrint("MP3/SD Loi", P_SERIAL | P_SERIAL1);
  }

  // Init Second Phone Number.
  lcd.print(".");
  if (SD.exists("PHONE.TXT")) {
    SerialsPrintln("Kiem tra so dien thoai thu 2...", P_SERIAL | P_SERIAL1);
    File file = SD.open("PHONE.TXT");
    if (file) {
      while (file.available()) {
        phone_second += file.readString();
      }
      phone_second.trim();
      if (phone_second.length() == 9 && phone_second.toInt()) {
        SerialsPrint("Doc so dien thoai phu thanh cong: ",
                     P_SERIAL | P_SERIAL1);
        SerialsPrintln(phone_second, P_SERIAL | P_SERIAL1);

      } else {
        SerialsPrint("Doc so dien thoai phu khong co", P_SERIAL | P_SERIAL1);
        phone_second = "";
      }
    } else
      SerialsPrintln("Loi mo file phone.txt", P_SERIAL | P_SERIAL1);
    file.close();
  } else
    SerialsPrintln("So dt phu chua khoi tao.", P_SERIAL | P_SERIAL1);

  // Init GSM Module
  // First test
  lcd.print(".");
  SerialsPrintln("\nDang khoi dong module GSM....\n", P_SERIAL | P_SERIAL1);
  uint8_t erCode = gsmReset();

  if (erCode) {
    SerialsPrint("Loi khoi dong module GSM: ", P_SERIAL | P_SERIAL1);
    SerialsPrintln(erCode, P_SERIAL | P_SERIAL1);
  } else {
    // Detect module type
    detectGsmModule();
    lcd.print(".");
    SerialsPrintln("Thiet lap gio GPS...", P_SERIAL | P_SERIAL1);
    bool timeOk = false;
    if (gsmModuleType == GSM_A7680) {
      sendATCommand("AT+CTZU=1", 3, 2000, "OK", "ERROR");
      sendATCommand("AT+COPS=2", 3, 5000, "OK", "ERROR");
      timeOk = sendATCommand("AT+COPS=0", 10, 120000, "CGEV:", "ERROR");
    } else {
      timeOk = sendATCommand("AT+CLTS=1;+COPS=2;+COPS=0", 10, 120000,
                             "CIEV:", "ERROR");
    }
    if (timeOk) {
      SerialsPrintln("Nhan gio thanh cong\n\nBat dau doc gio tu module GSM.",
                     P_SERIAL | P_SERIAL1);
      lcd.print(".");
      sendATCommand("AT+CCLK?", 3, 200, "CCLK:", "ERROR");
      timeFromString(gsmBuffer);
      lastReceiveTimeSignal = 1;
      lcd.setCursor(8, 1);
      lcd.print(
          printTime(1, 1, 21, gio_check, phut_check, giay_check).substring(9));
    } else
      SerialsPrintln("Khong the lay gio GPS", P_SERIAL | P_SERIAL1);

    SerialsPrintln("\nKiem Tra Tai Khoan...", P_SERIAL | P_SERIAL1);
    sendATCommand("AT+CUSD=1,\"*101#\",", 5, 5000, "CUSD:", "ERROR");
    String cusd = gsmBuffer.substring(gsmBuffer.indexOf(", \"") + 3,
                                      gsmBuffer.lastIndexOf("\","));
    SerialsPrintln("Chuoi CUSD:", P_SERIAL | P_SERIAL1);
    SerialsPrintln(cusd, P_SERIAL | P_SERIAL1);

    SerialsPrintln("\nThiet lap mode tin nhan...", P_SERIAL | P_SERIAL1);
    bool smsSetupOk = false;
    if (gsmModuleType == GSM_A7680) {
      smsSetupOk = sendATCommand("AT+CMGF=1", 5, 5000, "OK", "ERROR") &&
                   sendATCommand("AT+CNMI=1,1,0,0,1", 5, 5000, "OK", "ERROR");
    } else {
      smsSetupOk =
          sendATCommand("AT+CMGF=1;+CNMI=1,1,0,0,1", 5, 5000, "OK", "ERROR");
    }
    if (smsSetupOk) {

      if (gsmModuleType == GSM_A7680)
        sendATCommand("AT+CMGD=1,4", 5, 5000, "OK", "ERROR");
      else
        sendATCommand("AT+CMGDA=\"DEL ALL\"", 5, 5000, "OK", "ERROR");
      SerialsPrintln("Thiet lap tin nhan thanh cong", P_SERIAL | P_SERIAL1);
      lcd.setCursor(15, 0);
      lcd.print(".");

      if (sendATCommand("AT+CMGS=\"+84" + phone_master + "\"", 5, 60000, ">",
                        "ERROR")) {
        SerialsPrintln("Chuoi CUSD:", P_SERIAL | P_SERIAL1 | P_GSM);
        SerialsPrintln(cusd, P_SERIAL | P_SERIAL1 | P_GSM);
        SerialsPrintln("He Thong Nhac Khoi Dong", P_SERIAL | P_SERIAL1 | P_GSM);
        SerialsPrint(ver, P_GSM);
        sendATCommand("\032", 1, 30000, "OK", "ERROR");
        // gsm.print("\0032");
#if DEBUG
        SerialsPrint("So dien thoai phu(", P_SERIAL | P_SERIAL1);
        SerialsPrint(phone_second.length(), P_SERIAL | P_SERIAL1);
        SerialsPrint("): ", P_SERIAL | P_SERIAL1);
        SerialsPrintln(phone_second, P_SERIAL | P_SERIAL1);
#endif
        if (phone_second.length() == 9 &&
            sendATCommand("AT+CMGS=\"+84" + phone_second + "\"", 5, 60000, ">",
                          "ERROR")) {
          SerialsPrintln("Chuoi CUSD:", P_SERIAL | P_SERIAL1 | P_GSM);
          SerialsPrintln(cusd, P_SERIAL | P_SERIAL1 | P_GSM);
          SerialsPrintln("He Thong Nhac Khoi Dong",
                         P_SERIAL | P_SERIAL1 | P_GSM);
          SerialsPrint(ver, P_GSM);
          sendATCommand("\032", 1, 30000, "OK", "ERROR");
          // gsm.print("\0032");
        } else if (phone_second.length() == 9)
          SerialsPrintln("Loi khong gui duoc tin nhan phu",
                         P_SERIAL | P_SERIAL1);
        else
          SerialsPrintln("Khong co dien thoai phu", P_SERIAL | P_SERIAL1);

      } else
        SerialsPrintln("Loi khong gui duoc tin nhan chinh",
                       P_SERIAL | P_SERIAL1);

    } else
      SerialsPrintln("Loi thiet lap tin nhan", P_SERIAL | P_SERIAL1);
  }

  SerialsPrintln("He Thong Nhac San Sang", P_SERIAL | P_SERIAL1);

  lcd.setCursor(0, 0);
  lcd.print(ver);
  lcd.setCursor(0, 1);
  lcd.print("AUTO");

  SerialsPrintln("\nDoi Nhap Lenh: ", P_SERIAL | P_SERIAL1);
}
uint32_t holdBtnTime, lastManIndexTime, startBtnTime;
uint8_t playManIndex, keyState;

void checkButton() {
  if (millis() - startBtnTime < 20)
    return;

  bool button = digitalRead(BUTTON);

  if (button) {
    if (keyState == HOLD || keyState == PRESSED) {
      keyState = RELEASED;
    } else if (keyState == RELEASED)
      keyState = IDLE;
  } else {
    if (keyState == IDLE) {
      keyState = PRESSED;
      if (!mode) {
        playManIndex = (playManIndex + 1) % 10;
        lcd.setCursor(6, 1);
        lcd.print(playManIndex);
        lastManIndexTime = millis();
      }
      holdBtnTime = millis();
    } else if (keyState == PRESSED && (millis() - holdBtnTime) > 2000) {
      keyState = HOLD;
      if (musicPlayer.playingMusic) {
        musicPlayer.stopPlaying();
        SerialsPrintln("Dung bai hat dang chay", P_SERIAL | P_SERIAL1);
        digitalWrite(RELAY, LOW);
      }

      mode = !mode;
      playManIndex = 0;
      lcd.setCursor(0, 1);
      lcd.print(mode ? "AUTO   " : "MANUAL0");
    }
  }
  startBtnTime = millis();
}

void checkIR() {

  if (IrReceiver.decode()) {
    xulyhongngoai(IrReceiver.decodedIRData.command);
    SerialsPrintln("IR DECODING...", P_SERIAL | P_SERIAL1);
    IrReceiver.printIRResultShort(&Serial);
    SerialsPrintln("", P_SERIAL | P_SERIAL1);
    // IrReceiver.printIRResultRawFormatted(&Serial, true);
    IrReceiver.resume();
  }
}

void (*resetFunc)(void) = 0; // declare reset function at address 0
//------------------------------------------------------------------------------
void loop() {
  if (restart) {
    SerialsPrintln("Arduino restarting...", P_SERIAL | P_SERIAL1);
    restart = 0;
    resetFunc(); // Call reset
  }

  checkIR();
  checkButton();

  if (millis() - holdBtnTime > 10000) {
    if (!mode && playManIndex) {
      if (musicPlayer.playingMusic) {
        musicPlayer.stopPlaying();
        SerialsPrintln("Dung bai hat dang chay", P_SERIAL | P_SERIAL1);
      }

      digitalWrite(RELAY, HIGH);
      delay(2000);

      if (!playTrackByNumber(playManIndex))
        SerialsPrint("Khong co track ", P_SERIAL | P_SERIAL1);
      else
        SerialsPrint("Playing track ", P_SERIAL | P_SERIAL1);
      SerialsPrintln(playManIndex, P_SERIAL | P_SERIAL1);

      playManIndex = 0;
      lcd.setCursor(0, 1);
      lcd.print("MANUAL0");
    }
  }
  gsmBuffer = "";
  while (gsm.available()) {
    // checkIR();
    gsmBuffer += gsm.readString();
    SerialsPrint(gsmBuffer, P_SERIAL | P_SERIAL1);
    if (gsmBuffer.indexOf("+CMTI: ") > -1) {
      SerialsPrintln("Nhan duoc tin nhan moi", P_SERIAL | P_SERIAL1);
      have_sms = 1;
    }
    SerialsPrint("", P_SERIAL | P_SERIAL1);
  }

  if (have_sms) {
    have_sms = 0;
    SerialsPrintln("Xu Ly Tin Nhan...", P_SERIAL | P_SERIAL1);
    if (!sendATCommand("AT+CMGL=\"ALL\"", 5, 5000, "+CMGL:", "ERROR")) {
      if (sendATCommand("AT+CMGF=1;+CNMI=1,1,0,0,1", 5, 5000, "OK", "ERROR")) {
        if (!sendATCommand("AT+CMGL=\"ALL\"", 5, 5000, "+CMGL:", "ERROR"))
          SerialsPrint("Loi Khong Kiem tra duoc tin nhan",
                       P_SERIAL | P_SERIAL1);
        else
          have_sms = 1;
      } else
        SerialsPrint("Loi Thiet Lap Tin Nhan", P_SERIAL | P_SERIAL1);
    } else
      have_sms = 1;

    if (have_sms) {
      SerialsPrintln(gsmBuffer, P_SERIAL | P_SERIAL1);
      gsmBuffer = gsmBuffer.substring(gsmBuffer.indexOf("+CMGL:"));
      if ((gsmBuffer.indexOf(phone_master) > -1 &&
           gsmBuffer.indexOf(phone_master) < gsmBuffer.indexOf("\n")) ||
          (phone_second.length() == 9) &&
              gsmBuffer.indexOf(phone_second) > -1 &&
              gsmBuffer.indexOf(phone_second) < gsmBuffer.indexOf("\n")) {
        master = gsmBuffer.indexOf(phone_master) > -1;
        gsmBuffer = gsmBuffer.substring(gsmBuffer.indexOf("\n") + 1);
        gsmBuffer = gsmBuffer.substring(0, gsmBuffer.indexOf("\n") + 1);
        SerialsPrintln(gsmBuffer, P_SERIAL | P_SERIAL1);
        SerialsPrintln("Xu ly lenh tu tin nhan", P_SERIAL | P_SERIAL1);
        xulylenh(gsmBuffer);
      }
      if (gsmModuleType == GSM_A7680)
        sendATCommand("AT+CMGD=1,4", 5, 5000, "OK", "ERROR");
      else
        sendATCommand("AT+CMGDA=\"DEL ALL\"", 5, 5000, "OK", "ERROR");
      gsmBuffer = "";
      have_sms = 0;
    }
  }
  if (Serial.available() || Serial1.available()) {
    if (Serial.available()) {
      SerialsPrintln("Xu ly lenh tu may tinh", P_SERIAL | P_SERIAL1);
      xulylenh(Serial.readString());
    }
    if (Serial1.available()) {
      String bluCmd = Serial1.readString();
      SerialsPrintln("Xu ly lenh tu bluetooth", P_SERIAL | P_SERIAL1);
      SerialsPrintln((uint8_t)(bluCmd.charAt(0)), P_SERIAL | P_SERIAL1);
      if (bluCmd.length() > 2)
        xulylenh(bluCmd + "\n");
    }
    SerialsPrintln("\n\nDoi Nhap Lenh Moi: ", P_SERIAL | P_SERIAL1);
  }
  if (printMode)
    sendATCommand("AT+CCLK?", 1, 200, "CCLK:", "ERROR");
  else
    sendATCommandNoPrint("AT+CCLK?", 1, 200, "CCLK:", "ERROR");
  // sendATCommandNoPrint("AT+CCLK?",1,200,"CCLK:","ERROR");

  timeFromString(gsmBuffer);

  lcd.setCursor(8, 1);
  lcd.print(
      printTime(1, 1, 21, gio_check, phut_check, giay_check).substring(9));

  if (phut_old != phut_check) {
    phut_old = phut_check;
    SerialsPrint("so phut tinh tu lan cuoi nhan tin hieu GIO: ",
                 P_SERIAL | P_SERIAL1);
    SerialsPrintln(lastReceiveTimeSignal, P_SERIAL | P_SERIAL1);
    SerialsPrint("gsmBuffer: ", P_SERIAL | P_SERIAL1);
    SerialsPrintln(gsmBuffer, P_SERIAL | P_SERIAL1);
    if (!printMode)
      SerialsPrintln(printTime(ngay_check, thang_check, nam_check, gio_check,
                               phut_check, giay_check),
                     P_SERIAL | P_SERIAL1);
    if (lastReceiveTimeSignal > 2) {
      if (lastReceiveTimeSignal < 7) {
        SerialsPrint("gsmBuffer: ", P_SERIAL | P_SERIAL1);
        SerialsPrintln(gsmBuffer, P_SERIAL | P_SERIAL1);
        SerialsPrintln("Xu ly lenh tu gio GPS", P_SERIAL | P_SERIAL1);
        xulylenh(gsmBuffer);
      } else
        SerialsPrint(
            "Khong nhan duoc tin hieu thoi gian\nBo qua kiem tra lich nhac.",
            P_SERIAL | P_SERIAL1);
      if (!phut_old) {
        if (lastReceiveTimeSignal < 7)
          lastReceiveTimeSignal++;
        SerialsPrintln("Thiet lap gio GPS...", P_SERIAL | P_SERIAL1);
        bool timeOk = false;
        if (gsmModuleType == GSM_A7680) {
          sendATCommand("AT+CTZU=1", 3, 2000, "OK", "ERROR");
          sendATCommand("AT+COPS=2", 3, 10000, "OK", "ERROR");
          delay(2000);
          timeOk = sendATCommand("AT+COPS=0", 3, 60000, "CGEV:", "ERROR");
          if (timeOk)
            delay(5000); // Wait for network time URC to arrive
        } else {
          timeOk = sendATCommand("AT+CLTS=1;+COPS=2;+COPS=0", 10, 120000,
                                 "CIEV:", "ERROR");
        }
        if (timeOk) {
          SerialsPrintln(
              "Nhan gio thanh cong\n\nBat dau doc gio tu module GSM.",
              P_SERIAL | P_SERIAL1);
          sendATCommand("AT+CCLK?", 3, 200, "CCLK:", "ERROR");
          timeFromString(gsmBuffer);
          lastReceiveTimeSignal = 3;
        } else
          SerialsPrintln("Khong the lay gio GPS", P_SERIAL | P_SERIAL1);

      } else
        lastReceiveTimeSignal++;
    }
    if(lastReceiveTimeSignal < 8){
      xulylenh(gsmBuffer);
    }
  }
  if (!isMP3) { // Only periodically try to reinitialize if it failed
    if (millis() % 5000 <
        50) { // Throttle retries to ~5 seconds so it doesn't freeze the loop
      if (musicPlayer.begin()) {
        SD.begin(CARDCS);
        musicPlayer.setVolume(currentVolume, currentVolume);
        musicPlayer.useInterrupt(VS1053_FILEPLAYER_PIN_INT);
        SerialsPrintln("Mp3 khoi dong lai thanh cong.", P_SERIAL | P_SERIAL1);
        isMP3 = 1;
      }
    }
  }

  digitalWrite(RELAY, isMP3 && musicPlayer.playingMusic);

  // delay(100);
}

//------------------------------------------------------------------------------
uint8_t music_ls(Print *pr) {
  File root = SD.open("/");
  if (!root) {
    return ROOT_ERROR;
  }
  File entry;
  while ((entry = root.openNextFile())) {
    if (!entry.isDirectory()) {
      const char *f_name = entry.name();
      if (isFnMusic(f_name)) {
        pr->println(f_name);
      }
    }

    entry.close();
  }
  root.close();
}

//------------------------------------------------------------------------------
void SerialsPrint(String text, uint8_t output) {

  if (output & 2)
    Serial.print(text);
  if (output & 4)
    Serial1.print(text);
  if (output & 1) {
    if (smsBuffering) {
      // Buffer GSM output for multi-part SMS sending
      smsBuffer += text;
    } else {
      // For GSM module: replace \n with \r\n for AT command compatibility
      if (text.endsWith("\n")) {
        gsm.print(text.substring(0, text.length() - 1));
        gsm.print("\r\n");
      } else {
        gsm.print(text);
      }
    }
  }
}

// Send buffered SMS content split into parts that fit SMS limit
// Each \n in buffer becomes \r\n (2 bytes) on wire, so we must count actual size
#define SMS_MAX_SIZE 150  // Safe limit (160 minus overhead margin)

void sendBufferedSMS(String phone) {
  if (smsBuffer.length() == 0) return;
  
  int totalLen = smsBuffer.length();
  int partStart = 0;
  
  while (partStart < totalLen) {
    // Calculate how many buffer chars fit in SMS_MAX_SIZE actual bytes
    int smsSize = 0;
    int partEnd = partStart;
    int lastNewline = -1;
    
    while (partEnd < totalLen && smsSize < SMS_MAX_SIZE) {
      if (smsBuffer[partEnd] == '\n') {
        smsSize += 2;  // \n becomes \r\n on wire
        lastNewline = partEnd;
      } else if (smsBuffer[partEnd] != '\r') {
        smsSize++;
      }
      if (smsSize <= SMS_MAX_SIZE)
        partEnd++;
    }
    
    // If we stopped mid-text and there's a newline, break there for cleaner SMS
    if (partEnd < totalLen && lastNewline > partStart) {
      partEnd = lastNewline + 1;
    }
    
    // Fallback: if no progress, force advance by 1 char
    if (partEnd == partStart) partEnd++;
    
    String part = smsBuffer.substring(partStart, partEnd);
    part.trim();
    
    if (part.length() > 0) {
      if (sendATCommand("AT+CMGS=\"+84" + phone + "\"", 5, 60000, ">", "ERROR")) {
        // Send the part content directly to GSM
        for (unsigned int i = 0; i < part.length(); i++) {
          if (part[i] == '\n') {
            gsm.print("\r\n");
          } else {
            gsm.print(part[i]);
          }
        }
        sendATCommand("\032", 1, 30000, "OK", "ERROR");
        // Wait for network to process before sending next part
        delay(3000);
      } else {
        SerialsPrintln("Loi gui SMS phan tiep theo", P_SERIAL | P_SERIAL1);
        break;
      }
    }
    
    partStart = partEnd;
  }
  
  smsBuffer = "";
}

void SerialsPrint(const char *text, uint8_t output) {
  SerialsPrint(String(text), output);
}

void SerialsPrint(int text, uint8_t output) {
  SerialsPrint(String(text), output);
}

void SerialsPrintln(String text, uint8_t output) {
  SerialsPrint(text + "\n", output);
}

void SerialsPrintln(const char *text, uint8_t output) {
  SerialsPrint(String(text) + "\n", output);
}

void SerialsPrintln(int text, uint8_t output) {
  SerialsPrint(String(text) + "\n", output);
}

//------------------------------------------------------------------------------

String checkLichNhac() {
  File file = SD.open("lichnhac.txt");
  if (!file)
    return String("ER");
  while (file.available()) {
    String lineBuffer = file.readStringUntil('\n');
    lineBuffer.trim();
    lineBuffer.toUpperCase();

    SerialsPrintln(lineBuffer, P_SERIAL | P_SERIAL1);
    SerialsPrintln(gio_check, P_SERIAL | P_SERIAL1);
    SerialsPrintln(phut_check, P_SERIAL | P_SERIAL1);
    SerialsPrintln(lineBuffer.substring( 7, 9).toInt(), P_SERIAL | P_SERIAL1);
    SerialsPrintln(lineBuffer.substring(10,12).toInt(), P_SERIAL | P_SERIAL1);
    SerialsPrintln(gio_check  == lineBuffer.substring( 7, 9).toInt(), P_SERIAL | P_SERIAL1);
    SerialsPrintln(phut_check == lineBuffer.substring(10,12).toInt(), P_SERIAL | P_SERIAL1);

    if (lineBuffer.length() > 13 &&
        (lineBuffer[0] == 'T' || lineBuffer[0] == 'D' || lineBuffer[0] == 'C' ||
         lineBuffer[0] == 'A' || lineBuffer[0] == 'L') &&
        gio_check == lineBuffer.substring(7, 9).toInt() &&
        phut_check == lineBuffer.substring(10, 12).toInt()) {

      SerialsPrintln(String(lineBuffer[0]), P_SERIAL | P_SERIAL1);
      switch (lineBuffer[0]) {
      case 'D':
        if ((lineBuffer[1] - 48 || lineBuffer[2] - 48) &&
            (ngay_check != lineBuffer.substring(1, 3).toInt()))
          break;
      case 'T':
      case 'C':
        if (lineBuffer[0] - 'D' &&
            (tuan_check[0] != lineBuffer[0] || tuan_check[1] != lineBuffer[1] ||
             (lineBuffer[2] - 48 && tuan_check[2] != lineBuffer[2])))
          break;
        if ((lineBuffer[4] - 48 || lineBuffer[5] - 48) &&
            (thang_check != lineBuffer.substring(4, 6).toInt()))
          break;
      case 'L':
        if (lineBuffer[1] == 'P' && (ngay_check != ngay_phuc_sinh_check ||
                                     thang_check != thang_phuc_sinh_check))
          break;
        if (lineBuffer[1] == 'T' && (ngay_check != ngay_thang_thien_check ||
                                     thang_check != thang_thang_thien_check))
          break;
        if (lineBuffer[1] == 'H' && (ngay_check != ngay_hien_xuong_check ||
                                     thang_check != thang_hien_xuong_check))
          break;
      case 'A':
        if (lineBuffer[0] == 'A') {
          if (((lineBuffer[1] - 48 || lineBuffer[2] - 48) &&
               (ngay_am_check != lineBuffer.substring(1, 3).toInt())) ||
              ((lineBuffer[4] - 48 || lineBuffer[5] - 48) &&
               (thang_am_check != lineBuffer.substring(4, 6).toInt())))
            break;
        }
        if (lineBuffer[13] == '!') {
          restart = 1;
          file.close();
          return String("X");
        }
        String ret = lineBuffer[13] - 'X' ? lineBuffer.substring(13) + ".MP3"
                                          : String("X");

        file.close();
        return ret;
      }
    }
  }

  file.close();
  return String("X");
}

void ngayPhucSinh(uint8_t nam) {
  File easterFile = SD.open("easter.csv");
  if (!easterFile) {
    SerialsPrintln("Loi mo file lich easter", P_SERIAL | P_SERIAL1);
  }
  easterFile.seek(17 * nam);
  if (easterFile.available()) {
    String ngayLe = easterFile.readStringUntil('\n');
    ngay_phuc_sinh_check = ngayLe.substring(0, 2).toInt();
    thang_phuc_sinh_check = ngayLe.substring(2, 4).toInt();
    ngay_thang_thien_check = ngayLe.substring(5, 7).toInt();
    thang_thang_thien_check = ngayLe.substring(7, 9).toInt();
    ngay_hien_xuong_check = ngayLe.substring(10, 12).toInt();
    thang_hien_xuong_check = ngayLe.substring(12, 14).toInt();
#if DEBUG
    SerialsPrintln("Cac ngay lien quan le phuc sinh", P_SERIAL | P_SERIAL1);
    SerialsPrint("Le phuc sinh: ", P_SERIAL | P_SERIAL1);
    SerialsPrint(ngay_phuc_sinh_check, P_SERIAL | P_SERIAL1);
    SerialsPrint("/", P_SERIAL | P_SERIAL1);
    SerialsPrintln(thang_phuc_sinh_check, P_SERIAL | P_SERIAL1);
    SerialsPrint("Le thang thien: ", P_SERIAL | P_SERIAL1);
    SerialsPrint(ngay_thang_thien_check, P_SERIAL | P_SERIAL1);
    SerialsPrint("/", P_SERIAL | P_SERIAL1);
    SerialsPrintln(thang_thang_thien_check, P_SERIAL | P_SERIAL1);
    SerialsPrint("Le hien xuong: ", P_SERIAL | P_SERIAL1);
    SerialsPrint(ngay_hien_xuong_check, P_SERIAL | P_SERIAL1);
    SerialsPrint("/", P_SERIAL | P_SERIAL1);
    SerialsPrintln(thang_hien_xuong_check, P_SERIAL | P_SERIAL1);
#endif
  }
  easterFile.close();
  nam_old = nam;
}

uint8_t validDateTime(uint8_t ngay, uint8_t thang, uint8_t nam, uint8_t gio,
                      uint8_t phut, uint8_t giay) {
  uint8_t erCode = 0;
  if (gio > 23)
    erCode |= LOI_GIO;
  if (phut > 59)
    erCode |= LOI_PHUT;
  if (giay > 59)
    erCode |= LOI_GIAY;
  if (nam > 99)
    erCode |= LOI_NAM;
  if (!thang || thang > 12)
    erCode |= LOI_THANG;
  if (!ngay || ngay > daysInMonth[thang * (thang != 2 || nam % 4)])
    erCode |= LOI_NGAY;
  return erCode;
}

void ngayAmLich(uint8_t ngay, uint8_t thang, uint8_t nam) {
  File csvFile;
  uint16_t pos = (thang - 1) * 5 + (ngay - 1) * 62;
#if DEBUG
  SerialsPrint("Vi tri con tro: ", P_SERIAL | P_SERIAL1);
  SerialsPrintln(pos, P_SERIAL | P_SERIAL1);
  SerialsPrint("ten file: ", P_SERIAL | P_SERIAL1);
  SerialsPrintln(nam, P_SERIAL | P_SERIAL1);
#endif
  csvFile = SD.open(String(nam).c_str());
  if (!csvFile) {
    SerialsPrint("Loi mo file am lich nam ", P_SERIAL | P_SERIAL1);
    SerialsPrintln(2000 + nam, P_SERIAL | P_SERIAL1);
  }
  csvFile.seek(pos);
  if (csvFile.available()) {
    String lunar = csvFile.readStringUntil(',');
    ngay_am_check = lunar.substring(0, 2).toInt();
    thang_am_check = lunar.substring(2).toInt();
#if DEBUG
    SerialsPrint("Ngay am lich la: ", P_SERIAL | P_SERIAL1);
    SerialsPrint(ngay_am_check, P_SERIAL | P_SERIAL1);
    SerialsPrint("/", P_SERIAL | P_SERIAL1);
    SerialsPrintln(thang_am_check, P_SERIAL | P_SERIAL1);
#endif
  }
  csvFile.close();
  nam_old = nam;
  ngay_old = ngay;
  thang_old = thang;
}

void weekday(uint8_t ngay, uint8_t thang, uint8_t nam) {
  int check = (23 * thang / 9 + ngay + (thang > 2 ? !(nam % 4) : 2) + nam +
               (nam + 3) / 4 + 1);
  tuan_check = thu[(23 * thang / 9 + ngay + (thang > 2 ? !(nam % 4) : 2) + nam +
                    (nam + 3) / 4 + 1) %
                   7] +
               String(ngay + 6 < daysInMonth[thang * (thang != 2 || nam % 4)]
                          ? ngay / 7 + 1
                          : 5);
#if DEBUG
  SerialsPrint("tuan va thu tu tuan la: ", P_SERIAL | P_SERIAL1);
  SerialsPrintln(tuan_check, P_SERIAL | P_SERIAL1);
  SerialsPrint("check: ", P_SERIAL | P_SERIAL1);
  SerialsPrintln(check, P_SERIAL | P_SERIAL1);
#endif
}

bool timeFromString(String dateTime) {
  dateTime.trim();
  uint8_t ngay_tam, thang_tam, nam_tam, gio_tam, phut_tam, giay_tam;
  int pos = dateTime.indexOf("\"");
#if DEBUG
  // SerialsPrint("vi tri thoi gian: ", P_SERIAL | P_SERIAL1);
  // SerialsPrintln(pos, P_SERIAL | P_SERIAL1);
  // SerialsPrintln(dateTime, P_SERIAL | P_SERIAL1);
#endif
  if (pos > -1) {
    ngay_tam = dateTime.substring(pos + 7, pos + 9).toInt();
    thang_tam = dateTime.substring(pos + 4, pos + 6).toInt();
    nam_tam = dateTime.substring(pos + 1, pos + 3).toInt();
    gio_tam = dateTime.substring(pos + 10, pos + 12).toInt();
    phut_tam = dateTime.substring(pos + 13, pos + 15).toInt();
    giay_tam = dateTime.substring(pos + 16, pos + 18).toInt();
  } else {
    ngay_tam = dateTime.substring(0, 2).toInt();
    thang_tam = dateTime.substring(2, 4).toInt();
    nam_tam = dateTime.substring(4, 6).toInt();
    gio_tam = dateTime.substring(6, 8).toInt();
    phut_tam = dateTime.substring(8, 10).toInt();
    giay_tam = dateTime.substring(10, 12).toInt();
  }

  if (printMode) {
    SerialsPrintln("chuoi thoi gian convert: ", P_SERIAL | P_SERIAL1);
    SerialsPrintln(
        printTime(ngay_tam, thang_tam, nam_tam, gio_tam, phut_tam, giay_tam),
        P_SERIAL | P_SERIAL1);
  }

  if (!validDateTime(ngay_tam, thang_tam, nam_tam, gio_tam, phut_tam,
                     giay_tam)) {
    ngay_check = ngay_tam;
    thang_check = thang_tam;
    nam_check = nam_tam;
    gio_check = gio_tam;
    phut_check = phut_tam;
    giay_check = giay_tam;
    return 0;
  }
  return 1;
}

//------------------------------------------------------------------------------

uint8_t xulyFile(uint8_t cmdCode, String cmd, int line_check) {
  File file, file2;
  if (cmdCode != 1) {
    SD.remove("lichtemp.txt");
    file2 = SD.open("lichtemp.txt", FILE_WRITE);
    if (!file2) {
      return TEMP_ERROR;
    }
  }

  cmd.trim();

  file = SD.open(cmdCode == 1 ? cmd.c_str() : "lichnhac.txt");
  if (file) {

    SerialsPrint("thuc hien lenh ", P_SERIAL | P_SERIAL1 | have_sms);
    if (cmdCode == READ) {
      SerialsPrint("doc file ", P_SERIAL | P_SERIAL1 | have_sms);
    }
    if (cmdCode == ADD)
      SerialsPrint("them: ", P_SERIAL | P_SERIAL1 | have_sms);
    if (cmdCode == DEL)
      SerialsPrint("xoa", P_SERIAL | P_SERIAL1 | have_sms);
    if (cmd.length())
      SerialsPrint(cmd, P_SERIAL | P_SERIAL1 | have_sms);
    if (line_check) {
      SerialsPrint(" dong thu ", P_SERIAL | P_SERIAL1 | have_sms);
      SerialsPrint(line_check, P_SERIAL | P_SERIAL1 | have_sms);
    }
    SerialsPrintln("", P_SERIAL | P_SERIAL1 | have_sms);

    String lineBuffer;
    uint8_t line = 0;
    if (line_check < 1 && cmdCode == ADD) {
      file2.println(cmd);
      SerialsPrint("** ", P_SERIAL | P_SERIAL1 | have_sms);
      SerialsPrintln(cmd, P_SERIAL | P_SERIAL1 | have_sms);
    }
    while (file.available()) {
      lineBuffer = file.readStringUntil('\n');
      lineBuffer.trim();
      if (lineBuffer[0] == 'T' || lineBuffer[0] == 'D' ||
          lineBuffer[0] == 'C' || lineBuffer[0] == 'A' ||
          lineBuffer[0] == 'L') {

        SerialsPrint(
            (line_check == ++line && cmdCode == DEL)
                ? String("X")
                : String(line + (line > line_check) * (cmdCode % 3 - 1)),
            P_SERIAL | P_SERIAL1 | have_sms);
        SerialsPrint(". ", P_SERIAL | P_SERIAL1 | have_sms);
      }
      if (cmdCode == ADD || (line_check && line_check != line))
        file2.println(lineBuffer);
      SerialsPrintln(lineBuffer, P_SERIAL | P_SERIAL1 | have_sms);
      if (cmdCode == ADD && line_check == line) {
        file2.println(cmd);
        SerialsPrint("** ", P_SERIAL | P_SERIAL1 | have_sms);
        SerialsPrintln(cmd, P_SERIAL | P_SERIAL1 | have_sms);
      }
    }
    if (line_check > line && cmdCode == ADD) {
      file2.println(cmd);
      SerialsPrint("** ", P_SERIAL | P_SERIAL1 | have_sms);
      SerialsPrintln(cmd, P_SERIAL | P_SERIAL1 | have_sms);
    }

  } else {
    file.close();
    file2.close();
    return OPEN_ERROR;
  }

  file.close();
  file2.close();
  if (line_check) {
    if (!SD.remove("lichnhac.txt"))
      return REMO_ERROR;
    if (!sdRename("lichtemp.txt", "lichnhac.txt"))
      return RENA_ERROR;
  }
  return 0;
}

uint8_t sdCopy(const char *srcName, const char *desName) {
  uint8_t erCode = 0;
  File srcFile = SD.open(srcName);
  if (srcFile) {
    SD.remove(desName);
    File desFile = SD.open(desName, FILE_WRITE);
    if (desFile) {
      while (srcFile.available()) {
        desFile.write(srcFile.read());
      }
      desFile.close();
    } else
      erCode = DES_ERROR;
    srcFile.close();
  } else
    erCode = SRC_ERROR;
  return erCode;
}

//------------------------------------------------------------------------------
void xulylenh(String cmd) {
  SerialsPrint("Lenh nhan duoc: ", P_SERIAL | P_SERIAL1);
  SerialsPrintln(cmd, P_SERIAL | P_SERIAL1);
  // if(!isMP3)SerialsPrintln("Khong The Xu Ly Lenh Khi Khong Co Module MP3",
  // P_SERIAL | P_SERIAL1);
  if (cmd.indexOf("\n") == -1)
    return;

  CMDCODE cmdCode;
  uint8_t erCode;
  int line_check;
  File pFile;

  cmd.trim();
  cmd.toUpperCase();

  erCode = line_check = 0;
  cmdCode = NONE;

#if DEBUG
  if (cmd.indexOf("TEST") > -1) {
    cmd = cmd.substring(4);
    cmd.trim();
    gsm.println(cmd);
  }
#endif
  String cusd;
  if (have_sms) {
    if (sendATCommand("AT+CUSD=1,\"*101#\",", 5, 5000, "CUSD:", "ERROR")) {
      cusd = gsmBuffer.substring(gsmBuffer.indexOf(", \"") + 3,
                                 gsmBuffer.lastIndexOf("\",")) +
             "\n";
    } else
      SerialsPrintln("CUSD That Bai", P_SERIAL | P_SERIAL1);
  }

  if (have_sms && !master) {
    smsBuffer = "";
    smsBuffering = true;
    SerialsPrint("CUSD: ", P_SERIAL | P_SERIAL1 | have_sms);
    SerialsPrintln(cusd, P_SERIAL | P_SERIAL1 | have_sms);
    SerialsPrintln("Phone phu gui lenh: ", P_SERIAL | P_SERIAL1 | have_sms);
    SerialsPrintln(cmd, P_SERIAL | P_SERIAL1 | have_sms);
    smsBuffering = false;
    sendBufferedSMS(phone_master);
  }

  if (cmd.indexOf("READ") > -1 || cmd.indexOf("DOC") > -1)
    cmdCode = READ;
  else if (cmd.indexOf("ADD") > -1 || cmd.indexOf("GHI") > -1 ||
           cmd.indexOf("THEM") > -1)
    cmdCode = ADD;
  else if (cmd.indexOf("DEL") > -1 || cmd.indexOf("XOA") > -1)
    cmdCode = DEL;
  else if (cmd.indexOf("MAN") > -1)
    cmdCode = MAN;
  else if (cmd.indexOf("AUTO") > -1)
    cmdCode = AUTO;
  else if (cmd.indexOf("STOP") > -1 || cmd.indexOf("DUNG") > -1)
    cmdCode = STOP;
  else if (cmd.indexOf("PLAY") > -1)
    cmdCode = PLAY;
  else if (cmd.indexOf("SONG") > -1 || cmd.indexOf("BAIHAT") > -1)
    cmdCode = SONG;
  else if (cmd.indexOf("COM") > -1 || cmd.indexOf("THU") > -1)
    cmdCode = COM;
  else if (cmd.indexOf("RESET") > -1 || cmd.indexOf("DATLAI") > -1)
    cmdCode = RESET;
  else if (cmd.indexOf("FACT") > -1 || cmd.indexOf("GOC") > -1)
    cmdCode = FACTORY;
  else if (cmd.indexOf("DEF") > -1)
    cmdCode = DEF;
  else if (cmd.indexOf("STC") > -1)
    cmdCode = STC;
  else if (cmd.indexOf("+CCLK: ") > -1)
    cmdCode = GPS;
  else if (cmd.indexOf("RTC") > -1)
    cmdCode = RTC;
  else if (cmd.indexOf("COM") > -1)
    cmdCode = COM;

  else if (cmd.indexOf("TAIKHOAN") > -1 || cmd.indexOf("TK") > -1)
    cmdCode = TK;
  else if (cmd.indexOf("PRINT") > -1)
    cmdCode = PRINT;
  else if (cmd.indexOf("DT") > -1)
    cmdCode = PHONE;
  else if (cmd.indexOf("BAOCAO") > -1 || cmd.indexOf("BC") > -1)
    cmdCode = INFO;
  else if (cmd.indexOf("SIM") > -1)
    cmdCode = SIM;
  else if (cmd.indexOf("AT") > -1)
    cmdCode = ATC;
  else if (cmd.indexOf("RESTART") > -1 || cmd.indexOf("KHOIDONG") > -1)
    cmdCode = KDL;

  SerialsPrint("cmdCode: ", P_SERIAL | P_SERIAL1);
  SerialsPrintln(cmdCode, P_SERIAL | P_SERIAL1);

  if (cmdCode == ATC) {
  }
  if ((musicPlayer.playingMusic && cmdCode < STOP) ||
      (!isMP3 && cmdCode < MAN)) {
    cmdCode = NONE;
    erCode = isMP3 ? PLAYING_ERROR : MP3_ERROR;
  }
  if (have_sms && cmdCode != TK) {
    // Start buffering SMS content instead of opening SMS session directly
    smsBuffer = "";
    smsBuffering = true;
    SerialsPrint("CUSD: ", P_SERIAL | P_SERIAL1 | have_sms);
    SerialsPrintln(cusd, P_SERIAL | P_SERIAL1 | have_sms);
  }
  if (cmdCode == KDL) {
    restart = 1;
  }
  if (cmdCode >= STC && cmdCode <= COM) {
    SerialsPrintln("Nhan duoc chuoi thoi gian", P_SERIAL | P_SERIAL1);
    SerialsPrintln(cmd, P_SERIAL | P_SERIAL1);
    if (mode == (cmdCode > RTC)) {
      SerialsPrint("Khong the kiem tra gio nhac ",
                   P_SERIAL | P_SERIAL1 | have_sms);
      SerialsPrint(cmdCode, P_SERIAL | P_SERIAL1 | have_sms);
      SerialsPrint(" o che do ", P_SERIAL | P_SERIAL1 | have_sms);
      SerialsPrintln(mode ? "Auto" : "Maunal", P_SERIAL | P_SERIAL1 | have_sms);

    } else {
      if (mode && musicPlayer.playingMusic)
        erCode = PLAYING_ERROR;
      else {
        if (musicPlayer.playingMusic) {
          musicPlayer.stopPlaying();
          SerialsPrintln("Dung bai hat dang chay", P_SERIAL | P_SERIAL1);
        }

        if (timeFromString(cmd.substring(3)))
          erCode = TIME_ERROR;
        else {
          if (cmdCode == STC) {
            lastReceiveTimeSignal = 0;
            sendATCommand(String("AT+CCLK=\"") +
                              printTime(nam_check, thang_check, ngay_check,
                                        gio_check, phut_check, giay_check) +
                              String("+28\""),
                          5, 1000, "OK", "ERROR");
          }
          if (!mode || ngay_check != ngay_old || thang_check != thang_old ||
              nam_check != nam_old) {
            if (!mode || nam_check != nam_old)
              ngayPhucSinh(nam_check);
            ngayAmLich(ngay_check, thang_check, nam_check);
            /**tuan*/
            weekday(ngay_check, thang_check, nam_check);
            //              if(cmdCode != COM && gio_check == 10 && phut_check
            //              ==50){
            //                SerialsPrintln("Chuan Bi Reset Arduino",
            //                P_SERIAL | P_SERIAL1); restart = 1;
            //              }
          }
          SerialsPrint("Ngay am lich la: ", P_SERIAL | P_SERIAL1);
          SerialsPrint(ngay_am_check, P_SERIAL | P_SERIAL1);
          SerialsPrint("/", P_SERIAL | P_SERIAL1);
          SerialsPrintln(thang_am_check, P_SERIAL | P_SERIAL1);
          SerialsPrint("tuan: ", P_SERIAL | P_SERIAL1 | have_sms);
          SerialsPrintln(tuan_check, P_SERIAL | P_SERIAL1 | have_sms);

          SerialsPrintln("Checking Nhac.....", P_SERIAL | P_SERIAL1);
          cmd = checkLichNhac();
          SerialsPrint("Kiem tra lich nhac: ", P_SERIAL | P_SERIAL1);
          SerialsPrintln(cmd, P_SERIAL | P_SERIAL1);

          if (cmd.indexOf(".MP3") > -1) {
            digitalWrite(RELAY, HIGH);
            delay(2000);
            if (!playMP3File(cmd))
              erCode = PLAY_ERROR;
            if (!erCode) {
              SerialsPrint("Playing...", P_SERIAL | P_SERIAL1 | have_sms);
              SerialsPrintln(cmd, P_SERIAL | P_SERIAL1 | have_sms);
            }
            digitalWrite(RELAY, musicPlayer.playingMusic);
          } else if (cmd.indexOf("ER") > -1)
            erCode = LICH_ERROR;
          else if (cmd.indexOf("X") > -1)
            SerialsPrintln("Khong Co Lich Phat Nhac",
                           P_SERIAL | P_SERIAL1 | have_sms);
        }
      }
    }
  }
  if (restart) {
    SerialsPrint("Chuan bi khoi dong lai he thong nhac",
                 P_SERIAL | P_SERIAL1 | have_sms);
  }
  switch (cmdCode) {
  case READ:

    cmd =
        (cmd.indexOf(".TXT") > -1 ? (cmd.substring(cmd.indexOf("READ") + 4,
                                                   cmd.lastIndexOf(".TXT") + 4))
         : cmd.indexOf(".CSV") > -1
             ? (cmd.substring(cmd.indexOf("READ") + 4,
                              cmd.lastIndexOf(".CSV") + 4))
             : "lichnhac.txt");
    erCode = xulyFile(cmdCode, cmd, line_check);

    break;
  case ADD:
    line_check =
        cmd.substring(cmd.indexOf("ADD") + 3, cmd.indexOf(" ")).toInt();
    if (!line_check)
      line_check =
          cmd.substring(cmd.indexOf("GHI") + 3, cmd.indexOf(" ")).toInt();
    if (!line_check)
      line_check =
          cmd.substring(cmd.indexOf("THEM") + 4, cmd.indexOf(" ")).toInt();
    if (cmd.indexOf("ADD0") >= 0)
      line_check = -1;
    if (!line_check) {
      SerialsPrint("Hang them vao khong xac dinh",
                   P_SERIAL | P_SERIAL1 | have_sms);
      break;
    }
    cmd = cmd.substring(cmd.indexOf(" ") + 1);
    erCode = xulyFile(cmdCode, cmd, line_check);

    break;

  case DEL:
    SerialsPrintln("line_check", P_SERIAL | P_SERIAL1 | have_sms);
    line_check = cmd.substring(cmd.indexOf("DEL") + 3).toInt();
    if (!line_check)
      line_check = cmd.substring(cmd.indexOf("XOA") + 3).toInt();
    SerialsPrintln(line_check, P_SERIAL | P_SERIAL1 | have_sms);
    if (line_check <= 0) {
      SerialsPrint("Hang can xoa khong hop le",
                   P_SERIAL | P_SERIAL1 | have_sms);
      break;
    }

    erCode = xulyFile(cmdCode, cmd, line_check);

    break;
  case RESET:
    erCode = sdCopy("lichbak.txt", "lichtemp.txt");
    if (!erCode) {
      if (!SD.remove("lichnhac.txt"))
        erCode = REMO_ERROR;
      if (!sdRename("lichtemp.txt", "lichnhac.txt"))
        erCode = RENA_ERROR;
      if (!erCode)
        SerialsPrintln("reset file chuan thanh cong",
                       P_SERIAL | P_SERIAL1 | have_sms);
    }
    break;
  case FACTORY:
    erCode = sdCopy("lichgoc.txt", "lichtemp.txt");
    if (!erCode) {
      if (!SD.remove("lichnhac.txt"))
        erCode = REMO_ERROR;
      if (!sdRename("lichtemp.txt", "lichnhac.txt"))
        erCode = RENA_ERROR;
      if (!erCode)
        SerialsPrintln("reset file goc thanh cong",
                       P_SERIAL | P_SERIAL1 | have_sms);
    }
    break;
  case DEF:
    erCode = sdCopy("lichnhac.txt", "lichtemp.txt");
    if (!erCode) {
      if (!SD.remove("lichbak.txt"))
        erCode = REMO_ERROR;
      if (!sdRename("lichtemp.txt", "lichbak.txt"))
        erCode = RENA_ERROR;
      if (!erCode)
        SerialsPrintln("dat phai chuan thanh cong",
                       P_SERIAL | P_SERIAL1 | have_sms);
    }
    break;
  case MAN:
  case AUTO:
    mode = cmdCode - MAN;
    SerialsPrint("Chuyen qua che do ", P_SERIAL | P_SERIAL1 | have_sms);
    SerialsPrintln(mode ? "Auto" : "Manual", P_SERIAL | P_SERIAL1 | have_sms);
    lcd.setCursor(0, 1);
    lcd.print(mode ? "AUTO   " : "MANUAL0");
    break;
  case PLAY:
    if (mode) {
      SerialsPrintln("Chi co the choi nhac o che do Manual",
                     P_SERIAL | P_SERIAL1);
      break;
    }

    if (musicPlayer.playingMusic) {
      musicPlayer.stopPlaying();
      SerialsPrintln("Dung bai hat dang chay", P_SERIAL | P_SERIAL1 | have_sms);
    }

    if (cmd.indexOf(".MP3"))
      cmd = cmd.substring(cmd.indexOf("PLAY") + 4, cmd.lastIndexOf(".MP3") + 4);
    else
      erCode = NAME_ERROR;
    cmd.trim();
    if (cmd.length() < 5)
      erCode = NAME_ERROR;

    if (!erCode) {
      SerialsPrintln(cmd, P_SERIAL | P_SERIAL1 | have_sms);
      digitalWrite(RELAY, HIGH);
      delay(2000);
      if (!playMP3File(cmd))
        erCode = PLAY_ERROR;
      if (!erCode) {
        SerialsPrint("Playing...", P_SERIAL | P_SERIAL1 | have_sms);
        SerialsPrintln(cmd, P_SERIAL | P_SERIAL1 | have_sms);
      }
      digitalWrite(RELAY, musicPlayer.playingMusic);
    }
    break;
  case STOP:
    if (mode) {
      SerialsPrintln("Chi co the dung choi nhac o che do Manual",
                     P_SERIAL | P_SERIAL1 | have_sms);
      break;
    }
    if (musicPlayer.playingMusic) {
      musicPlayer.stopPlaying();
      SerialsPrintln("Dung choi bai hat", P_SERIAL | P_SERIAL1 | have_sms);

    } else
      SerialsPrintln("Dang khong co nhac duoc choi",
                     P_SERIAL | P_SERIAL1 | have_sms);
    break;
  case SONG:
    music_ls(&Serial);
    music_ls(&Serial1);
    if (have_sms && smsBuffering) {
      // Buffer song list for multi-part SMS instead of writing directly
      File songRoot = SD.open("/");
      if (songRoot) {
        File songEntry;
        while ((songEntry = songRoot.openNextFile())) {
          if (!songEntry.isDirectory() && isFnMusic(songEntry.name())) {
            SerialsPrintln(songEntry.name(), have_sms);
          }
          songEntry.close();
        }
        songRoot.close();
      }
    } else {
      music_ls(&gsm);
    }
    SerialsPrintln("\nDone", P_SERIAL | P_SERIAL1 | have_sms);
    break;
  case STC:
  case GPS:
  case RTC:
  case COM:
  case KDL:
    break;

  case TK:
    sendATCommand("AT+CUSD=1,\"*101#\",", 5, 5000, "CUSD:", "ERROR");
    gsmBuffer = gsmBuffer.substring(gsmBuffer.indexOf(", \"") + 3,
                                    gsmBuffer.lastIndexOf("\","));
    if (have_sms) {
      smsBuffer = "";
      smsBuffering = true;
    }
    SerialsPrintln("Chuoi CUSD:", P_SERIAL | P_SERIAL1 | have_sms);
    SerialsPrintln(gsmBuffer, P_SERIAL | P_SERIAL1 | have_sms);
    break;

  case PHONE:
    if (!master) {
      erCode = MASTER_ERROR;
      break;
    }
    cusd = "";
    if (cmd[3] == '+' && cmd[4] == '8' && cmd[5] == '4' && cmd.length() > 14 &&
        cmd.substring(6, 15).toInt())
      phone_second = cusd = cmd.substring(6, 15);
    else if (cmd[3] == '0' && cmd.length() > 12 && cmd.substring(4, 13).toInt())
      phone_second = cusd = cmd.substring(4, 13);
    else if (cmd[3] == 'X')
      phone_second = cusd = "";

    if (cusd.length() == 9 || cmd[3] == 'X') {
      SD.remove("PHONE.TXT");
      pFile = SD.open("PHONE.TXT", FILE_WRITE);
      if (pFile)
        pFile.print(cusd);
      else
        erCode = OPEN_ERROR;
      pFile.close();
      if (cmd[3] == 'X')
        SerialsPrint("Da Xoa So Dien Thoai Phu",
                     P_SERIAL | P_SERIAL1 | have_sms);
      else {
        SerialsPrint("So dien thoai phu hien tai la: ",
                     P_SERIAL | P_SERIAL1 | have_sms);
        SerialsPrint(phone_second, P_SERIAL | P_SERIAL1 | have_sms);
      }
    } else
      SerialsPrint("So Dien Thoai Khong Hop Le",
                   P_SERIAL | P_SERIAL1 | have_sms);

    SerialsPrintln("", P_SERIAL | P_SERIAL1);
    if (have_sms) {
      smsBuffering = false;
      sendBufferedSMS(master ? phone_master : phone_second);
    }
    if (cusd.length() == 9) {
      SerialsPrintln("Nhan tin thong bao dien thoai phu.",
                     P_SERIAL | P_SERIAL1);
      if (sendATCommand("AT+CMGS=\"+84" + phone_second + "\"", 5, 60000,
                               ">", "ERROR")) {
        gsm.print("Dien thoai cua ban vua duoc dat lam dien thoai phu.");
        sendATCommand("\032", 1, 30000, "OK", "ERROR");
      }
    }
    have_sms = 0; // Already sent via sendBufferedSMS above, prevent double send

    break;
  case PRINT:
    printMode = !printMode;
    SerialsPrint("Doi che do print thoi gian thanh: ",
                 P_SERIAL | P_SERIAL1 | have_sms);
    SerialsPrintln(printMode, P_SERIAL | P_SERIAL1 | have_sms);
    break;

  case INFO:

    SerialsPrint("mode: ", P_SERIAL | P_SERIAL1 | have_sms);
    SerialsPrintln(mode, P_SERIAL | P_SERIAL1 | have_sms);
    SerialsPrint("printMode: ", P_SERIAL | P_SERIAL1 | have_sms);
    SerialsPrintln(printMode, P_SERIAL | P_SERIAL1 | have_sms);
    SerialsPrint("MP3: ", P_SERIAL | P_SERIAL1 | have_sms);
    SerialsPrintln(isMP3, P_SERIAL | P_SERIAL1 | have_sms);
    SerialsPrint("Playing: ", P_SERIAL | P_SERIAL1 | have_sms);
    SerialsPrintln(musicPlayer.playingMusic, P_SERIAL | P_SERIAL1 | have_sms);
    SerialsPrint("Lan Cuoi Nhan Tin Hieu Thoi Gian: ",
                 P_SERIAL | P_SERIAL1 | have_sms);
    SerialsPrintln(lastReceiveTimeSignal, P_SERIAL | P_SERIAL1 | have_sms);
    SerialsPrint("dien thoai phu: ", P_SERIAL | P_SERIAL1 | have_sms);
    if (phone_second.length() == 9)
      SerialsPrintln(phone_second, P_SERIAL | P_SERIAL1 | have_sms);
    else
      SerialsPrintln("Khong co", P_SERIAL | P_SERIAL1 | have_sms);
    break;

  case SIM:
    initGsm();
    break;
  case ATC:
    if (cmd.indexOf("%%") > -1) {
      String atCmd, acc, reject;
      uint32_t timeout;
      uint8_t retry;

      atCmd = cmd.substring(0, cmd.indexOf("%%"));
      cmd = cmd.substring(cmd.indexOf("%%") + 2);
      retry = cmd.substring(0, cmd.indexOf("%%")).toInt();
      if (!retry)
        retry = 5;
      cmd = cmd.substring(cmd.indexOf("%%") + 2);
      timeout = cmd.substring(0, cmd.indexOf("%%")).toInt();
      if (!timeout)
        timeout = 5000;
      cmd = cmd.substring(cmd.indexOf("%%") + 2);
      acc = cmd.substring(0, cmd.indexOf("%%"));
      if (!acc.length())
        acc = "OK";
      cmd = cmd.substring(cmd.indexOf("%%") + 2);
      reject = cmd.substring(0, cmd.indexOf("%%"));
      if (!reject.length())
        reject = "ERROR";
      sendATCommand(atCmd, retry, timeout, acc.c_str(), reject.c_str());

    } else
      sendATCommand(cmd, 5, 5000, "OK", "ERROR");
    SerialsPrintln("Ket thuc thuc hien lenh AT", P_SERIAL | P_SERIAL1);
    break;

  default:
    if (!erCode)
      SerialsPrintln("Lenh khong hop le.", P_SERIAL | P_SERIAL1 | have_sms);
    break;
  }
  if (erCode) {
    SerialsPrint("Thi hanh lenh gap loi ma so: ",
                 P_SERIAL | P_SERIAL1 | have_sms);
    SerialsPrintln(erCode, P_SERIAL | P_SERIAL1 | have_sms);
  }
  if (have_sms) {
    smsBuffering = false;
    sendBufferedSMS(master ? phone_master : phone_second);
  }
  have_sms = 0;
}

//------------------------------------------------------------------------------

void xulyhongngoai(int cmd) {
  uint8_t track, erCode;
  SerialsPrint("Lenh Hong Ngoai: ", P_SERIAL | P_SERIAL1);
  SerialsPrintln(cmd, P_SERIAL | P_SERIAL1);
  if (!isMP3) {
    SerialsPrintln("Khong The Nhan Hong Ngoai Khi Khong Co Module MP3",
                   P_SERIAL | P_SERIAL1);
    return;
  }
  if (mode && cmd != 64 && cmd != 68 && cmd != 69) {
    SerialsPrintln("Chi kha dung o mode Manual", P_SERIAL | P_SERIAL1);
    return;
  }

  track = 0;
  switch (cmd) {
  case 12: // Phim 1
    track = 1;
    break;
  case 24: // Phim 2
    track = 2;
    break;
  case 94: // Phim 3
    track = 3;
    break;
  case 8: // Phim 4
    track = 4;
    break;
  case 28: // Phim 5
    track = 5;
    break;
  case 90: // Phim 6
    track = 6;
    break;
  case 66: // Phim 7
    track = 7;
    break;
  case 82: // Phim 8
    track = 8;
    break;
  case 74: // Phim 9
    track = 9;
    break;
  case 13: // Phim 0
    track = 10;
    break;

  case 7:
  case 22: // Phim - Volume
  case 21:
  case 25: // Phim + Volume

    // Volume control using currentVolume tracking
    if (cmd == 7 || cmd == 22) {
      if (currentVolume < 252)
        currentVolume += 2; // quieter
    } else {
      if (currentVolume > 1)
        currentVolume -= 2; // louder
    }
    musicPlayer.setVolume(currentVolume, currentVolume);

    SerialsPrint(F("Volume changed to -"), P_SERIAL | P_SERIAL1);
    SerialsPrint(currentVolume >> 1, P_SERIAL | P_SERIAL1);
    SerialsPrintln(F("[dB]"), P_SERIAL | P_SERIAL1);

    break;

  case 69:
  case 68:
  case 64: // Mode - Power - Back
    mode = !mode;
    SerialsPrint("Chuyen qua che do ", P_SERIAL | P_SERIAL1);
    SerialsPrintln(mode ? "Auto" : "Manual", P_SERIAL | P_SERIAL1);
    lcd.setCursor(0, 1);
    lcd.print(mode ? "AUTO   " : "MANUAL0");
    break;
  case 9:
  case 71:
  case 70:
  case 67:
  case 0: // Stop - EQ - Play/Pause
    if (IrReceiver.decodedIRData.numberOfBits < 15)
      break;
    if (musicPlayer.playingMusic) {
      musicPlayer.stopPlaying();
      digitalWrite(RELAY, LOW);
      SerialsPrintln("Dung choi bai hat", P_SERIAL | P_SERIAL1);
      break;
    } else
      SerialsPrintln("Dang Khong Hat", P_SERIAL | P_SERIAL1);
  }
  if (track) {

    if (musicPlayer.playingMusic) {
      musicPlayer.stopPlaying();
      SerialsPrintln("Dung bai hat dang chay", P_SERIAL | P_SERIAL1);
    }

    digitalWrite(RELAY, HIGH);
    delay(2000);

    if (!playTrackByNumber(track))
      SerialsPrint("Khong co track ", P_SERIAL | P_SERIAL1);
    else
      SerialsPrint("Playing track ", P_SERIAL | P_SERIAL1);
    SerialsPrintln(track, P_SERIAL | P_SERIAL1);
  }
  digitalWrite(RELAY, musicPlayer.playingMusic);
}

bool gsmReadSerial(uint32_t timeout, const char *accept, const char *reject) {
  gsmBuffer = "";
  uint64_t timeOld = millis();
  bool result = 0;
  bool response_found = false;

  while ((millis() < timeOld + timeout)) {
    checkIR();
    checkButton();

    if (gsm.available()) {
      char buff = gsm.read();
      gsmBuffer += buff;

#if DEBUG
      if (printMode) {
        SerialsPrint("Chuoi nhan duoc tu sim800: ", P_SERIAL | P_SERIAL1);
        SerialsPrint(gsmBuffer, P_SERIAL | P_SERIAL1);
      }
#endif
      if (!response_found)
        timeOld = millis();

      if (gsmBuffer.indexOf("+CMTI:") > -1) {
        SerialsPrintln("Nhan duoc tin nhan moi", P_SERIAL | P_SERIAL1);
        have_sms = 1;
      }

      if (!response_found && (buff == '\n' || buff == '>') &&
          (gsmBuffer.indexOf(accept) > -1 || gsmBuffer.indexOf(reject) > -1)) {
#if DEBUG
        SerialsPrint("chi muc chap nhan: ", P_SERIAL | P_SERIAL1);
        SerialsPrintln(gsmBuffer.indexOf(accept), P_SERIAL | P_SERIAL1);
        SerialsPrint("chi muc tu choi: ", P_SERIAL | P_SERIAL1);
        SerialsPrintln(gsmBuffer.indexOf(reject), P_SERIAL | P_SERIAL1);
#endif
        timeout = 1000; // 1s wait for trailing data
        timeOld = millis();
        result = gsmBuffer.indexOf(accept) > -1;
        response_found = true; // Stop checking on future chars
      }
    }
  }
  return result;
}

bool sendATCommand(String cmd, uint8_t retry, uint32_t timeout, const char *acc,
                   const char *err) {

  return sendATCommand(cmd.c_str(), retry, timeout, acc, err);
}

bool sendATCommandNoPrint(String cmd, uint8_t retry, uint32_t timeout,
                          const char *acc, const char *err) {

  return sendATCommandNoPrint(cmd.c_str(), retry, timeout, acc, err);
}

bool sendATCommandNoPrint(const char *cmd, uint8_t retry, uint32_t timeout,
                          const char *acc, const char *err) {

  gsm.println(cmd);
  bool res = gsmReadSerial(timeout, acc, err);
  while (!res && retry && --retry) {
    gsm.println(cmd);
    res = gsmReadSerial(timeout, acc, err);
  }

  return res;
}

bool sendATCommand(const char *cmd, uint8_t retry, uint32_t timeout,
                   const char *acc, const char *err) {

  SerialsPrint("Gui lenh AT command: ", P_SERIAL | P_SERIAL1);
  SerialsPrintln(cmd, P_SERIAL | P_SERIAL1 | P_GSM);
  // gsm.println(cmd);
  bool res = gsmReadSerial(timeout, acc, err);
  while (!res && retry && --retry) {
    SerialsPrint("So luot thu lai con: ", P_SERIAL | P_SERIAL1);
    SerialsPrintln(retry, P_SERIAL | P_SERIAL1);
    SerialsPrintln("\nGui lai lenh AT", P_SERIAL | P_SERIAL1);
    //      if(cmd[3])gsm.println("A/");
    SerialsPrintln(cmd, P_SERIAL | P_SERIAL1 | P_GSM);
    // gsm.println(cmd);
    res = gsmReadSerial(timeout, acc, err);
  }
  while (gsm.available())
    gsmBuffer += gsm.readString();
  SerialsPrint("Thanh Cong: ", P_SERIAL | P_SERIAL1);
  SerialsPrintln(res ? "Co" : "Khong", P_SERIAL | P_SERIAL1);
  SerialsPrintln(gsmBuffer, P_SERIAL | P_SERIAL1);

  return res;
}

uint8_t gsmReset() {
  // Wait for module to boot up
  delay(3000);

  // Auto-baud detection: try common baud rates
  const long baudRates[] = {38400, 9600, 115200};
  const char *baudNames[] = {"38400", "9600", "115200"};
  bool connected = false;

  for (uint8_t i = 0; i < 3 && !connected; i++) {
    gsm.end();
    gsm.begin(baudRates[i]);
    delay(100);
    SerialsPrint("Trying baud: ", P_SERIAL | P_SERIAL1);
    SerialsPrintln(baudRates[i], P_SERIAL | P_SERIAL1);

    // Send AT and read raw response (don't use sendATCommand — baud might be
    // wrong)
    for (uint8_t attempt = 0; attempt < 3 && !connected; attempt++) {
      while (gsm.available())
        gsm.read(); // flush
      gsm.println("AT");
      gsmBuffer = "";
      uint32_t t = millis();
      while (millis() - t < 1000) {
        if (gsm.available())
          gsmBuffer += (char)gsm.read();
      }
      if (gsmBuffer.indexOf("OK") > -1) {
        connected = true;
        SerialsPrint("Module found at baud: ", P_SERIAL | P_SERIAL1);
        SerialsPrintln(baudNames[i], P_SERIAL | P_SERIAL1);
      }
    }
  }

  if (!connected)
    return CONN_ERROR;

  // Flush leftover data from baud detection and let module settle
  delay(500);
  while (gsm.available())
    gsm.read();

  // Verify communication works with sendATCommand
  if (!sendATCommand("AT", 3, 2000, "OK", "ERROR"))
    return CONN_ERROR;

  // Detect module type early so we can use correct reset command
  detectGsmModule();

  // Module-specific reset
  if (gsmModuleType == GSM_A7680) {
    // A7680C: use AT+CRESET or soft reset via CFUN
    sendATCommand("AT+CFUN=0", 2, 5000, "OK", "ERROR");
    delay(1000);
    sendATCommand("AT+CFUN=1", 2, 10000, "OK", "ERROR");
    sendATCommand("AT", 3, 5000, "OK", "ERROR");
  } else {
    // SIM800C: full restart
    sendATCommand("AT+CFUN=1,1", 2, 10000, "OK", "ERROR");
    sendATCommand("AT", 6, 5000, "OK", "ERROR");
  }

  // Check SIM card
  bool res = sendATCommand("AT+CPIN?", 4, 5000, "READY", "ERROR");
  if (gsmBuffer.length()) {
    if (!res && gsmBuffer.indexOf("ERR") > -1) {
      return SIM_ERROR;
    }
  } else
    return CONN_ERROR;
  return 0;
}

void initGsm() {
  lcd.setCursor(0, 0);
  lcd.print("                ");
  lcd.setCursor(0, 0);
  SerialsPrintln("\nDang khoi dong module GSM....\n", P_SERIAL | P_SERIAL1);
  uint8_t erCode = gsmReset();

  if (erCode) {
    SerialsPrint("Loi khoi dong module GSM: ", P_SERIAL | P_SERIAL1);
    SerialsPrintln(erCode, P_SERIAL | P_SERIAL1);
  } else {
    // Module already detected in gsmReset
    lcd.print(gsmModuleType == GSM_A7680 ? "A7680" : "SIM800");
    if (lastReceiveTimeSignal > 2) {
      lcd.print(".");
      SerialsPrintln("Thiet lap gio GPS...", P_SERIAL | P_SERIAL1);
      bool timeOk = false;
      if (gsmModuleType == GSM_A7680) {
        sendATCommand("AT+CTZU=1", 3, 2000, "OK", "ERROR");
        sendATCommand("AT+COPS=2", 3, 10000, "OK", "ERROR");
        delay(2000);
        timeOk = sendATCommand("AT+COPS=0", 3, 60000, "CGEV:", "ERROR");
        if (timeOk)
          delay(5000); // Wait for network time URC to arrive
      } else {
        timeOk = sendATCommand("AT+CLTS=1;+COPS=2;+COPS=0", 10, 120000,
                               "CIEV:", "ERROR");
      }
      if (timeOk) {
        SerialsPrintln("Nhan gio thanh cong\n\nBat dau doc gio tu module GSM.",
                       P_SERIAL | P_SERIAL1);
        lcd.print(".");
        sendATCommand("AT+CCLK?", 3, 200, "CCLK:", "ERROR");
        timeFromString(gsmBuffer);
        lastReceiveTimeSignal = 1;
        lcd.setCursor(8, 1);
        lcd.print(printTime(1, 1, 21, gio_check, phut_check, giay_check)
                      .substring(9));
      } else
        SerialsPrintln("Khong the lay gio GPS", P_SERIAL | P_SERIAL1);
    }
    SerialsPrintln("\nKiem Tra Tai Khoan...", P_SERIAL | P_SERIAL1);
    sendATCommand("AT+CUSD=1,\"*101#\",", 5, 5000, "CUSD:", "ERROR");
    gsmBuffer = gsmBuffer.substring(gsmBuffer.indexOf(", \"") + 3,
                                    gsmBuffer.lastIndexOf("\","));
    SerialsPrintln("Chuoi CUSD:", P_SERIAL | P_SERIAL1);
    SerialsPrintln(gsmBuffer, P_SERIAL | P_SERIAL1);

    SerialsPrintln("\nThiet lap mode tin nhan...", P_SERIAL | P_SERIAL1);
    bool smsSetupOk = false;
    if (gsmModuleType == GSM_A7680) {
      smsSetupOk = sendATCommand("AT+CMGF=1", 5, 5000, "OK", "ERROR") &&
                   sendATCommand("AT+CNMI=1,1,0,0,1", 5, 5000, "OK", "ERROR");
    } else {
      smsSetupOk =
          sendATCommand("AT+CMGF=1;+CNMI=1,1,0,0,1", 5, 5000, "OK", "ERROR");
    }
    if (smsSetupOk) {
      if (gsmModuleType == GSM_A7680)
        sendATCommand("AT+CMGD=1,4", 5, 5000, "OK", "ERROR");
      else
        sendATCommand("AT+CMGDA=\"DEL ALL\"", 5, 5000, "OK", "ERROR");
      SerialsPrintln("Thiet lap tin nhan thanh cong", P_SERIAL | P_SERIAL1);
      lcd.setCursor(8, 0);
      lcd.print(".");

    } else
      SerialsPrintln("Loi thiet lap tin nhan", P_SERIAL | P_SERIAL1);
  }
}

String printTime(uint8_t ngay, uint8_t thang, uint8_t nam, uint8_t gio,
                 uint8_t phut, uint8_t giay) {
  String dateTime = "";
  if (ngay < 10)
    dateTime = "0";
  dateTime += ngay;
  dateTime += "/";
  if (thang < 10)
    dateTime += "0";
  dateTime += thang;
  dateTime += "/";
  if (nam < 10)
    dateTime += "0";
  dateTime += nam;
  dateTime += ",";
  if (gio < 10)
    dateTime += "0";
  dateTime += gio;
  dateTime += ":";
  if (phut < 10)
    dateTime += "0";
  dateTime += phut;
  dateTime += ":";
  if (giay < 10)
    dateTime += "0";
  dateTime += giay;
  return dateTime;
}
