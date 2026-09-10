#include <SPI.h>
#include <MFRC522.h>
#include <Keypad.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <RTClib.h>
#include "USB.h"
#include "USBHIDKeyboard.h"
#include <HijelHID_BLEKeyboard.h>
#include <AESLib.h>

// Demo configuration is public. Keep personal settings in the ignored local header.
#if __has_include("credentials.local.h")
#include "credentials.local.h"
#else
#include "credentials.example.h"
#endif

// Pin definitions
#define SS_PIN    10
#define RST_PIN   9
#define BUZZER    3

// Auth Config
const int MAX_ATTEMPTS = 3;
const int LOCKOUT_SECONDS = 30;

// Profile Config
const int NUM_PROFILES = sizeof(profiles) / sizeof(profiles[0]);

// Keypad Setup
const byte ROWS = 4;
const byte COLS = 4;
char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};
byte rowPins[ROWS] = {A1, A0, 8, 7};
byte colPins[COLS] = {6, 5, 4, 2};
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// Component Objects
MFRC522 rfid(SS_PIN, RST_PIN);
LiquidCrystal_I2C lcd(0x27, 20, 4);
RTC_DS1307 rtc;
USBHIDKeyboard usbKeyboard;
HijelHID_BLEKeyboard bleKeyboard("MFA Security Key", "Arduino", 100);
AESLib aesLib;

// State Variables
int failedAttempts = 0;
bool lockedOut = false;
DateTime lockoutTime;
String validUID = "";
String validPIN = "";
bool bluetoothMode = false;
unsigned long aKeyHoldStart = 0;
bool aKeyHeld = false;

// AES Decrypt
String decryptCredential(byte* encrypted, int len) {
  byte decrypted[len];
  byte ivCopy[16];
  memcpy(ivCopy, aesIV, 16);
  aesLib.decrypt(encrypted, len, decrypted, aesKey, 128, ivCopy);
  decrypted[len - 1] = '\0';
  return String((char*)decrypted);
}

// Type via active mode
void typeText(const char* text) {
  if (bluetoothMode) {
    for (int i = 0; i < strlen(text); i++) {
      bleKeyboard.write(text[i]);
      delay(50);
    }
  } else {
    usbKeyboard.print(text);
  }
}

void typeTab() {
  if (bluetoothMode) {
    bleKeyboard.write('\t');  // write() accepts ASCII, not raw HID keycodes
    delay(100);
  } else {
    usbKeyboard.write('\t');
  }
}

void typeEnter() {
  if (bluetoothMode) {
    bleKeyboard.write('\n');
    delay(100);
  } else {
    usbKeyboard.write('\n');
  }
}

// Helper Functions
void beepSuccess() {
  digitalWrite(BUZZER, HIGH);
  delay(500);
  digitalWrite(BUZZER, LOW);
}

void beepFail() {
  for (int i = 0; i < 3; i++) {
    digitalWrite(BUZZER, HIGH);
    delay(150);
    digitalWrite(BUZZER, LOW);
    delay(150);
  }
}

void beepShort() {
  digitalWrite(BUZZER, HIGH);
  delay(100);
  digitalWrite(BUZZER, LOW);
}

void logEvent(String event) {
  DateTime now = rtc.now();
  Serial.print("[");
  Serial.print(now.year());
  Serial.print("/");
  Serial.print(now.month());
  Serial.print("/");
  Serial.print(now.day());
  Serial.print(" ");
  Serial.print(now.hour());
  Serial.print(":");
  Serial.print(now.minute());
  Serial.print(":");
  Serial.print(now.second());
  Serial.print("] ");
  Serial.println(event);
}

void showIdle() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("  MFA Security Key  ");
  lcd.setCursor(0, 1);
  lcd.print("--------------------");
  lcd.setCursor(0, 2);
  lcd.print("  Scan your card... ");
  lcd.setCursor(0, 3);
  if (bluetoothMode) {
    if (bleKeyboard.isConnected()) {
      lcd.print(" BT: Connected      ");
    } else {
      lcd.print(" BT: Searching...   ");
    }
  } else {
    lcd.print(" USB Mode    [A]Toggle");
  }
}

void showModeToggle() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("--------------------");
  lcd.setCursor(0, 1);
  if (bluetoothMode) {
    lcd.print("  Mode: BLUETOOTH   ");
  } else {
    lcd.print("  Mode: USB         ");
  }
  lcd.setCursor(0, 2);
  lcd.print("  Hold [A] to toggle");
  lcd.setCursor(0, 3);
  lcd.print("--------------------");
  beepShort();
  delay(1500);
  showIdle();
}

String getCardUID() {
  String uid = "";
  for (byte i = 0; i < rfid.uid.size; i++) {
    if (rfid.uid.uidByte[i] < 0x10) uid += "0";
    uid += String(rfid.uid.uidByte[i], HEX);
    if (i < rfid.uid.size - 1) uid += " ";
  }
  uid.toUpperCase();
  return uid;
}

String getPIN() {
  String pin = "";
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("  Enter your PIN:   ");
  lcd.setCursor(0, 1);
  lcd.print("--------------------");
  lcd.setCursor(0, 2);
  lcd.print("  ");
  lcd.setCursor(0, 3);
  lcd.print("  [#]OK  [*]Delete  ");

  while (true) {
    char key = keypad.getKey();
    if (key) {
      if (key == '#') {
        return pin;
      } else if (key == '*') {
        if (pin.length() > 0) {
          pin.remove(pin.length() - 1);
          lcd.setCursor(2, 2);
          lcd.print("                ");
          lcd.setCursor(2, 2);
          for (int i = 0; i < (int)pin.length(); i++) lcd.print("*");
        }
      } else if (pin.length() < 6) {
        pin += key;
        lcd.setCursor(2 + pin.length() - 1, 2);
        lcd.print("*");
        beepShort();
      }
    }
  }
}

int selectProfile() {
  int current = 0;
  while (true) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("--------------------");
    lcd.setCursor(0, 1);
    String line = " <[4]  ";
    line += profiles[current].name;
    line += "  [6]> ";
    lcd.print(line);
    lcd.setCursor(0, 2);
    String uname = String(profiles[current].username);
    if (uname.length() > 20) uname = uname.substring(0, 17) + "...";
    lcd.print(uname);
    lcd.setCursor(0, 3);
    lcd.print(" [#]Select [*]Back  ");

    while (true) {
      char key = keypad.getKey();
      if (key) {
        if (key == '6') {
          current = (current + 1) % NUM_PROFILES;
          beepShort();
          break;
        } else if (key == '4') {
          current = (current - 1 + NUM_PROFILES) % NUM_PROFILES;
          beepShort();
          break;
        } else if (key == '#') {
          return current;
        } else if (key == '*') {
          return -1;
        }
      }
    }
  }
}

void typeCredentials(int profileIndex) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("--------------------");
  lcd.setCursor(0, 1);
  lcd.print("   ACCESS GRANTED   ");
  lcd.setCursor(0, 2);
  lcd.print("  Logging in to:    ");
  lcd.setCursor(0, 3);
  lcd.print("  ");
  lcd.print(profiles[profileIndex].name);
  beepSuccess();
  logEvent(String("ACCESS GRANTED - Profile: ") + profiles[profileIndex].name);
  delay(1000);

  typeText(profiles[profileIndex].username);
  delay(300);
  typeTab();
  delay(300);
  typeText(profiles[profileIndex].password);
  delay(300);
  typeEnter();

  delay(2000);
  showIdle();
}

void accessDenied(String reason) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("--------------------");
  lcd.setCursor(0, 1);
  lcd.print("   ACCESS DENIED    ");
  lcd.setCursor(0, 2);
  lcd.print(reason);
  lcd.setCursor(0, 3);
  lcd.print("--------------------");
  beepFail();
  logEvent("ACCESS DENIED - " + reason);
  delay(2000);
  showIdle();
}

void checkLockout() {
  if (!lockedOut) return;
  DateTime now = rtc.now();
  long elapsed = now.unixtime() - lockoutTime.unixtime();
  if (elapsed >= LOCKOUT_SECONDS) {
    lockedOut = false;
    failedAttempts = 0;
    logEvent("LOCKOUT EXPIRED");
  } else {
    long remaining = LOCKOUT_SECONDS - elapsed;
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("--------------------");
    lcd.setCursor(0, 1);
    lcd.print("  DEVICE LOCKED     ");
    lcd.setCursor(0, 2);
    lcd.print("  Wait: ");
    lcd.print(remaining);
    lcd.print("s      ");
    lcd.setCursor(0, 3);
    lcd.print("--------------------");
    delay(1000);
  }
}

void checkModeToggle() {
  if (keypad.getKeys()) {
    for (int i = 0; i < 10; i++) {
      if (keypad.key[i].kchar == 'A') {
        if (keypad.key[i].kstate == PRESSED) {
          aKeyHoldStart = millis();
          aKeyHeld = true;
        }
        if (keypad.key[i].kstate == RELEASED) {
          aKeyHeld = false;
        }
      }
    }
  }

  if (aKeyHeld && (millis() - aKeyHoldStart >= 2000)) {
    aKeyHeld = false;
    bluetoothMode = !bluetoothMode;
    logEvent(bluetoothMode ? "MODE: Bluetooth" : "MODE: USB");
    showModeToggle();
  }
}
// Setup
void setup() {
  Serial.begin(9600);
  pinMode(BUZZER, OUTPUT);

  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("  MFA Security Key  ");
  lcd.setCursor(0, 1);
  lcd.print("  Initializing...   ");
  delay(1500);

  validUID = decryptCredential(encryptedUID, 16);
  validPIN = decryptCredential(encryptedPIN, 16);

  SPI.begin();
  rfid.PCD_Init();
  lcd.clear();
  lcd.setCursor(0, 1);
  lcd.print("  RFID:    OK       ");
  delay(600);

  lcd.setCursor(0, 2);
  lcd.print("  KEYPAD:  OK       ");
  delay(600);

  rtc.begin();
  if (!rtc.isrunning()) {
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }
  lcd.setCursor(0, 3);
  lcd.print("  RTC:     OK       ");
  delay(600);

  usbKeyboard.begin();
  USB.begin();
  bleKeyboard.begin();
  lcd.clear();
  lcd.setCursor(0, 1);
  lcd.print("  USB HID: OK       ");
  lcd.setCursor(0, 2);
  lcd.print("  BLE HID: OK       ");
  delay(600);

  lcd.clear();
  lcd.setCursor(0, 1);
  lcd.print("  System Ready!     ");
  beepSuccess();
  delay(1500);

  showIdle();
  logEvent("SYSTEM BOOT");
}

// Main Loop
void loop() {
  // Check mode toggle
  checkModeToggle();

  if (lockedOut) {
    checkLockout();
    return;
  }

  if (!rfid.PICC_IsNewCardPresent()) return;
  if (!rfid.PICC_ReadCardSerial()) return;

  String uid = getCardUID();
  logEvent("CARD SCANNED - UID: " + uid);

  if (uid != validUID) {
    accessDenied("  Invalid Card      ");
    rfid.PICC_HaltA();
    return;
  }

  lcd.clear();
  lcd.setCursor(0, 1);
  lcd.print("  Card accepted!    ");
  beepShort();
  delay(800);

  String pin = getPIN();

  if (pin != validPIN) {
    failedAttempts++;
    if (failedAttempts >= MAX_ATTEMPTS) {
      lockedOut = true;
      lockoutTime = rtc.now();
      logEvent("LOCKOUT TRIGGERED");
      accessDenied("  Too many attempts  ");
    } else {
      String msg = "  Wrong PIN (";
      msg += failedAttempts;
      msg += "/3)     ";
      accessDenied(msg);
    }
    rfid.PICC_HaltA();
    return;
  }

  int profile = selectProfile();

  if (profile == -1) {
    lcd.clear();
    lcd.setCursor(0, 1);
    lcd.print("   Cancelled.       ");
    delay(1000);
    showIdle();
  } else {
    failedAttempts = 0;
    typeCredentials(profile);
  }

  rfid.PICC_HaltA();
}
