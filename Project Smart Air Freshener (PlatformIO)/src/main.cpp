#define BLYNK_TEMPLATE_ID "TMPL6yypVJXVs"
#define BLYNK_TEMPLATE_NAME "SmartSpray"
#define BLYNK_AUTH_TOKEN "D6b_dLWhhMktuDoAl9Vm5tA2NZZN1BGZ"
#define BLYNK_PRINT Serial

#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>
#include <TimeLib.h>
#include <WidgetRTC.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <EEPROM.h>
#include <LiquidCrystal_I2C.h>
LiquidCrystal_I2C lcd(0x27, 16, 2);

String botToken = "7877132934:AAGYGir2xtz7ACkkZg6jh_ZS40DfviuEjkA"; // Token dari BotFather
String chatID = "8070797176";

char auth[] = "D6b_dLWhhMktuDoAl9Vm5tA2NZZN1BGZ";
char ssid[] = "Fnetwork";
char pass[] = "passwordnyalupa";

// Pin Konfigurasi
#define EEPROM_SIZE 32
#define MOTOR_FORWARD_PIN 23
#define MOTOR_REVERSE_PIN 19
#define BUTTON_PIN 4
#define BUZZER_PIN 5
#define WIFI_LED_PIN 27
#define PIR_PIN 14
#define SPRAY_DURATION 300 // ms

#define VPIN_SPRAY_BUTTON V0
#define VPIN_SPRAY_STATUS V1
#define VPIN_TIMER1 V2
#define VPIN_TIMER2 V3
#define VPIN_TIMER3 V4
#define VPIN_PIR_MODE V5

BlynkTimer timer;
WidgetRTC rtc;

unsigned long lcdTimeout = 0;
bool lcdNeedsReset = false;
bool countdownDisplayed[3] = {false, false, false};

bool pirModeEnabled = false;
bool motionDetected = false;
unsigned long lastMotionTime = 0;

bool lastButtonState = HIGH;
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 300; // ms
volatile bool buttonPressed = false;
unsigned long lastButtonPress = 0;

bool isSpraying = false;

bool triggeredByButton = false;
bool triggeredByTimer = false;

int scheduleHour[3] = {-1, -1, -1};
int scheduleMinute[3] = {-1, -1, -1};
bool alreadySprayed[3] = {false, false, false};

bool isOnline = false;

void updateLCD(String line1, String line2, bool autoReset = false)
{
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(line1);
  lcd.setCursor(0, 1);
  lcd.print(line2);

  if (autoReset)
  {
    lcdTimeout = millis();
    lcdNeedsReset = true;
  }
}

void tampilkanDefaultLCD()
{
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("SmartSpray Ready");

  // Misalnya tampilkan mode di baris ke-2:
  lcd.setCursor(0, 1);
  if (!isOnline)
    lcd.print("Mode: Offline");
  else if (pirModeEnabled)
    lcd.print("Mode: PIR Aktif");
  else
    lcd.print("Mode: Manual");
}

void sendTelegramMessage(String message)
{
  if (!isOnline)
    return;
  if (WiFi.status() == WL_CONNECTED)
  {
    HTTPClient http;
    String url = "https://api.telegram.org/bot" + botToken + "/sendMessage?chat_id=" + chatID + "&text=" + message;

    http.begin(url);
    int httpCode = http.GET();
    http.end();
    Serial.println(httpCode > 0 ? "Pesan Telegram terkirim." : "Gagal kirim Telegram.");
  }
}

void tampilkanCountdownLCD(int index)
{
  char msg[17];
  sprintf(msg, "Akan Semprot %02d:%02d", scheduleHour[index], scheduleMinute[index]);
  updateLCD("SmartSpray", msg);
}

void buzzerBeepTwice()
{
  for (int i = 0; i < 2; i++)
  {
    digitalWrite(BUZZER_PIN, HIGH); // bunyi
    delay(200);
    digitalWrite(BUZZER_PIN, LOW); // Bunyi OFF
    delay(200);                    // Jeda antar beep (200ms)
  }
}

void simpanJadwalKeEEPROM()
{
  for (int i = 0; i < 3; i++)
  {
    EEPROM.write(i * 2, scheduleHour[i]);
    EEPROM.write(i * 2 + 1, scheduleMinute[i]);
  }
  EEPROM.commit();
  Serial.println("✅ Jadwal disimpan ke EEPROM");
}

void muatJadwalDariEEPROM()
{
  for (int i = 0; i < 3; i++)
  {
    scheduleHour[i] = EEPROM.read(i * 2);
    scheduleMinute[i] = EEPROM.read(i * 2 + 1);

    // Validasi
    if (scheduleHour[i] > 23 || scheduleMinute[i] > 59)
    {
      scheduleHour[i] = -1;
      scheduleMinute[i] = -1;
    }
  }
  Serial.println("📥 Jadwal dimuat dari EEPROM");
}

void spray()
{
  if (isSpraying)
    return;

  isSpraying = true;

  if (triggeredByButton)
  {
    if (isOnline)
      sendTelegramMessage("Penyemprotan dilakukan secara MANUAL melalui tombol.");
    updateLCD("Manual Button", "Spraying...");
    triggeredByButton = false;
  }

  else if (triggeredByTimer)
  {
    if (isOnline)
      sendTelegramMessage("Penyemprotan dilakukan sesuai JADWAL TIMER.");
    updateLCD("Timer Spray", "Spraying...");
    triggeredByTimer = false;
  }
  else if (pirModeEnabled)
  {
    updateLCD("PIR Sensor", "Spraying...");
  }

  else
  {
    updateLCD("Blynk Manual", "Spraying...");
  }

  buzzerBeepTwice();

  Serial.println("Melakukan Spraying...");
  if (isOnline)
  {
    Blynk.virtualWrite(VPIN_SPRAY_STATUS, "Spraying...");
    Blynk.logEvent("spray_event", "Penyemprotan dilakukan!");
  }

  // digitalWrite(LED_PIN, HIGH);

  digitalWrite(MOTOR_FORWARD_PIN, HIGH);

  delay(SPRAY_DURATION);

  digitalWrite(MOTOR_FORWARD_PIN, LOW);
  digitalWrite(BUZZER_PIN, LOW);
  delay(100);

  Serial.println("Spray done.");
  if (isOnline)
    Blynk.virtualWrite(VPIN_SPRAY_STATUS, "Selesai Menyemprot");
  delay(1500);
  tampilkanDefaultLCD();
  isSpraying = false;

  delay(1500); // tampilkan status selama 1,5 detik

  tampilkanDefaultLCD();
  isSpraying = false;
}

void checkButton()
{
  static bool lastState = HIGH;
  bool currentState = digitalRead(BUTTON_PIN);

  if (lastButtonState == HIGH && currentState == LOW)
  {
    unsigned long currentTime = millis();
    if (currentTime - lastDebounceTime > debounceDelay)
    {
      Serial.println("Tombol fisik ditekan!");
      triggeredByButton = true;
      spray();
      lastDebounceTime = currentTime;
    }
  }

  lastButtonState = currentState;
}

void IRAM_ATTR handleButtonInterrupt()
{
  static unsigned long lastInterruptTime = 0;
  unsigned long currentTime = millis();

  // Debounce: abaikan jika <300ms dari tombol sebelumnya
  if (currentTime - lastInterruptTime > debounceDelay)
  {
    buttonPressed = true;
    lastButtonPress = currentTime;
    lastInterruptTime = currentTime;
  }
}

BLYNK_WRITE(VPIN_SPRAY_BUTTON)
{
  if (!isOnline)
    return;
  int value = param.asInt();
  if (value == 1)
  {
    if (!pirModeEnabled)
    {
      spray();
    }
    else
    {
      Serial.println("PIR mode aktif - penyemprotan manual Blynk diabaikan");
    }
    Blynk.virtualWrite(VPIN_SPRAY_BUTTON, 0);
  }
}

BLYNK_WRITE(VPIN_TIMER1)
{
  if (!isOnline)
    return;
  TimeInputParam t(param);
  if (t.hasStartTime())
  {
    scheduleHour[0] = t.getStartHour();
    scheduleMinute[0] = t.getStartMinute();
    alreadySprayed[0] = false;
    simpanJadwalKeEEPROM();
    Serial.printf("Timer 1 diset: %02d:%02d\n", scheduleHour[0], scheduleMinute[0]);

    char schedule[17];
    sprintf(schedule, "Timer: %02d:%02d", scheduleHour[0], scheduleMinute[0]);
    updateLCD(schedule, "", true);
  }
}

BLYNK_WRITE(VPIN_TIMER2)
{
  if (!isOnline)
    return;
  TimeInputParam t(param);
  if (t.hasStartTime())
  {
    scheduleHour[1] = t.getStartHour();
    scheduleMinute[1] = t.getStartMinute();
    alreadySprayed[1] = false;
    simpanJadwalKeEEPROM();
    Serial.printf("Timer 2 diset: %02d:%02d\n", scheduleHour[1], scheduleMinute[1]);

    char schedule[17];
    sprintf(schedule, "Timer: %02d:%02d", scheduleHour[1], scheduleMinute[1]);
    updateLCD(schedule, "", true);
  }
}

BLYNK_WRITE(VPIN_TIMER3)
{
  if (!isOnline)
    return;
  TimeInputParam t(param);
  if (t.hasStartTime())
  {
    scheduleHour[2] = t.getStartHour();
    scheduleMinute[2] = t.getStartMinute();
    alreadySprayed[2] = false;
    simpanJadwalKeEEPROM();
    Serial.printf("Timer 3 diset: %02d:%02d\n", scheduleHour[2], scheduleMinute[2]);

    char schedule[17];
    sprintf(schedule, "Timer: %02d:%02d", scheduleHour[2], scheduleMinute[2]);
    updateLCD(schedule, "", true);
  }
}

BLYNK_WRITE(VPIN_PIR_MODE)
{
  if (isOnline)
    pirModeEnabled = param.asInt();
  if (pirModeEnabled)
    Serial.println("Mode PIR AKTIF — hanya semprot jika sensor aktif");
  else
    Serial.println("Mode PIR NONAKTIF — semprot bisa manual dan timer");
}

void checkSchedules()
{
  int nowHour = hour();
  int nowMinute = minute();
  int nowSecond = second();
  Serial.printf("RTC sekarang: %02d:%02d (%d)\n", nowHour, nowMinute, year());

  for (int i = 0; i < 3; i++)
  {
    if (scheduleHour[i] == -1)
      continue;

    // Jika 30 detik sebelum waktu semprot
    if (nowHour == scheduleHour[i] && nowMinute == scheduleMinute[i] && nowSecond >= 30 && !countdownDisplayed[i])
      Serial.printf("▶️ Countdown LCD Tampil: Jadwal %d %02d:%02d\n", i, nowHour, nowMinute);

    {
      tampilkanCountdownLCD(i); // Tampilkan ke LCD
      lcdTimeout = millis();    // Mulai timer 5 detik
      lcdNeedsReset = true;
      countdownDisplayed[i] = true;
      char msg[17];
      sprintf(msg, "Timer: %02d:%02d", nowHour, nowMinute, nowSecond);
      Serial.println(msg);
    }

    if (nowHour == scheduleHour[i] && nowMinute == scheduleMinute[i] && nowSecond == 0)
    {
      if (!alreadySprayed[i])
      {
        Serial.printf("JADWAL %d COCOK — menyemprot!\n", i);
        triggeredByTimer = true;

        if (!pirModeEnabled)
        {
          spray();
        }
        else
        {
          Serial.println("PIR mode aktif - penyemprotan via timer diabaikan");
        }
        alreadySprayed[i] = true;
        countdownDisplayed[i] = false; // Reset setelah menyemprot
      }
    }

    if (nowMinute != scheduleMinute[i] || nowHour != scheduleHour[i])
    {
      countdownDisplayed[i] = false;
      alreadySprayed[i] = false;
    }
  }

  Serial.printf("Sekarang %02d:%02d\n", nowHour, nowMinute, nowSecond);
}

void setup()
{
  Serial.begin(115200);
  Serial.println("Scanning I2C devices...");

  EEPROM.begin(EEPROM_SIZE);
  muatJadwalDariEEPROM();

  for (byte address = 1; address < 127; address++)
  {
    Wire.beginTransmission(address);
    if (Wire.endTransmission() == 0)
    {
      Serial.print("I2C device found at address 0x");
      Serial.println(address, HEX);
    }
  }
  Serial.println("Scan done.");

  lcd.init();      // Inisialisasi LCD
  delay(100);      // Tambahan delay agar LCD siap
  lcd.backlight(); // Nyalakan backlight
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("SmartSpray Ready");
  lcd.setCursor(0, 1);
  lcd.print("Menunggu input...");
  Serial.println("ESP32 Ready. Tekan tombol untuk menyemprot.");

  Wire.begin();

  pinMode(WIFI_LED_PIN, OUTPUT);
  digitalWrite(WIFI_LED_PIN, LOW);

  pinMode(PIR_PIN, INPUT);

  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  pinMode(MOTOR_FORWARD_PIN, OUTPUT);
  pinMode(MOTOR_REVERSE_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  //  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), handleButtonInterrupt, FALLING);

  digitalWrite(MOTOR_FORWARD_PIN, LOW);
  digitalWrite(MOTOR_REVERSE_PIN, LOW);

  WiFi.begin(ssid, pass);
  Serial.println("Menyambungkan WiFi...");
  updateLCD("WiFi", "Menyambung...");

  const unsigned long wifiTimeout = 15000;
  unsigned long startAttempt = millis();

  while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < wifiTimeout)
  {
    delay(200);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.println("\n✅ WiFi Tersambung");
    digitalWrite(WIFI_LED_PIN, HIGH);
    isOnline = true;
    updateLCD("WiFi", "Tersambung");
    delay(5000);
    tampilkanDefaultLCD();
  }
  else
  {
    Serial.println("\n❌ Gagal WiFi");
    isOnline = false;
    digitalWrite(WIFI_LED_PIN, LOW);
    updateLCD("WiFi", "Gagal");
  }

  if (isOnline)
  {
    Blynk.config(auth, "blynk.cloud", 80);
    Blynk.connect(15000); // timeout 3 detik
    if (Blynk.connected())
    {
      Serial.println("✅ Blynk Terhubung");
      sendTelegramMessage("ESP32 Terhubung!");
    }
    else
    {
      Serial.println("❌ Gagal konek Blynk");
      isOnline = false;
    }
  }

  // timer.setInterval(100, checkButton);
  rtc.begin();
  time_t t = now();
  Serial.printf("Waktu awal RTC: %02d:%02d:%02d %02d/%02d/%04d\n", hour(t), minute(t), second(t), day(t), month(t), year(t));
  setSyncInterval(60); // update waktu tiap 60 detik

  if (year() < 2025)
  {
    Serial.println("RTC belum tersinkronisasi!");
    return;
  }
  timer.setInterval(250, checkSchedules);
  Serial.println("✅ Sistem siap. Gunakan tombol fisik atau aplikasi.");

  char scheduleMsg[17];
  sprintf(scheduleMsg, "Timer: %02d:%02d", scheduleHour[0], scheduleMinute[0]);
  updateLCD("SmartSpray", scheduleMsg);

  delay(1500);           // tampilkan info jadwal sejenak
  tampilkanDefaultLCD(); // kembali ke tampilan default
}

void loop()
{
  checkButton();

  if (buttonPressed)
  {
    buttonPressed = false; // Reset
    Serial.println("Tombol ditekan!");

    if (!pirModeEnabled && !isSpraying)
    {
      triggeredByButton = true;
      spray();
    }
  }

  if (isOnline && pirModeEnabled && digitalRead(PIR_PIN) == HIGH && !isSpraying)
  {
    unsigned long now = millis();
    if (now - lastMotionTime > 10000) // Hindari penyemprotan berulang tiap gerakan
    {
      Serial.println("Gerakan terdeteksi oleh PIR!");
      triggeredByButton = false;
      triggeredByTimer = false;
      spray();
      sendTelegramMessage("Penyemprotan dilakukan karena GERAKAN terdeteksi oleh sensor PIR.");
      lastMotionTime = now;
    }
  }

  if (lcdNeedsReset && millis() - lcdTimeout >= 5000)
  {
    tampilkanDefaultLCD();
    lcdNeedsReset = false;
    Serial.println("🔁 LCD kembali ke tampilan default.");
  }

  if (isOnline)
  {
    Blynk.run();
  }
  timer.run();

  static int lastCheckedMinute = -1;
  time_t t = now();
  int currentHour = hour(t);
  int currentMinute = minute(t);

  if (currentMinute != lastCheckedMinute)
  {
    for (int i = 0; i < 3; i++)
    {
      if (scheduleHour[i] == currentHour && scheduleMinute[i] == currentMinute)
      {
        Serial.printf("Timer %d cocok! Menyemprot sekarang!\n", i + 1);
        triggeredByTimer = true;
        spray();
      }
    }
    lastCheckedMinute = currentMinute;
  }

  if (WiFi.status() == WL_CONNECTED)
  {
    digitalWrite(WIFI_LED_PIN, HIGH);
  }
  else
  {
    digitalWrite(WIFI_LED_PIN, LOW);
  }
}

// end