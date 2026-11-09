#include <M5Unified.h>
#include <IRsend.h>
#include <Preferences.h>
#include <WiFi.h>
#include <math.h>
#include <NimBLEDevice.h>
#include <NimBLEHIDDevice.h>

#define SCREEN_ROTATION 3
#define CPU_MHZ 80
#define BRIGHT_NORMAL 120
#define BRIGHT_DIM 15
#define DIM_AFTER_MS 30000

#define C565(r, g, b) ((uint16_t)((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3)))
#define COL_ON       C565(0, 120, 20)
#define COL_OFF      C565(200, 30, 30)
#define COL_BATT_OK  C565(0, 140, 20)
#define COL_BATT_MID C565(230, 150, 0)
#define COL_BATT_LOW C565(210, 30, 30)

struct Theme {
  const char* name;
  uint16_t bg, bg2, hdr, hdrTxt, sel, selEdge, txt, selTxt, dk, info;
};

static const Theme themes[] = {
  { "YESIL", C565(150,220,60), C565(166,232,84), C565(255,225,30), C565(40,90,10), C565(255,238,70), C565(120,90,0), C565(25,85,10), C565(70,45,0), C565(60,130,20), C565(90,60,0) },
  { "MAVI", C565(110,170,235), C565(128,185,245), C565(30,90,200), C565(255,255,255), C565(200,230,255), C565(10,50,120), C565(10,40,100), C565(0,30,80), C565(20,60,150), C565(0,60,120) },
  { "PEMBE", C565(250,170,200), C565(255,190,215), C565(220,50,120), C565(255,255,255), C565(255,225,240), C565(150,20,80), C565(120,10,60), C565(100,0,50), C565(180,30,100), C565(130,20,70) },
  { "TURUNCU", C565(255,180,80), C565(255,195,105), C565(220,100,0), C565(255,255,255), C565(255,230,160), C565(130,60,0), C565(110,50,0), C565(90,40,0), C565(180,80,0), C565(120,60,0) },
  { "MOR", C565(180,150,230), C565(195,168,240), C565(100,50,180), C565(255,255,255), C565(230,215,255), C565(60,20,120), C565(50,20,110), C565(40,10,90), C565(80,40,150), C565(80,40,130) },
  { "KOYU", C565(30,34,40), C565(40,44,52), C565(255,200,0), C565(20,20,20), C565(70,76,90), C565(255,200,0), C565(230,230,230), C565(255,255,255), C565(90,95,105), C565(255,200,0) },
};
static const int THEME_COUNT = sizeof(themes) / sizeof(themes[0]);

struct BananaBody { const char* name; uint16_t y, d, l; };

static const BananaBody bodies[] = {
  { "SARI", C565(255,255,0), C565(224,220,28), C565(238,238,144) },
  { "YESIL", C565(120,220,40), C565(80,170,30), C565(180,240,120) },
  { "PEMBE", C565(255,120,190), C565(215,80,150), C565(255,180,220) },
  { "MAVI", C565(80,160,255), C565(50,110,210), C565(160,205,255) },
  { "TURUNCU", C565(255,160,0), C565(215,120,0), C565(255,200,120) },
  { "KIRMIZI", C565(240,50,50), C565(190,30,30), C565(255,130,130) },
  { "MOR", C565(170,90,230), C565(130,60,190), C565(205,160,245) },
};
static const int BODY_COUNT = sizeof(bodies) / sizeof(bodies[0]);

struct BgColor { const char* name; uint16_t c; };

static const BgColor bgColors[] = {
  { "YESIL", C565(30,226,30) },
  { "SIYAH", C565(0,0,0) },
  { "BEYAZ", C565(255,255,255) },
  { "MAVI", C565(30,100,230) },
  { "KIRMIZI", C565(220,30,30) },
  { "PEMBE", C565(255,150,200) },
  { "TURUNCU", C565(255,140,0) },
  { "MOR", C565(110,40,170) },
};
static const int BGC_COUNT = sizeof(bgColors) / sizeof(bgColors[0]);

uint16_t COL_BG, COL_BG2, COL_HDR, COL_HDR_TXT, COL_SEL, COL_SEL_EDGE, COL_TXT, COL_SEL_TXT, COL_DK, COL_INFO;

int themeIdx = 0;
int bodyIdx = 0;
int bgcIdx = 0;

void applyTheme() {
  COL_BG = themes[themeIdx].bg;
  COL_BG2 = themes[themeIdx].bg2;
  COL_HDR = themes[themeIdx].hdr;
  COL_HDR_TXT = themes[themeIdx].hdrTxt;
  COL_SEL = themes[themeIdx].sel;
  COL_SEL_EDGE = themes[themeIdx].selEdge;
  COL_TXT = themes[themeIdx].txt;
  COL_SEL_TXT = themes[themeIdx].selTxt;
  COL_DK = themes[themeIdx].dk;
  COL_INFO = themes[themeIdx].info;
}

static const uint16_t IR_PIN_INT = 19;
static const uint16_t IR_PIN_EXT = 26;
IRsend* irsend = nullptr;
bool extLed = false;

struct IrCode { uint8_t protocol; uint64_t data; uint16_t bits; };
struct TvBrand { const char* name; const IrCode* codes; uint8_t codeCount; };

static const IrCode samsungCodes[] = { { 0, 0xE0E040BF, 32 } };
static const IrCode lgCodes[]      = { { 0, 0x20DF10EF, 32 } };
static const IrCode sonyCodes[]    = { { 1, 0xA90, 12 }, { 1, 0xA90, 15 } };
static const IrCode tclCodes[]     = { { 0, 0x57E318E7, 32 }, { 0, 0xEF00FF00, 32 } };
static const IrCode philipsCodes[] = { { 2, 0x0C, 12 } };
static const IrCode vestelCodes[]  = { { 0, 0x08F7807F, 32 }, { 0, 0x619E807F, 32 } };

static const TvBrand brands[] = {
  { "SAMSUNG", samsungCodes, sizeof(samsungCodes) / sizeof(samsungCodes[0]) },
  { "LG",      lgCodes,      sizeof(lgCodes)      / sizeof(lgCodes[0]) },
  { "SONY",    sonyCodes,    sizeof(sonyCodes)    / sizeof(sonyCodes[0]) },
  { "TCL",     tclCodes,     sizeof(tclCodes)     / sizeof(tclCodes[0]) },
  { "PHILIPS", philipsCodes, sizeof(philipsCodes) / sizeof(philipsCodes[0]) },
  { "VESTEL",  vestelCodes,  sizeof(vestelCodes)  / sizeof(vestelCodes[0]) },
};
#define BRAND_COUNT 6
static bool brandState[BRAND_COUNT] = { false, false, false, false, false, false };

void applyIrPin() {
  if (irsend != nullptr) {
    delete irsend;
    irsend = nullptr;
  }
  uint16_t active = extLed ? IR_PIN_EXT : IR_PIN_INT;
  uint16_t other  = extLed ? IR_PIN_INT : IR_PIN_EXT;
  pinMode(other, OUTPUT);
  digitalWrite(other, LOW);
  irsend = new IRsend(active);
  irsend->begin();
}

#define STATE_BOOT 0
#define STATE_MAIN 1
#define STATE_IR_MENU 2
#define STATE_IR_DEVICE 3
#define STATE_PIN 4
#define STATE_SETTINGS 5
#define STATE_WIFI 6
#define STATE_WIFI_LIST 7
#define STATE_WIFI_DETAIL 8
#define STATE_BLE 9
int appState = STATE_BOOT;

#define M_IR 0
#define M_WIFI 1
#define M_BLE 2
#define M_SET 3
#define M_PIN 4
#define MAIN_MAX 5
const char* mainNames[MAIN_MAX];
int mainIds[MAIN_MAX];
int MAIN_COUNT = 0;

int mainIndex = 0;
int irIndex = 0;
int irDeviceBrand = 0;
int pinIndex = 0;
int setIndex = 0;
int wifiMenuIndex = 0;
uint32_t bootStartMs = 0;

uint32_t lastActivityMs = 0;
bool dimmed = false;

Preferences prefs;

static const char* const bananaPix[] = {
  ".....................YY",
  "...................YYYY.....Y",
  "..................YY.Y....YYY.BBB",
  ".................YY..Y...YY...YDBB",
  "................YY..Y...YY..YYDDBB",
  "....................Y.YY...YYYYYY",
  "............YYY....Y.YY....YYYYYY",
  "..............YY...YYY....YWYYYY",
  "...............Y...YY....YWYYYYY",
  "........YY.....YY.......DDYYYYYY",
  ".........Y......Y......DYYYYYYD",
  "..........Y.....Y.....YLYYYYYDD",
  "....Y.....YY....Y....LLYYYYYYLL",
  "Y...YYY....YY...Y....YYYYYYYDL",
  "YY..YYYY....YYYY....YYYYYYYYDL",
  "YYY..Y..YY..........YYYYYYYYD",
  ".YYY.Y...YYY......DYYYYYYYYW",
  ".Y.YYY.....Y.....DDYYYYYYYW",
  ".Y..YYY........YYYYYYYYYLY",
  ".Y...YY.......YYYYYYYYYLL",
  ".Y....Y.....YYYYYYWYYYYL",
  ".Y.........YDDYYYWWYYYLL",
  ".YY.......LLDYYYYYYYYY",
  "..Y......LLDYYYYYYYYY",
  ".......YYYYYYYYYLLYY",
  ".....YYYYYYYYYYLLY",
  ".BBYYYYYYYYYYY",
  ".BBBYYYYLYY",
  ".BBBYYYLL",
  "..BB",
};
#define BANANA_ROWS 30
#define BANANA_COLS 34

void drawBananaArt(lgfx::LGFXBase &d) {
  uint16_t cBg = bgColors[bgcIdx].c;
  uint16_t cY  = bodies[bodyIdx].y;
  uint16_t cD  = bodies[bodyIdx].d;
  uint16_t cL  = bodies[bodyIdx].l;
  uint16_t cW  = d.color565(255, 255, 255);
  uint16_t cB  = d.color565(116, 110, 16);

  int scale = d.height() / BANANA_ROWS;
  if (scale < 1) scale = 1;
  int offX = (d.width()  - BANANA_COLS * scale) / 2;
  int offY = (d.height() - BANANA_ROWS * scale) / 2;

  d.fillScreen(cBg);
  for (int r = 0; r < BANANA_ROWS; r++) {
    const char* row = bananaPix[r];
    for (int c = 0; row[c] != '\0'; c++) {
      uint16_t col;
      switch (row[c]) {
        case 'Y': col = cY; break;
        case 'D': col = cD; break;
        case 'L': col = cL; break;
        case 'W': col = cW; break;
        case 'B': col = cB; break;
        default:  continue;
      }
      d.fillRect(offX + c * scale, offY + r * scale, scale, scale, col);
    }
  }
}

void drawBootSplash() {
  M5.Display.startWrite();
  drawBananaArt(M5.Display);
  M5.Display.endWrite();
}

void previewBanana() {
  M5.Display.startWrite();
  drawBananaArt(M5.Display);
  M5.Display.endWrite();
  delay(1200);
}

void drawBatteryIconOn(lgfx::LGFXBase &d, int rightX, int topY, uint16_t emptyBg) {
  int lvl = M5.Power.getBatteryLevel();
  if (lvl < 0) lvl = 0;
  if (lvl > 100) lvl = 100;
  bool chg = M5.Power.isCharging();

  int w = 22, h = 11;
  int x = rightX - w - 5;
  int y = topY;

  uint16_t fillCol = (lvl > 50) ? COL_BATT_OK : ((lvl > 20) ? COL_BATT_MID : COL_BATT_LOW);

  d.fillRect(x, y, w, h, emptyBg);
  d.drawRect(x, y, w, h, COL_HDR_TXT);
  d.fillRect(x + w, y + 3, 2, h - 6, COL_HDR_TXT);

  int innerW = w - 4;
  int fillW = innerW * lvl / 100;
  if (fillW > 0) d.fillRect(x + 2, y + 2, fillW, h - 4, fillCol);

  if (chg) {
    d.setTextFont(1);
    d.setTextDatum(middle_center);
    d.setTextColor(COL_HDR_TXT);
    d.drawString("+", x + w / 2, y + h / 2 + 1);
  }

  char pct[6];
  snprintf(pct, sizeof(pct), "%d", lvl);
  d.setTextFont(1);
  d.setTextDatum(middle_right);
  d.setTextColor(COL_HDR_TXT, emptyBg);
  d.drawString(pct, x - 2, y + h / 2 + 1);
}

void drawBatteryIcon(int rightX, int topY, uint16_t emptyBg) {
  drawBatteryIconOn(M5.Display, rightX, topY, emptyBg);
}

void drawMarqueeRowOn(lgfx::LGFXBase &d, int x, int y, int w, int rowH, const char* text, uint16_t col, uint16_t bg) {
  d.setTextFont(4);
  d.setTextDatum(middle_left);
  int tw = d.textWidth(text);
  if (tw <= w) {
    d.setTextColor(col, bg);
    d.drawString(text, x, y + rowH / 2);
    return;
  }
  d.setClipRect(x, y, w, rowH);
  d.setTextColor(col, bg);
  int gap = 26;
  int cyc = tw + gap;
  int off = (int)((millis() / 40) % (uint32_t)cyc);
  d.drawString(text, x - off, y + rowH / 2);
  d.drawString(text, x - off + cyc, y + rowH / 2);
  d.clearClipRect();
}

void sendBrandCode(int bi) {
  for (int i = 0; i < brands[bi].codeCount; i++) {
    uint8_t p = brands[bi].codes[i].protocol;
    uint64_t data = brands[bi].codes[i].data;
    uint16_t bits = brands[bi].codes[i].bits;
    if (p == 0) irsend->sendNEC(data, bits);
    else if (p == 1) irsend->sendSony(data, bits, 3);
    else irsend->sendRC5(data, bits);
    delay(40);
  }
}

void drawList(const char* title, const char* const* names, const char* const* rights,
              const uint16_t* rcols, int count, int sel) {
  auto& d = M5.Display;
  const int HDR_H = 22;
  const bool hasBar = (count > 3);
  const int listW = hasBar ? d.width() - 8 : d.width();

  d.startWrite();
  d.fillScreen(COL_BG);

  d.fillRect(0, 0, d.width(), HDR_H, COL_HDR);
  d.fillRect(0, HDR_H - 3, d.width(), 3, COL_DK);
  d.setTextDatum(middle_left);
  d.setTextColor(COL_HDR_TXT, COL_HDR);
  d.setTextFont(2);
  d.drawString(title, 6, (HDR_H - 3) / 2 + 1);

  drawBatteryIcon(d.width(), 5, COL_HDR);

  const int rowH = (d.height() - HDR_H) / 3;
  int firstVisible = sel - 1;
  if (firstVisible > count - 3) firstVisible = count - 3;
  if (firstVisible < 0) firstVisible = 0;

  for (int row = 0; row < 3 && (firstVisible + row) < count; row++) {
    int idx = firstVisible + row;
    int y = HDR_H + row * rowH;
    bool selected = (idx == sel);

    uint16_t bg = (idx & 1) ? COL_BG2 : COL_BG;
    d.fillRect(0, y, listW, rowH, bg);

    uint16_t txtCol = COL_TXT;
    if (selected) {
      d.fillRoundRect(4, y + 2, listW - 8, rowH - 4, 7, COL_SEL);
      d.drawRoundRect(4, y + 2, listW - 8, rowH - 4, 7, COL_SEL_EDGE);
      d.drawRoundRect(5, y + 3, listW - 10, rowH - 6, 6, COL_SEL_EDGE);
      int ay = y + rowH / 2;
      d.fillTriangle(11, ay - 6, 11, ay + 6, 19, ay, COL_SEL_EDGE);
      txtCol = COL_SEL_TXT;
    }

    d.setTextColor(txtCol);
    d.setTextFont(4);
    d.setTextDatum(middle_left);
    d.drawString(names[idx], 26, y + rowH / 2);

    if (rights != nullptr && rights[idx] != nullptr) {
      d.setTextFont(2);
      d.setTextDatum(middle_right);
      d.setTextColor(rcols[idx]);
      d.drawString(rights[idx], listW - 14, y + rowH / 2);
    }
  }

  if (hasBar) {
    int trackH = d.height() - HDR_H;
    int thumbH = trackH * 3 / count;
    if (thumbH < 10) thumbH = 10;
    int thumbY = HDR_H + (trackH - thumbH) * firstVisible / (count - 3);
    d.fillRect(d.width() - 7, HDR_H, 7, trackH, COL_DK);
    d.fillRoundRect(d.width() - 6, thumbY, 5, thumbH, 2, COL_HDR);
  }
  d.endWrite();
}

void buildMainMenu() {
  MAIN_COUNT = 0;
  mainNames[MAIN_COUNT] = "IR";          mainIds[MAIN_COUNT++] = M_IR;
  mainNames[MAIN_COUNT] = "WIFI";        mainIds[MAIN_COUNT++] = M_WIFI;
  mainNames[MAIN_COUNT] = "BANANA BLE";  mainIds[MAIN_COUNT++] = M_BLE;
  mainNames[MAIN_COUNT] = "AYARLAR";     mainIds[MAIN_COUNT++] = M_SET;
  mainNames[MAIN_COUNT] = "PIN AYARI";   mainIds[MAIN_COUNT++] = M_PIN;
}

void drawMainMenu() {
  drawList("BANANA MENU", mainNames, nullptr, nullptr, MAIN_COUNT, mainIndex);
}

static const char* const setNames[5] = { "TEMA", "MUZ", "ARKA PLAN", "SIFIRLA", "< GERI" };

void drawSettingsMenu() {
  const char* rights[5] = { themes[themeIdx].name, bodies[bodyIdx].name, bgColors[bgcIdx].name, nullptr, nullptr };
  uint16_t rcols[5] = { COL_INFO, COL_INFO, COL_INFO, 0, 0 };
  drawList("AYARLAR", setNames, rights, rcols, 5, setIndex);
}

void saveSettings() {
  prefs.putUInt("th", themeIdx);
  prefs.putUInt("bd", bodyIdx);
  prefs.putUInt("bg", bgcIdx);
}

static const char* irNames[BRAND_COUNT + 1];

void drawIrMenu() {
  const char* rights[BRAND_COUNT + 1];
  uint16_t rcols[BRAND_COUNT + 1];
  for (int i = 0; i < BRAND_COUNT; i++) {
    rights[i] = brandState[i] ? "ON" : "OFF";
    rcols[i] = brandState[i] ? COL_ON : COL_OFF;
  }
  rights[BRAND_COUNT] = nullptr;
  rcols[BRAND_COUNT] = 0;
  drawList("IR - TV-B-GONE", irNames, rights, rcols, BRAND_COUNT + 1, irIndex);
}

void drawDeviceScreen(int bi, bool on) {
  auto& d = M5.Display;
  d.startWrite();
  d.fillScreen(COL_BG);
  d.fillRect(0, 0, d.width(), 22, COL_HDR);
  d.fillRect(0, 19, d.width(), 3, COL_DK);
  d.setTextDatum(middle_left);
  d.setTextColor(COL_HDR_TXT, COL_HDR);
  d.setTextFont(2);
  d.drawString(brands[bi].name, 6, 10);
  drawBatteryIcon(d.width(), 5, COL_HDR);

  d.setTextColor(COL_TXT, COL_BG);
  d.setTextFont(4);
  d.setTextDatum(middle_center);
  d.drawString("ON / OFF", d.width() / 2, d.height() / 2 - 2);

  int cy = d.height() - 26;
  int cxOn = d.width() / 2 - 20, cxOff = d.width() / 2 + 20, r = 10;
  d.fillCircle(cxOn, cy, r, on ? TFT_GREEN : d.color565(80, 140, 40));
  d.drawCircle(cxOn, cy, r, TFT_BLACK);
  d.fillCircle(cxOff, cy, r, !on ? TFT_RED : d.color565(140, 80, 40));
  d.drawCircle(cxOff, cy, r, TFT_BLACK);

  d.setTextFont(1);
  d.setTextDatum(bottom_center);
  d.setTextColor(COL_TXT, COL_BG);
  d.drawString("M5: Gonder   Yan tus: Geri", d.width() / 2, d.height() - 2);
  d.endWrite();
}

static const char* const pinNames[3] = { "G26 DIS LED", "AKTIF LED", "< GERI" };

void drawPinMenu() {
  const char* rights[3] = { extLed ? "ON" : "OFF", extLed ? "G26" : "G19", nullptr };
  uint16_t rcols[3] = { (uint16_t)(extLed ? COL_ON : COL_OFF), (uint16_t)COL_INFO, 0 };
  drawList("PIN: GND + G26", pinNames, rights, rcols, 3, pinIndex);
}

M5Canvas canvas(&M5.Display);

int GW = 240, GH = 135;
bool canvasOk = false;

bool canvasEnter() {
  GW = M5.Display.width();
  GH = M5.Display.height();
  canvas.setColorDepth(16);
  canvas.setPsram(true);
  canvasOk = (canvas.createSprite(GW, GH) != nullptr);
  return canvasOk;
}

void canvasLeave() {
  canvas.deleteSprite();
  canvasOk = false;
}

#define WIFI_MAX 20
char ssidBuf[WIFI_MAX][33];
char rssiBuf[WIFI_MAX][8];
int wifiCount = 0;
int wifiListIndex = 0;
uint32_t wifiListNextDrawMs = 0;

#define WIFI_COUNT 2
static const char* const wifiMainNames[WIFI_COUNT] = { "AG TARA", "< GERI" };

void drawWifiMenu() {
  drawList("WIFI", wifiMainNames, nullptr, nullptr, WIFI_COUNT, wifiMenuIndex);
}

void wifiDoScan() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);

  int n = WiFi.scanNetworks();
  wifiCount = 0;
  for (int i = 0; i < n && wifiCount < WIFI_MAX; i++) {
    String ss = WiFi.SSID(i);
    if (ss.length() == 0) continue;
    strncpy(ssidBuf[wifiCount], ss.c_str(), 32);
    ssidBuf[wifiCount][32] = '\0';
    snprintf(rssiBuf[wifiCount], sizeof(rssiBuf[0]), "%d", (int)WiFi.RSSI(i));
    wifiCount++;
  }
  WiFi.scanDelete();
}

void wifiScanAndShow() {
  auto& d = M5.Display;
  d.startWrite();
  d.fillScreen(COL_BG);
  d.fillRect(0, 0, d.width(), 22, COL_HDR);
  d.fillRect(0, 19, d.width(), 3, COL_DK);
  d.setTextDatum(middle_center);
  d.setTextColor(COL_HDR_TXT, COL_HDR);
  d.setTextFont(2);
  d.drawString("AGLAR TARANIYOR...", d.width() / 2, 10);
  d.endWrite();

  wifiDoScan();
  wifiListIndex = 0;
}

void drawWifiScanList() {
  if (!canvasOk) { if (!canvasEnter()) return; }
  const int HDR_H = 22;

  canvas.fillScreen(COL_BG);
  canvas.fillRect(0, 0, GW, HDR_H, COL_HDR);
  canvas.fillRect(0, HDR_H - 3, GW, 3, COL_DK);
  canvas.setTextDatum(middle_left);
  canvas.setTextColor(COL_HDR_TXT, COL_HDR);
  canvas.setTextFont(2);
  canvas.drawString("WIFI TARAMA", 6, (HDR_H - 3) / 2 + 1);
  drawBatteryIconOn(canvas, GW, 5, COL_HDR);

  if (wifiCount == 0) {
    canvas.setTextFont(2);
    canvas.setTextDatum(middle_center);
    canvas.setTextColor(COL_TXT, COL_BG);
    canvas.drawString("AG YOK - GERI (M5)", GW / 2, HDR_H + (GH - HDR_H) / 2);
    canvas.pushSprite(0, 0);
    return;
  }

  int count = wifiCount + 1;
  bool hasBar = count > 3;
  int listW = hasBar ? GW - 8 : GW;
  int sel = wifiListIndex;
  const int rowH = (GH - HDR_H) / 3;
  int firstVisible = sel - 1;
  if (firstVisible > count - 3) firstVisible = count - 3;
  if (firstVisible < 0) firstVisible = 0;

  for (int row = 0; row < 3 && (firstVisible + row) < count; row++) {
    int idx = firstVisible + row;
    int y = HDR_H + row * rowH;
    bool selected = (idx == sel);
    uint16_t bg = (idx & 1) ? COL_BG2 : COL_BG;
    canvas.fillRect(0, y, listW, rowH, bg);
    uint16_t txtCol = COL_TXT;
    if (selected) {
      canvas.fillRoundRect(4, y + 2, listW - 8, rowH - 4, 7, COL_SEL);
      canvas.drawRoundRect(4, y + 2, listW - 8, rowH - 4, 7, COL_SEL_EDGE);
      int ay = y + rowH / 2;
      canvas.fillTriangle(11, ay - 6, 11, ay + 6, 19, ay, COL_SEL_EDGE);
      txtCol = COL_SEL_TXT;
      bg = COL_SEL;
    }
    int textX = 26;
    if (idx == wifiCount) {
      canvas.setTextFont(4);
      canvas.setTextDatum(middle_left);
      canvas.setTextColor(txtCol, bg);
      canvas.drawString("< GERI", textX, y + rowH / 2);
    } else {
      int textW = listW - textX - 50;
      drawMarqueeRowOn(canvas, textX, y, textW, rowH, ssidBuf[idx], txtCol, bg);
      canvas.setTextFont(2);
      canvas.setTextDatum(middle_right);
      canvas.setTextColor(COL_INFO, bg);
      canvas.drawString(rssiBuf[idx], listW - 14, y + rowH / 2);
    }
  }

  if (hasBar) {
    int trackH = GH - HDR_H;
    int thumbH = trackH * 3 / count;
    if (thumbH < 10) thumbH = 10;
    int thumbY = HDR_H + (trackH - thumbH) * firstVisible / (count - 3);
    canvas.fillRect(GW - 7, HDR_H, 7, trackH, COL_DK);
    canvas.fillRoundRect(GW - 6, thumbY, 5, thumbH, 2, COL_HDR);
  }
  canvas.pushSprite(0, 0);
}

bool wifiListNeedsMarquee() {
  if (wifiCount == 0) return false;
  if (wifiListIndex >= wifiCount) return false;
  canvas.setTextFont(4);
  int tw = canvas.textWidth(ssidBuf[wifiListIndex]);
  int count = wifiCount + 1;
  bool hasBar = count > 3;
  int listW = hasBar ? GW - 8 : GW;
  int availW = listW - 26 - 50;
  return tw > availW;
}

float estimateDistance(int rssi) {
  const float txPower = -45.0f;
  const float n = 2.5f;
  float d = pow(10.0f, (txPower - (float)rssi) / (10.0f * n));
  if (d < 0.3f) d = 0.3f;
  if (d > 60.0f) d = 60.0f;
  return d;
}

#define HIST_N 40
char detailSsid[33];
int detailRssi = -100;
float rssiHistory[HIST_N];
int histCount = 0;
uint32_t detailNextSampleMs = 0;
uint32_t detailRevealUntil = 0;
uint32_t wifiDetailLastDrawMs = 0;
bool photoDrawn = false;

void wifiDetailEnter(int idx) {
  if (idx < 0 || idx >= wifiCount) return;
  if (!canvasOk) canvasEnter();
  strncpy(detailSsid, ssidBuf[idx], 32);
  detailSsid[32] = '\0';
  detailRssi = atoi(rssiBuf[idx]);
  histCount = 0;
  photoDrawn = false;
  detailNextSampleMs = millis() + 300;
  detailRevealUntil = millis() + 1100;
  wifiDetailLastDrawMs = 0;
  appState = STATE_WIFI_DETAIL;
}

void wifiDetailSampleIfDue() {
  if (millis() < detailNextSampleMs) return;
  detailNextSampleMs = millis() + 2500;

  int n = WiFi.scanNetworks();
  int rssi = detailRssi;
  for (int i = 0; i < n; i++) {
    if (WiFi.SSID(i) == String(detailSsid)) { rssi = WiFi.RSSI(i); break; }
  }
  WiFi.scanDelete();
  detailRssi = rssi;

  if (histCount < HIST_N) {
    rssiHistory[histCount++] = (float)rssi;
  } else {
    for (int i = 1; i < HIST_N; i++) rssiHistory[i - 1] = rssiHistory[i];
    rssiHistory[HIST_N - 1] = (float)rssi;
  }
}

void wifiDetailDraw(bool showingPhoto) {
  if (!canvasOk) { if (!canvasEnter()) return; }

  if (showingPhoto) {
    if (!photoDrawn) {
      drawBananaArt(canvas);
      canvas.setTextDatum(bottom_center);
      canvas.setTextColor(TFT_WHITE);
      canvas.setTextFont(2);
      canvas.drawString("TARANIYOR...", GW / 2, GH - 4);
      canvas.pushSprite(0, 0);
      photoDrawn = true;
    }
    return;
  }

  const int HDR_H = 22;
  canvas.fillScreen(TFT_BLACK);
  canvas.fillRect(0, 0, GW, HDR_H, COL_HDR);
  canvas.fillRect(0, HDR_H - 3, GW, 3, COL_DK);
  drawMarqueeRowOn(canvas, 6, 0, GW - 70, HDR_H, detailSsid, COL_HDR_TXT, COL_HDR);
  drawBatteryIconOn(canvas, GW, 5, COL_HDR);

  int gx = 4, gy = HDR_H + 4, gw = GW - 8, gh = 46;
  canvas.drawRect(gx, gy, gw, gh, TFT_DARKGREY);
  int bw = gw / HIST_N;
  if (bw < 1) bw = 1;
  for (int i = 0; i < histCount; i++) {
    float r = rssiHistory[i];
    float t = (r + 90.0f) / 60.0f;
    if (t < 0) t = 0;
    if (t > 1) t = 1;
    int bh = (int)(t * (gh - 4));
    uint16_t col = canvas.color565((int)(255 * (1 - t)), (int)(220 * t), 40);
    canvas.fillRect(gx + 2 + i * bw, gy + gh - 2 - bh, (bw > 1 ? bw - 1 : 1), bh, col);
  }

  char buf[24];
  snprintf(buf, sizeof(buf), "%d dBm", detailRssi);
  canvas.setTextFont(2);
  canvas.setTextDatum(top_left);
  canvas.setTextColor(TFT_WHITE, TFT_BLACK);
  canvas.drawString(buf, gx + 2, gy + gh + 2);

  int qual = constrain(map(detailRssi, -90, -30, 0, 100), 0, 100);
  snprintf(buf, sizeof(buf), "KALITE %%%d", qual);
  canvas.setTextDatum(top_right);
  canvas.drawString(buf, gx + gw - 2, gy + gh + 2);

  float dist = estimateDistance(detailRssi);
  snprintf(buf, sizeof(buf), "~%.1f m (tahmini)", dist);
  canvas.setTextFont(1);
  canvas.setTextDatum(top_center);
  canvas.setTextColor(TFT_WHITE, TFT_BLACK);
  canvas.drawString(buf, GW / 2, gy + gh + 22);

  canvas.setTextDatum(bottom_center);
  canvas.drawString("YAN TUS: GERI", GW / 2, GH - 2);
  canvas.pushSprite(0, 0);
}

NimBLEServer* bleServer = nullptr;
NimBLEHIDDevice* hid = nullptr;
NimBLECharacteristic* kbChar = nullptr;
NimBLECharacteristic* ccChar = nullptr;
bool bleStarted = false;
bool bleAdvOn = false;
volatile bool bleConnected = false;
volatile bool bleJustConnected = false;
bool bleNeedDraw = true;
int bleMode = 0;
uint32_t bleSplashUntil = 0;
uint32_t bleFlashUntil = 0;

static const char* const bleModeNames[3] = { "TELEFON SELFIE", "WINDOWS EKRAN GORUNTUSU", "MAC EKRAN GORUNTUSU" };

class BananaCallbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer* srv, NimBLEConnInfo& info) override {
    bleConnected = true;
    bleJustConnected = true;
  }
  void onDisconnect(NimBLEServer* srv, NimBLEConnInfo& info, int reason) override {
    bleConnected = false;
    bleNeedDraw = true;
    if (bleAdvOn) NimBLEDevice::startAdvertising();
  }
};

void bleSendKey(uint8_t modifier, uint8_t key) {
  if (!bleConnected || kbChar == nullptr) return;
  uint8_t press[8] = { modifier, 0, key, 0, 0, 0, 0, 0 };
  kbChar->setValue(press, sizeof(press));
  kbChar->notify();
  delay(40);
  uint8_t rel[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
  kbChar->setValue(rel, sizeof(rel));
  kbChar->notify();
}

void bleSendConsumer(uint16_t usage) {
  if (!bleConnected || ccChar == nullptr) return;
  uint8_t press[2] = { (uint8_t)(usage & 0xFF), (uint8_t)(usage >> 8) };
  ccChar->setValue(press, sizeof(press));
  ccChar->notify();
  delay(60);
  uint8_t rel[2] = { 0, 0 };
  ccChar->setValue(rel, sizeof(rel));
  ccChar->notify();
}

void bleShoot() {
  if (!bleConnected) return;
  if (bleMode == 0) {
    bleSendConsumer(0x00E9);
  } else if (bleMode == 1) {
    bleSendKey(0x08 | 0x02, 0x16);
  } else {
    bleSendKey(0x08 | 0x02, 0x20);
  }
  bleFlashUntil = millis() + 500;
  bleNeedDraw = true;
}

void bleStartBanana() {
  if (!bleStarted) {
    NimBLEDevice::init("Banana BLE");
    NimBLEDevice::setPower(ESP_PWR_LVL_N0);
    NimBLEDevice::setSecurityAuth(true, false, true);

    bleServer = NimBLEDevice::createServer();
    bleServer->setCallbacks(new BananaCallbacks());

    hid = new NimBLEHIDDevice(bleServer);
    kbChar = hid->getInputReport(1);
    ccChar = hid->getInputReport(2);

    hid->setManufacturer("Banana");
    hid->setPnp(0x02, 0x05ac, 0x820a, 0x0210);
    hid->setHidInfo(0x00, 0x01);

    static const uint8_t reportMap[] = {
      0x05, 0x01, 0x09, 0x06, 0xA1, 0x01,
      0x85, 0x01,
      0x05, 0x07, 0x19, 0xE0, 0x29, 0xE7,
      0x15, 0x00, 0x25, 0x01, 0x75, 0x01, 0x95, 0x08, 0x81, 0x02,
      0x95, 0x01, 0x75, 0x08, 0x81, 0x01,
      0x95, 0x06, 0x75, 0x08, 0x15, 0x00, 0x25, 0x65,
      0x05, 0x07, 0x19, 0x00, 0x29, 0x65, 0x81, 0x00,
      0xC0,
      0x05, 0x0C, 0x09, 0x01, 0xA1, 0x01,
      0x85, 0x02,
      0x15, 0x00, 0x26, 0xFF, 0x03,
      0x19, 0x00, 0x2A, 0xFF, 0x03,
      0x75, 0x10, 0x95, 0x01, 0x81, 0x00,
      0xC0
    };
    hid->setReportMap((uint8_t*)reportMap, sizeof(reportMap));
    hid->startServices();

    NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
    adv->setName("Banana BLE");
    adv->setAppearance(0x03C1);
    adv->addServiceUUID(hid->getHidService()->getUUID());

    hid->setBatteryLevel(100);
    bleStarted = true;
  }
  NimBLEDevice::startAdvertising();
  bleAdvOn = true;
}

void bleStopBanana() {
  bleAdvOn = false;
  if (!bleStarted) return;
  NimBLEDevice::stopAdvertising();
  if (bleServer != nullptr) {
    std::vector<uint16_t> peers = bleServer->getPeerDevices();
    for (size_t i = 0; i < peers.size(); i++) bleServer->disconnect(peers[i]);
  }
  bleConnected = false;
}

void drawBleScreen() {
  if (!canvasOk) { if (!canvasEnter()) return; }
  const int HDR_H = 22;

  if (bleConnected && millis() < bleSplashUntil) {
    drawBananaArt(canvas);
    canvas.setTextDatum(bottom_center);
    canvas.setTextColor(TFT_WHITE);
    canvas.setTextFont(2);
    canvas.drawString("MUZ BAGLANTISI KURULDU!", GW / 2, GH - 4);
    canvas.pushSprite(0, 0);
    return;
  }

  canvas.fillScreen(COL_BG);
  canvas.fillRect(0, 0, GW, HDR_H, COL_HDR);
  canvas.fillRect(0, HDR_H - 3, GW, 3, COL_DK);
  canvas.setTextDatum(middle_left);
  canvas.setTextColor(COL_HDR_TXT, COL_HDR);
  canvas.setTextFont(2);
  canvas.drawString("BANANA BLE", 6, (HDR_H - 3) / 2 + 1);
  drawBatteryIconOn(canvas, GW, 5, COL_HDR);

  canvas.setTextDatum(middle_center);
  if (bleConnected) {
    canvas.fillCircle(GW / 2, HDR_H + 18, 7, COL_ON);
    canvas.setTextFont(2);
    canvas.setTextColor(COL_TXT, COL_BG);
    canvas.drawString("MUZ BAGLANDI", GW / 2, HDR_H + 38);
    canvas.setTextFont(1);
    canvas.setTextColor(COL_INFO, COL_BG);
    canvas.drawString(bleModeNames[bleMode], GW / 2, HDR_H + 56);
    canvas.setTextColor(COL_TXT, COL_BG);
    canvas.drawString("M5: MUZ CEK   B: MOD   YAN: GERI", GW / 2, GH - 8);
  } else {
    canvas.fillCircle(GW / 2, HDR_H + 18, 7, bleAdvOn ? COL_BATT_MID : COL_OFF);
    canvas.setTextFont(2);
    canvas.setTextColor(COL_TXT, COL_BG);
    canvas.drawString(bleAdvOn ? "MUZ YAYINDA" : "MUZ UYUYOR", GW / 2, HDR_H + 38);
    canvas.setTextFont(1);
    canvas.setTextColor(COL_INFO, COL_BG);
    canvas.drawString("Telefon BLE ayarlarinda: Banana BLE", GW / 2, HDR_H + 56);
    canvas.setTextColor(COL_TXT, COL_BG);
    canvas.drawString("M5: ACIK/KAPALI   YAN: GERI", GW / 2, GH - 8);
  }

  if (millis() < bleFlashUntil) {
    canvas.fillRect(0, 0, GW, GH, TFT_WHITE);
    canvas.setTextDatum(middle_center);
    canvas.setTextFont(4);
    canvas.setTextColor(TFT_BLACK, TFT_WHITE);
    canvas.drawString("MUZ CEKILDI!", GW / 2, GH / 2);
  }
  canvas.pushSprite(0, 0);
}

void setup() {
  setCpuFrequencyMhz(CPU_MHZ);

  auto cfg = M5.config();
  cfg.internal_spk = false;
  cfg.internal_mic = false;
  M5.begin(cfg);
  M5.Display.setRotation(SCREEN_ROTATION);
  M5.Display.setBrightness(BRIGHT_NORMAL);

  prefs.begin("banana", false);
  extLed = prefs.getBool("ext", false);
  themeIdx = prefs.getUInt("th", 0);
  bodyIdx = prefs.getUInt("bd", 0);
  bgcIdx = prefs.getUInt("bg", 0);
  if (themeIdx < 0 || themeIdx >= THEME_COUNT) themeIdx = 0;
  if (bodyIdx < 0 || bodyIdx >= BODY_COUNT) bodyIdx = 0;
  if (bgcIdx < 0 || bgcIdx >= BGC_COUNT) bgcIdx = 0;
  applyTheme();

  applyIrPin();
  WiFi.mode(WIFI_OFF);

  buildMainMenu();
  for (int i = 0; i < BRAND_COUNT; i++) irNames[i] = brands[i].name;
  irNames[BRAND_COUNT] = "< GERI";

  drawBootSplash();
  bootStartMs = millis();
  lastActivityMs = millis();
  appState = STATE_BOOT;
}

void loop() {
  M5.update();

  bool anyBtn = M5.BtnA.wasPressed() || M5.BtnB.wasPressed() || M5.BtnPWR.wasPressed();
  if (anyBtn) {
    lastActivityMs = millis();
    if (dimmed) {
      dimmed = false;
      M5.Display.setBrightness(BRIGHT_NORMAL);
    }
  }
  if (appState == STATE_WIFI_DETAIL || appState == STATE_BLE) {
    lastActivityMs = millis();
  } else if (!dimmed && appState != STATE_BOOT && millis() - lastActivityMs > DIM_AFTER_MS) {
    dimmed = true;
    M5.Display.setBrightness(BRIGHT_DIM);
  }

  switch (appState) {
    case STATE_BOOT:
      if (millis() - bootStartMs >= 3000) {
        appState = STATE_MAIN;
        drawMainMenu();
      }
      break;

    case STATE_MAIN:
      if (M5.BtnB.wasPressed()) {
        mainIndex = (mainIndex + 1) % MAIN_COUNT;
        drawMainMenu();
      }
      if (M5.BtnPWR.wasPressed()) {
        mainIndex = (mainIndex - 1 + MAIN_COUNT) % MAIN_COUNT;
        drawMainMenu();
      }
      if (M5.BtnA.wasPressed()) {
        switch (mainIds[mainIndex]) {
          case M_IR:
            appState = STATE_IR_MENU;
            irIndex = 0;
            drawIrMenu();
            break;
          case M_WIFI:
            WiFi.mode(WIFI_STA);
            appState = STATE_WIFI;
            wifiMenuIndex = 0;
            drawWifiMenu();
            break;
          case M_BLE:
            if (!canvasOk) canvasEnter();
            appState = STATE_BLE;
            bleSplashUntil = 0;
            bleFlashUntil = 0;
            bleJustConnected = false;
            bleStartBanana();
            bleNeedDraw = true;
            break;
          case M_SET:
            appState = STATE_SETTINGS;
            setIndex = 0;
            drawSettingsMenu();
            break;
          case M_PIN:
            appState = STATE_PIN;
            pinIndex = 0;
            drawPinMenu();
            break;
        }
      }
      break;

    case STATE_SETTINGS: {
      const int count = 5;
      if (M5.BtnB.wasPressed()) {
        setIndex = (setIndex + 1) % count;
        drawSettingsMenu();
      }
      if (M5.BtnPWR.wasPressed()) {
        setIndex = (setIndex - 1 + count) % count;
        drawSettingsMenu();
      }
      if (M5.BtnA.wasPressed()) {
        if (setIndex == 0) {
          themeIdx = (themeIdx + 1) % THEME_COUNT;
          applyTheme();
          saveSettings();
          drawSettingsMenu();
        } else if (setIndex == 1) {
          bodyIdx = (bodyIdx + 1) % BODY_COUNT;
          saveSettings();
          previewBanana();
          drawSettingsMenu();
        } else if (setIndex == 2) {
          bgcIdx = (bgcIdx + 1) % BGC_COUNT;
          saveSettings();
          previewBanana();
          drawSettingsMenu();
        } else if (setIndex == 3) {
          themeIdx = 0;
          bodyIdx = 0;
          bgcIdx = 0;
          applyTheme();
          saveSettings();
          drawSettingsMenu();
        } else {
          appState = STATE_MAIN;
          drawMainMenu();
        }
      }
      break;
    }

    case STATE_WIFI:
      if (M5.BtnB.wasPressed()) {
        wifiMenuIndex = (wifiMenuIndex + 1) % WIFI_COUNT;
        drawWifiMenu();
      }
      if (M5.BtnPWR.wasPressed()) {
        wifiMenuIndex = (wifiMenuIndex - 1 + WIFI_COUNT) % WIFI_COUNT;
        drawWifiMenu();
      }
      if (M5.BtnA.wasPressed()) {
        if (wifiMenuIndex == 0) {
          wifiScanAndShow();
          if (!canvasOk) canvasEnter();
          appState = STATE_WIFI_LIST;
          wifiListIndex = 0;
          wifiListNextDrawMs = 0;
          drawWifiScanList();
        } else {
          WiFi.mode(WIFI_OFF);
          appState = STATE_MAIN;
          drawMainMenu();
        }
      }
      break;

    case STATE_WIFI_LIST: {
      int count = (wifiCount == 0) ? 1 : (wifiCount + 1);
      bool changed = false;
      if (M5.BtnB.wasPressed()) { wifiListIndex = (wifiListIndex + 1) % count; changed = true; }
      if (M5.BtnPWR.wasPressed()) { wifiListIndex = (wifiListIndex - 1 + count) % count; changed = true; }
      if (changed) { wifiListNextDrawMs = millis() + 80; drawWifiScanList(); }

      if (M5.BtnA.wasPressed()) {
        if (wifiCount == 0 || wifiListIndex == wifiCount) {
          if (canvasOk) canvasLeave();
          appState = STATE_WIFI;
          drawWifiMenu();
        } else {
          wifiDetailEnter(wifiListIndex);
        }
      }

      if (appState == STATE_WIFI_LIST && wifiListNeedsMarquee() && millis() >= wifiListNextDrawMs) {
        wifiListNextDrawMs = millis() + 80;
        drawWifiScanList();
      }
      break;
    }

    case STATE_WIFI_DETAIL: {
      bool showingPhoto = millis() < detailRevealUntil;
      if (!showingPhoto) wifiDetailSampleIfDue();

      if (showingPhoto) {
        wifiDetailDraw(true);
      } else if (millis() - wifiDetailLastDrawMs >= 200) {
        wifiDetailLastDrawMs = millis();
        wifiDetailDraw(false);
      }
      if (!showingPhoto && (M5.BtnB.wasPressed() || M5.BtnPWR.wasPressed())) {
        appState = STATE_WIFI_LIST;
        wifiListNextDrawMs = 0;
        drawWifiScanList();
      }
      break;
    }

    case STATE_BLE: {
      static bool lastConn = false;
      if (bleJustConnected) {
        bleJustConnected = false;
        bleSplashUntil = millis() + 1500;
        bleNeedDraw = true;
      }
      if (bleConnected != lastConn) {
        lastConn = bleConnected;
        bleNeedDraw = true;
      }
      if (M5.BtnA.wasPressed()) {
        if (bleConnected) {
          if (millis() >= bleSplashUntil) bleShoot();
        } else {
          if (bleAdvOn) bleStopBanana();
          else bleStartBanana();
          bleNeedDraw = true;
        }
      }
      if (M5.BtnB.wasPressed() && bleConnected) {
        bleMode = (bleMode + 1) % 3;
        bleNeedDraw = true;
      }
      if (M5.BtnPWR.wasPressed()) {
        bleStopBanana();
        appState = STATE_MAIN;
        drawMainMenu();
        break;
      }
      if (bleSplashUntil != 0 && millis() >= bleSplashUntil) {
        bleSplashUntil = 0;
        bleNeedDraw = true;
      }
      if (bleFlashUntil != 0 && millis() >= bleFlashUntil) {
        bleFlashUntil = 0;
        bleNeedDraw = true;
      }
      if (bleNeedDraw) {
        bleNeedDraw = false;
        drawBleScreen();
      }
      break;
    }

    case STATE_PIN: {
      const int count = 3;
      if (M5.BtnB.wasPressed()) {
        pinIndex = (pinIndex + 1) % count;
        drawPinMenu();
      }
      if (M5.BtnPWR.wasPressed()) {
        pinIndex = (pinIndex - 1 + count) % count;
        drawPinMenu();
      }
      if (M5.BtnA.wasPressed()) {
        if (pinIndex == 2) {
          appState = STATE_MAIN;
          drawMainMenu();
        } else {
          extLed = !extLed;
          prefs.putBool("ext", extLed);
          applyIrPin();
          drawPinMenu();
        }
      }
      break;
    }

    case STATE_IR_MENU: {
      const int count = BRAND_COUNT + 1;
      if (M5.BtnB.wasPressed()) {
        irIndex = (irIndex + 1) % count;
        drawIrMenu();
      }
      if (M5.BtnPWR.wasPressed()) {
        irIndex = (irIndex - 1 + count) % count;
        drawIrMenu();
      }
      if (M5.BtnA.wasPressed()) {
        if (irIndex == BRAND_COUNT) {
          appState = STATE_MAIN;
          drawMainMenu();
        } else {
          irDeviceBrand = irIndex;
          appState = STATE_IR_DEVICE;
          drawDeviceScreen(irDeviceBrand, brandState[irDeviceBrand]);
        }
      }
      break;
    }

    case STATE_IR_DEVICE: {
      if (M5.BtnA.wasPressed()) {
        sendBrandCode(irDeviceBrand);
        brandState[irDeviceBrand] = !brandState[irDeviceBrand];
        drawDeviceScreen(irDeviceBrand, brandState[irDeviceBrand]);
      }
      if (M5.BtnB.wasPressed() || M5.BtnPWR.wasPressed()) {
        appState = STATE_IR_MENU;
        drawIrMenu();
      }
      break;
    }
  }

  bool fastLoop = (appState == STATE_WIFI_DETAIL || appState == STATE_WIFI_LIST);
  if (fastLoop) delay(1);
  else delay(20);
}
