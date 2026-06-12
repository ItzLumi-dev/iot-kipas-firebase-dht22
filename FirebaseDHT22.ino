#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>
#include <WiFi.h>
#include <FirebaseESP32.h> 

// ==========================================
// 1. KONFIGURASI WIFI & FIREBASE
// ==========================================
#define WIFI_SSID "PONDOX BENGKEL"               
#define WIFI_PASSWORD "kasihkeraskaka"       
#define FIREBASE_HOST "https://suhudht22-71151-default-rtdb.asia-southeast1.firebasedatabase.app/" 
#define FIREBASE_AUTH "OLIHlRDMMduVcDeb2Rir5hVOvz801Jk2hy8KXkC9"     

// Tambahan objek untuk library Firebase versi baru
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// ==========================================
// 2. KONFIGURASI PIN & SENSOR
// ==========================================
#define DHTPIN 23
#define DHTTYPE DHT22
#define RELAY_PIN 19
const float BATAS_SUHU = 30.0;

DHT dht(DHTPIN, DHTTYPE);

// ==========================================
// 3. INISIALISASI OLED
// ==========================================
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// ==========================================
// 4. VARIABEL KONTROL SYSTEM
// ==========================================
String modeSistem = "Otomatis"; 
String statusKipas = "OFF";
String perintahManual = "OFF";

unsigned long waktuLama = 0;
const long interval = 2000; 

void setup() {
  Serial.begin(115200);
  
  // Setup Pin Relay
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);
  
  // Setup DHT
  dht.begin();

  // Inisialisasi OLED
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("OLED Gagal diinisialisasi"));
    for(;;);
  }
  
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(10, 25);
  display.setTextSize(1);
  display.println("CONNECTING WIFI...");
  display.display();

  // Mulai Koneksi WiFi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Connected!");

  // ---------------------------------------------------------
  // Mulai Koneksi Firebase (UPDATE UNTUK VERSI BARU)
  // ---------------------------------------------------------
  config.database_url = FIREBASE_HOST;
  config.signer.tokens.legacy_token = FIREBASE_AUTH;
  
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  // Set nilai default di Firebase saat pertama kali nyala
  Firebase.setString(fbdo, "/kontrol/mode", modeSistem);
  Firebase.setString(fbdo, "/kontrol/kipas", perintahManual);

  Serial.println("=== SISTEM IOT READY ===");
}

void loop() {
  unsigned long waktuSekarang = millis();
  if (waktuSekarang - waktuLama >= interval) {
    waktuLama = waktuSekarang;

    float suhu = dht.readTemperature();
    float lembap = dht.readHumidity();

    if (isnan(suhu) || isnan(lembap)) {
      Serial.println("Error: Sensor DHT22 tidak terbaca!");
      return;
    }
    
    // 1. Baca Mode Sistem dari Firebase
    if (Firebase.getString(fbdo, "/kontrol/mode")) {
      modeSistem = fbdo.stringData();
    }
    
    // 2. Baca Saklar Kipas dari Firebase (Hanya berlaku jika mode Manual)
    if (modeSistem == "Manual") {
      if (Firebase.getString(fbdo, "/kontrol/kipas")) {
        perintahManual = fbdo.stringData();
      }
    }

    // LOGIKA DUAL MODE UNTUK RELAY/KIPAS
    if (modeSistem == "Manual") {
      if (perintahManual == "ON") {
        digitalWrite(RELAY_PIN, HIGH);
        statusKipas = "ON";
      } else {
        digitalWrite(RELAY_PIN, LOW);
        statusKipas = "OFF";
      }
    } 
    else { 
      if (suhu > BATAS_SUHU) {
        digitalWrite(RELAY_PIN, HIGH);
        statusKipas = "ON";
        Firebase.setString(fbdo, "/kontrol/kipas", "ON"); 
      } else {
        digitalWrite(RELAY_PIN, LOW);
        statusKipas = "OFF";
        Firebase.setString(fbdo, "/kontrol/kipas", "OFF");
      }
    }

    // KIRIM DATA SENSOR KE FIREBASE
    Firebase.setFloat(fbdo, "/sensor/suhu", suhu);
    Firebase.setFloat(fbdo, "/sensor/lembap", lembap);

    // PRINT LOG KE SERIAL MONITOR
    Serial.printf("Suhu: %.1f C | Lembap: %.0f %% | Mode: %s | Kipas: %s\n", 
                  suhu, lembap, modeSistem.c_str(), statusKipas.c_str());

    // UPDATE TAMPILAN OLED
    display.clearDisplay();
    display.setTextSize(1);
    
    display.setCursor(0, 0);
    display.print("MODE : "); 
    display.println(modeSistem);
    display.println("---------------------");

    display.printf("Suhu : %.1f C\n", suhu);
    display.printf("Lembap: %.0f %%\n", lembap);
    display.println("---------------------");

    display.print("KIPAS: ");
    if (statusKipas == "ON") {
      display.setTextColor(SSD1306_BLACK, SSD1306_WHITE); 
    }
    display.print(statusKipas);
    display.setTextColor(SSD1306_WHITE); 
    
    display.display();
  }
}