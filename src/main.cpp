#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <DNSServer.h>
#include <EEPROM.h>
#include <MD_Parola.h>
#include <MD_MAX72xx.h>
#include <SPI.h>
#include <time.h>
#include <Updater.h>
#include "bigFont.h"

#define HARDWARE_TYPE MD_MAX72XX::FC16_HW
#define MAX_DEVICES 4
#define CS_PIN D6

#define MDNS_HOSTNAME "mhome"

#define NTP_SERVER "pool.ntp.org"
#define TIMEZONE_OFFSET 3
#define DST_OFFSET 0

#define EEPROM_SIZE 160
#define EEPROM_MAGIC 0xAA
#define EEPROM_MAGIC_ADDR 0
#define EEPROM_SSID_ADDR 1
#define EEPROM_SSID_LEN 32
#define EEPROM_PASS_ADDR (EEPROM_SSID_ADDR + EEPROM_SSID_LEN)
#define EEPROM_PASS_LEN 64
#define EEPROM_ADMIN_ADDR (EEPROM_PASS_ADDR + EEPROM_PASS_LEN)
#define EEPROM_ADMIN_LEN 32
#define EEPROM_ADMIN_MAGIC_ADDR (EEPROM_ADMIN_ADDR + EEPROM_ADMIN_LEN)
#define EEPROM_ADMIN_MAGIC 0xBB
#define EEPROM_BRIGHTNESS_MODE_ADDR (EEPROM_ADMIN_MAGIC_ADDR + 1)
#define EEPROM_BRIGHTNESS_VALUE_ADDR (EEPROM_BRIGHTNESS_MODE_ADDR + 1)
#define EEPROM_BRIGHTNESS_MAGIC_ADDR (EEPROM_BRIGHTNESS_VALUE_ADDR + 1)
#define EEPROM_BRIGHTNESS_MAGIC 0xCC
#define EEPROM_LANG_ADDR (EEPROM_BRIGHTNESS_MAGIC_ADDR + 1)
#define EEPROM_LANG_MAGIC_ADDR (EEPROM_LANG_ADDR + 1)
#define EEPROM_LANG_MAGIC 0xDD

#define LANG_EN 0
#define LANG_RU 1

#ifndef FW_VERSION
#define FW_VERSION "dev"
#endif
#ifndef BUILD_NUMBER
#define BUILD_NUMBER 0
#endif
#ifndef BUILD_TAG
#define BUILD_TAG "unknown"
#endif
#define STRINGIFY2(x) #x
#define STRINGIFY(x) STRINGIFY2(x)
#define RTC_MAGIC 0xC0FFEE01

uint8_t lang = LANG_EN;

enum LangKey {
  L_TAB_STATUS,
  L_TAB_SETTINGS,
  L_FIRMWARE,
  L_FREE_RAM,
  L_UPTIME,
  L_BOOT,
  L_RESET_LABEL,
  L_PREV,
  L_NTP_SYNC,
  L_SYSINFO,
  L_AUTO_SENSOR,
  L_BRIGHTNESS,
  L_REBOOT,
  L_REBOOT_CONFIRM,
  L_RESET_WIFI,
  L_RESET_WIFI_CONFIRM,
  L_NEVER,
  L_SEC_AGO,
  L_LOGIN_FIRST,
  L_PASSWORD_PLACEHOLDER,
  L_LOGIN_BTN,
  L_WRONG_PASSWORD,
  L_CURRENT,
  L_FIRMWARE_UPDATE,
  L_UPLOAD,
  L_UPDATE_CONFIRM,
  L_UPDATE_SUCCESS,
  L_UPDATE_FAIL,
  L_ONLINE,
  L_AUTO_SUBTITLE,
  L_LANGUAGE,
  L_NO_FILE,
  L_CHOOSE,
  L_REBOOTING,
  L_RECONNECTING,
  L_RESET_DONE,
  L_UPDATING,
  L_UPDATING_SUB,
  L_CANCEL,
  L_CONFIRM,
  L_COUNT
};

const char* STR_EN[L_COUNT] = {
  "Status", "Settings", "Firmware", "Free RAM", "Uptime", "Boot #", "Reset", "prev", "NTP sync",
  "System information", "Auto by light sensor", "Brightness", "Reboot", "Reboot device?",
  "Reset WiFi Settings", "Reset WiFi settings?", "never", "sec ago", "Please log in first",
  "Password", "Login", "Wrong password", "Current",
  "Firmware update (.bin)", "Upload", "Upload new firmware? Device will reboot.",
  "Update successful, rebooting...", "Update failed",
  "online", "Brightness adjusts itself", "Language", "No file chosen", "choose",
  "Rebooting...", "Waiting for the device to come back online...",
  "WiFi settings cleared. Reconnect to the mHome setup network to configure again.",
  "Uploading firmware...", "Please don't close this page",
  "Cancel", "Confirm"
};

const char* STR_RU[L_COUNT] = {
  "Статус", "Настройки", "Прошивка", "Свободно RAM", "Время работы", "Загрузка #", "Сброс", "пред.", "Синхр. NTP",
  "Системная информация", "Авто по датчику света", "Яркость", "Перезагрузка", "Перезагрузить устройство?",
  "Сброс настроек WiFi", "Сбросить настройки WiFi?", "никогда", "сек назад", "Сначала войдите",
  "Пароль", "Войти", "Неверный пароль", "Текущая",
  "Обновление прошивки (.bin)", "Загрузить", "Загрузить новую прошивку? Устройство перезагрузится.",
  "Обновление успешно, перезагрузка...", "Ошибка обновления",
  "на связи", "Яркость подстраивается сама", "Язык", "Файл не выбран", "выбрать",
  "Перезагрузка...", "Ждём, пока устройство снова будет доступно...",
  "Настройки WiFi сброшены. Подключитесь к сети настройки mHome, чтобы настроить заново.",
  "Загрузка прошивки...", "Не закрывайте страницу",
  "Отмена", "Подтвердить"
};

String t(LangKey key) {
  return String(lang == LANG_RU ? STR_RU[key] : STR_EN[key]);
}

String jsStringArray(const char* const* arr) {
  String out = "[";
  for (int i = 0; i < L_COUNT; i++) {
    if (i > 0) out += ",";
    out += "\"";
    String s = arr[i];
    s.replace("\"", "\\\"");
    out += s;
    out += "\"";
  }
  out += "]";
  return out;
}

MD_Parola display = MD_Parola(HARDWARE_TYPE, CS_PIN, MAX_DEVICES);

bool bigFontActive = false;

void useBigFont(bool big) {
  if (bigFontActive == big) return;
  display.setFont(big ? bigFont : nullptr);
  bigFontActive = big;
}

struct RtcData {
  uint32_t magic;
  uint32_t bootCount;
  uint8_t lastResetReason;
};

uint32_t bootCount = 1;
String resetReasonNow = "";
String resetReasonPrev = "";

String resetReasonName(uint8_t code) {
  switch (code) {
    case REASON_DEFAULT_RST: return "Power on";
    case REASON_WDT_RST: return "Hardware watchdog";
    case REASON_EXCEPTION_RST: return "Exception";
    case REASON_SOFT_WDT_RST: return "Software watchdog";
    case REASON_SOFT_RESTART: return "Soft restart";
    case REASON_DEEP_SLEEP_AWAKE: return "Deep-sleep wake";
    case REASON_EXT_SYS_RST: return "External reset";
    default: return "Unknown";
  }
}

void initBootDiagnostics() {
  RtcData rtc;
  bool haveRtc = ESP.rtcUserMemoryRead(0, (uint32_t*)&rtc, sizeof(rtc));
  uint8_t currentReason = ESP.getResetInfoPtr()->reason;

  if (haveRtc && rtc.magic == RTC_MAGIC) {
    bootCount = rtc.bootCount + 1;
    resetReasonPrev = resetReasonName(rtc.lastResetReason);
  } else {
    bootCount = 1;
    resetReasonPrev = "(unknown)";
  }
  resetReasonNow = resetReasonName(currentReason);

  rtc.magic = RTC_MAGIC;
  rtc.bootCount = bootCount;
  rtc.lastResetReason = currentReason;
  ESP.rtcUserMemoryWrite(0, (uint32_t*)&rtc, sizeof(rtc));
}

uint32_t wifiReconnectCount = 0;
bool wifiFirstGotIp = true;
WiFiEventHandler gotIpHandler;

String wifiStatusText() {
  switch (WiFi.status()) {
    case WL_CONNECTED: return "CONNECTED";
    case WL_NO_SSID_AVAIL: return "NO_SSID_AVAIL";
    case WL_CONNECT_FAILED: return "CONNECT_FAILED";
    case WL_CONNECTION_LOST: return "CONNECTION_LOST";
    case WL_DISCONNECTED: return "DISCONNECTED";
    case WL_IDLE_STATUS: return "IDLE";
    default: return "UNKNOWN";
  }
}

String formatUptime() {
  unsigned long s = millis() / 1000;
  unsigned int h = s / 3600;
  unsigned int m = (s % 3600) / 60;
  unsigned int sec = s % 60;
  char buf[16];
  snprintf(buf, sizeof(buf), "%02u:%02u:%02u", h, m, sec);
  return String(buf);
}

String formatTime() {
  time_t now;
  time(&now);
  struct tm *timeinfo = localtime(&now);
  char buf[9];
  snprintf(buf, sizeof(buf), "%02d:%02d", timeinfo->tm_hour, timeinfo->tm_min);
  return String(buf);
}

WiFiUDP ntpUDP;

const int NTP_PACKET_SIZE = 48;
byte ntpBuffer[NTP_PACKET_SIZE];

unsigned long lastNtpUpdate = 0;
const unsigned long NTP_INTERVAL = 3600000;
bool timeSynced = false;

ESP8266WebServer server(80);
DNSServer dnsServer;
bool apMode = false;
String adminPassword = "";
String sessionToken = "";
bool autoBrightness = true;
uint8_t manualBrightness = 8;
uint8_t currentBrightness = 8;

bool isLoggedIn() {
  if (adminPassword.length() == 0) return true;
  if (sessionToken.length() == 0) return false;
  String cookie = server.header("Cookie");
  return cookie.indexOf("session=" + sessionToken) != -1;
}

String sharedHead() {
  return R"rawliteral(<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<link rel="preconnect" href="https://fonts.googleapis.com">
<link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
<link href="https://fonts.googleapis.com/css2?family=Archivo:wght@300;400;500;600&display=swap" rel="stylesheet">
<style>
:root{--ease:cubic-bezier(.2,.8,.2,1);--color-text:#201e1d;--color-accent:#ec3013;--color-accent-700:#b8280f;--color-neutral-300:#e4e1de;--color-neutral-400:#cfcbc7;--color-neutral-700:#7a7672;--color-neutral-800:#55514d}
html,body{margin:0;padding:0}
body{background:#fbfafa;font-family:'Archivo',sans-serif;color:var(--color-text)}
*{box-sizing:border-box}
a{color:var(--color-accent-700)}
a:hover{color:var(--color-accent)}
.mh-wrap{min-height:100vh;background:linear-gradient(170deg,#ffffff 0%,#fbfafa 40%,#f6f4f3 100%);display:flex;justify-content:center;padding:48px 20px 64px}
.mh-card{width:100%;max-width:440px}
.mh-header{display:flex;align-items:center;justify-content:space-between;gap:16px}
.mh-title{font-weight:500;font-size:19px;letter-spacing:-0.01em}
.mh-online{display:flex;align-items:center;gap:7px;font-size:12.5px;color:var(--color-neutral-800)}
.mh-dot{width:5px;height:5px;border-radius:50%;background:#2ea043;display:inline-block}
.mh-overlay{display:none;position:fixed;inset:0;background:rgba(32,30,29,.55);align-items:center;justify-content:center;z-index:100;padding:20px}
.mh-overlay.show{display:flex}
.mh-overlay-card{background:#fff;border-radius:6px;padding:32px 28px;max-width:320px;text-align:center;box-shadow:0 20px 60px rgba(0,0,0,.25)}
.mh-spinner{width:32px;height:32px;border:3px solid var(--color-neutral-300);border-top-color:var(--color-accent);border-radius:50%;margin:0 auto 16px;animation:mh-spin .8s linear infinite}
@keyframes mh-spin{to{transform:rotate(360deg)}}
.mh-overlay-title{font-size:16px;font-weight:500;margin-bottom:6px}
.mh-overlay-sub{font-size:13px;color:var(--color-neutral-700)}
.mh-confirm-msg{font-size:14.5px;line-height:1.4;margin-bottom:20px}
.mh-confirm-actions{display:flex;gap:8px}
.mh-confirm-actions button{flex:1;padding:11px 14px;border-radius:3px;font-family:inherit;font-size:14px;cursor:pointer;transition:filter .3s var(--ease),transform .3s var(--ease)}
.mh-confirm-actions button:hover{transform:translateY(-1px)}
.mh-confirm-cancel{background:linear-gradient(120deg,#ffffff,#f4f2f1);border:1px solid rgba(32,30,29,.24);color:var(--color-text)}
.mh-confirm-ok{background:linear-gradient(120deg,var(--color-accent),var(--color-accent-700));border:0;color:#fff}
.mh-confirm-ok:hover{filter:brightness(1.1)}
.mh-footer{margin-top:36px;padding-top:14px;border-top:1px solid rgba(32,30,29,.18);font-size:12px;color:var(--color-neutral-700);display:flex;justify-content:space-between;gap:12px}
.mh-field{width:100%;padding:12px 14px;margin:8px 0;border:1px solid rgba(32,30,29,.24);border-radius:3px;background:linear-gradient(120deg,#ffffff,#f6f4f3);color:var(--color-text);font-family:inherit;font-size:15px}
.mh-field::placeholder{color:var(--color-neutral-700)}
.mh-label{font-size:13px;color:var(--color-neutral-800);display:block;margin-top:14px}
.mh-btn{width:100%;padding:13px 14px;margin-top:16px;background:linear-gradient(120deg,var(--color-neutral-800),var(--color-text));border:0;border-radius:3px;font-family:inherit;font-size:15px;color:#fff;cursor:pointer;transition:filter .3s var(--ease),transform .3s var(--ease)}
.mh-btn:hover{filter:brightness(1.3);transform:translateY(-1px)}
.mh-err{color:var(--color-accent-700);text-align:center;font-size:13.5px;margin-top:12px}
.mh-tabs{display:flex;gap:20px;margin-top:26px}
.mh-tab{padding:0 0 8px;background:none;border:0;border-bottom:2px solid transparent;font-family:inherit;font-weight:500;font-size:14px;text-align:left;cursor:pointer;color:var(--color-neutral-700);transition:color .35s var(--ease),border-color .35s var(--ease)}
.mh-tab.active{color:var(--color-text);border-bottom-color:var(--color-text)}
.mh-tabpanel{display:none}
.mh-tabpanel.active{display:block}
.mh-clock{padding:30px 0 4px}
.mh-clock-num{font-size:56px;line-height:1;font-weight:400;letter-spacing:-0.04em;font-variant-numeric:tabular-nums}
.mh-metarow{display:flex;flex-wrap:wrap;gap:6px 22px;margin-top:14px;font-size:13px;color:var(--color-neutral-800)}
.mh-metarow .sep{color:var(--color-neutral-400)}
.mh-sysinfo-toggle{width:100%;display:flex;align-items:center;gap:8px;margin-top:28px;padding:0 0 10px;background:none;border:0;font-family:inherit;font-size:13.5px;color:var(--color-neutral-700);cursor:pointer;text-align:left;transition:color .35s var(--ease)}
.mh-sysinfo-toggle:hover{color:var(--color-text)}
.mh-caret{font-size:12px;line-height:1;display:inline-block;color:var(--color-neutral-700);transition:transform .45s var(--ease)}
.mh-caret.open{transform:rotate(180deg)}
.mh-sysinfo-wrap{overflow:hidden;max-height:0;opacity:0;transition:max-height .55s var(--ease),opacity .4s var(--ease)}
.mh-sysinfo-wrap.open{max-height:900px;opacity:1}
.mh-row{display:flex;align-items:baseline;justify-content:space-between;gap:20px;padding:9px 4px 9px 0;border-top:1px solid rgba(32,30,29,.16);font-size:13.5px;transition:background .4s var(--ease)}
.mh-row:hover{background:linear-gradient(90deg,rgba(236,48,19,.05),transparent 70%)}
.mh-row .k{color:var(--color-neutral-800);white-space:nowrap}
.mh-row .v{text-align:right;font-variant-numeric:tabular-nums}
.mh-settings{padding-top:28px}
.mh-switchrow{display:flex;align-items:center;justify-content:space-between;gap:20px}
.mh-switchrow .t1{font-size:15px}
.mh-switchrow .t2{margin-top:5px;font-size:12.5px;color:var(--color-neutral-800)}
.mh-switch{width:46px;height:24px;flex:0 0 auto;padding:2px;border-radius:999px;border:1px solid rgba(32,30,29,.2);background:linear-gradient(120deg,#fff,#efecec);display:flex;cursor:pointer;transition:background .45s var(--ease),border-color .45s var(--ease)}
.mh-switch.on{border-color:transparent;background:linear-gradient(120deg,var(--color-neutral-700),var(--color-text))}
.mh-knob{width:18px;height:18px;border-radius:999px;background:var(--color-neutral-700);transition:transform .45s var(--ease),background .45s var(--ease)}
.mh-switch.on .mh-knob{background:#fff;transform:translateX(22px)}
.mh-brightness{margin-top:24px;transition:opacity .4s var(--ease)}
.mh-brightness.dim{opacity:.45}
.mh-brightness-row{display:flex;align-items:baseline;justify-content:space-between;gap:16px;font-size:13.5px}
.mh-brightness-row .lbl{color:var(--color-neutral-800)}
.mh-brightness-row .val{font-variant-numeric:tabular-nums}
.mh-range{-webkit-appearance:none;appearance:none;background:transparent;width:100%;margin-top:14px;height:16px;cursor:pointer}
.mh-range::-webkit-slider-runnable-track{height:3px;background:linear-gradient(90deg,var(--color-neutral-400),var(--color-neutral-300))}
.mh-range::-webkit-slider-thumb{-webkit-appearance:none;height:16px;width:16px;margin-top:-7px;background:#fff;border:1px solid var(--color-neutral-700);transition:transform .35s var(--ease),border-color .35s var(--ease)}
.mh-range:hover::-webkit-slider-thumb{transform:scale(1.15);border-color:var(--color-text)}
.mh-range::-moz-range-track{height:3px;background:var(--color-neutral-300)}
.mh-range::-moz-range-thumb{height:16px;width:16px;border-radius:0;background:#fff;border:1px solid var(--color-neutral-700)}
.mh-range:disabled{cursor:not-allowed}
.mh-current{margin-top:8px;font-size:12px;color:var(--color-neutral-700);font-variant-numeric:tabular-nums}
.mh-langrow{display:flex;align-items:center;justify-content:space-between;gap:20px;margin-top:26px;padding-top:18px;border-top:1px solid rgba(32,30,29,.18)}
.mh-langrow .lbl{font-size:13.5px;color:var(--color-neutral-800)}
.mh-seg{position:relative;display:grid;grid-auto-flow:column;grid-auto-columns:1fr;border:1px solid rgba(32,30,29,.24);background:linear-gradient(120deg,#ffffff,#f4f2f1);min-width:190px;border-radius:3px}
.mh-seg-ind{position:absolute;top:3px;bottom:3px;left:3px;width:calc(50% - 3px);background:linear-gradient(120deg,var(--color-neutral-800),var(--color-text));border-radius:2px;transition:transform .45s var(--ease);pointer-events:none}
.mh-seg-ind.en{transform:translateX(100%)}
.mh-seg button{position:relative;z-index:1;padding:9px 13px;background:none;border:0;font-family:inherit;font-size:13.5px;font-weight:400;cursor:pointer;color:var(--color-neutral-800);transition:color .4s var(--ease),font-weight .4s}
.mh-seg button.active{color:#fff;font-weight:500}
.mh-grid2{display:grid;grid-template-columns:1fr 1fr;gap:8px;margin-top:22px}
.mh-card-btn{display:flex;align-items:center;justify-content:space-between;gap:12px;padding:12px 14px;background:linear-gradient(120deg,#ffffff,#f4f2f1);border:1px solid rgba(32,30,29,.24);border-radius:3px;font-family:inherit;font-size:14px;color:var(--color-text);cursor:pointer;transition:background .4s var(--ease),border-color .4s var(--ease),transform .4s var(--ease);width:100%}
.mh-card-btn:hover{background:linear-gradient(120deg,#ffffff,#eeebea);border-color:var(--color-text);transform:translateY(-1px)}
.mh-card-btn.danger{background:linear-gradient(120deg,#ffffff,rgba(236,48,19,.07));border-color:rgba(236,48,19,.22);color:var(--color-accent-700)}
.mh-card-btn.danger:hover{background:linear-gradient(120deg,rgba(236,48,19,.06),rgba(236,48,19,.14));border-color:var(--color-accent)}
.mh-update{margin-top:26px;padding-top:18px;border-top:1px solid rgba(32,30,29,.18)}
.mh-update .lbl{font-size:13.5px;color:var(--color-neutral-800)}
.mh-filepick{display:flex;align-items:center;justify-content:space-between;gap:16px;margin-top:12px;padding:13px 14px;border:1px solid rgba(32,30,29,.24);border-radius:3px;background:linear-gradient(120deg,#ffffff,#f6f4f3);cursor:pointer;transition:border-color .4s var(--ease),background .4s var(--ease)}
.mh-filepick:hover{border-color:var(--color-text);background:linear-gradient(120deg,#ffffff,#f0edec)}
.mh-filepick .name{font-size:14px;color:var(--color-neutral-700)}
.mh-filepick .choose{font-size:12.5px;color:var(--color-neutral-800)}
.mh-upload-btn{width:100%;margin-top:8px;display:flex;align-items:center;justify-content:space-between;padding:12px 14px;background:linear-gradient(120deg,var(--color-neutral-800),var(--color-text));border:0;border-radius:3px;font-family:inherit;font-size:14px;color:#fff;cursor:pointer;transition:filter .4s var(--ease),transform .4s var(--ease)}
.mh-upload-btn:hover{filter:brightness(1.3);transform:translateY(-1px)}
</style>)rawliteral";
}

String loginPage(const String& error) {
  String html = R"rawliteral(<!DOCTYPE html>
<html><head><title>mHome Login</title>
)rawliteral" + sharedHead() + R"rawliteral(
</head><body>
<div class="mh-wrap"><div class="mh-card" style="max-width:360px">
<div class="mh-header"><div class="mh-title">mHome</div></div>
<form action="/login" method="POST" style="margin-top:26px">
<input class="mh-field" name="password" type="password" placeholder=")rawliteral" + t(L_PASSWORD_PLACEHOLDER) + R"rawliteral(" autofocus>
<button type="submit" class="mh-btn">)rawliteral" + t(L_LOGIN_BTN) + R"rawliteral(</button>
</form>
)rawliteral";
  if (error.length() > 0) {
    html += "<p class=\"mh-err\">" + error + "</p>";
  }
  html += R"rawliteral(</div></div></body></html>)rawliteral";
  return html;
}

void handleLogin() {
  String pass = server.hasArg("password") ? server.arg("password") : "";
  if (pass.length() > 0 && pass == adminPassword) {
    sessionToken = String((uint32_t)random(0, 0x7FFFFFFF), HEX) + String(millis(), HEX);
    server.sendHeader("Set-Cookie", "session=" + sessionToken + "; HttpOnly");
    server.sendHeader("Location", "/");
    server.send(302);
  } else {
    server.send(401, "text/html", loginPage(t(L_WRONG_PASSWORD)));
  }
}

String htmlPage(const String& networks) {
  return R"rawliteral(<!DOCTYPE html>
<html><head><title>mHome WiFi Setup</title>
)rawliteral" + sharedHead() + R"rawliteral(
</head><body>
<div class="mh-wrap"><div class="mh-card">
<div class="mh-header"><div class="mh-title">mHome</div></div>
<p style="text-align:center;font-size:13px;color:var(--color-neutral-800)">WiFi Setup</p>
<form action="/save" method="POST">
<label class="mh-label">Network</label>
)rawliteral" + networks + R"rawliteral(
<a id="scanBtn" href="#" onclick="this.href='http://'+location.hostname+'/'" class="mh-card-btn" style="justify-content:center;margin-top:8px;text-decoration:none">Scan networks</a>
<input class="mh-field" name="ssid" id="ssid" placeholder="SSID" maxlength="31">
<label class="mh-label">Password</label>
<input class="mh-field" name="pass" type="password" placeholder="Password" maxlength="63">
<label class="mh-label">Settings password</label>
<input class="mh-field" name="adminpass" type="password" placeholder="Password to access settings" maxlength="31" minlength="4" required>
<label class="mh-label">Interface language</label>
<select class="mh-field" name="lang">
<option value="en" selected>English</option>
<option value="ru">Русский</option>
</select>
<button type="submit" class="mh-btn">Save &amp; Restart</button>
</form>
</div></div></body></html>)rawliteral";
}

String scanNetworksSelect() {
  int n = WiFi.scanNetworks();
  String html = "<select class=\"mh-field\" onchange=\"document.getElementById('ssid').value=this.value\">";
  html += "<option value=\"\">-- Select network --</option>";
  if (n == 0) {
    html += "<option value=\"\" disabled>No networks found</option>";
  } else {
    for (int i = 0; i < n; i++) {
      html += "<option value=\"" + WiFi.SSID(i) + "\">" + WiFi.SSID(i) + " (" + String(WiFi.RSSI(i)) + " dBm)</option>";
    }
  }
  html += "</select>";
  return html;
}

void handleRoot() {
  Serial.println("[WEB] GET / (captive portal)");
  String nets = scanNetworksSelect();
  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server.sendHeader("Connection", "close");
  server.send(200, "text/html", htmlPage(nets));
}

void handleSave() {
  String ssid = server.hasArg("ssid") ? server.arg("ssid") : "";
  String pass = server.hasArg("pass") ? server.arg("pass") : "";
  String adminPass = server.hasArg("adminpass") ? server.arg("adminpass") : "";
  Serial.println("[WEB] POST /save ssid=" + ssid + " pass=" + (pass.length() > 0 ? "****" : "(empty)"));

  if (ssid.length() == 0) {
    server.send(400, "text/html", "<h1>SSID is required</h1><br><a href='/'>Back</a>");
    return;
  }
  if (adminPass.length() < 4) {
    server.send(400, "text/html", "<h1>Settings password must be at least 4 characters</h1><br><a href='/'>Back</a>");
    return;
  }

  EEPROM.write(EEPROM_MAGIC_ADDR, EEPROM_MAGIC);
  for (int i = 0; i < EEPROM_SSID_LEN; i++) {
    EEPROM.write(EEPROM_SSID_ADDR + i, i < (int)ssid.length() ? ssid[i] : 0);
  }
  for (int i = 0; i < EEPROM_PASS_LEN; i++) {
    EEPROM.write(EEPROM_PASS_ADDR + i, i < (int)pass.length() ? pass[i] : 0);
  }
  for (int i = 0; i < EEPROM_ADMIN_LEN; i++) {
    EEPROM.write(EEPROM_ADMIN_ADDR + i, i < (int)adminPass.length() ? adminPass[i] : 0);
  }
  EEPROM.write(EEPROM_ADMIN_MAGIC_ADDR, EEPROM_ADMIN_MAGIC);

  uint8_t selectedLang = (server.hasArg("lang") && server.arg("lang") == "ru") ? LANG_RU : LANG_EN;
  EEPROM.write(EEPROM_LANG_ADDR, selectedLang);
  EEPROM.write(EEPROM_LANG_MAGIC_ADDR, EEPROM_LANG_MAGIC);
  EEPROM.commit();

  server.send(200, "text/html", "<h1>Saved! Restarting...</h1>");
  delay(1000);
  ESP.restart();
}

void sendChunk(const String& s) {
  // An empty chunk ("0\r\n\r\n" in HTTP chunked encoding) means "end of
  // response" - the browser stops reading right there. Several dynamic
  // fragments here (conditional CSS classes, etc.) legitimately evaluate
  // to "" depending on state, so skip the actual write for those instead
  // of accidentally terminating the response early.
  if (s.length() == 0) return;
  server.sendContent(s);
  yield();
}

void handleSettings() {
  if (!isLoggedIn()) {
    server.send(200, "text/html", loginPage(""));
    return;
  }
  Serial.println("[WEB] GET / (settings)");

  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "text/html", "");

  sendChunk(R"rawliteral(<!DOCTYPE html>
<html><head><title>mHome Settings</title>
)rawliteral");
  sendChunk(sharedHead());
  sendChunk(R"rawliteral(
</head><body>
<div class="mh-wrap"><div class="mh-card">

<div class="mh-header">
<div class="mh-title">mHome</div>
<div class="mh-online"><span class="mh-dot"></span><span data-i18n=")rawliteral" + String((int)L_ONLINE) + R"rawliteral(">)rawliteral");
  sendChunk(t(L_ONLINE));
  sendChunk(R"rawliteral(</span></div>
</div>

<div class="mh-tabs">
<button type="button" class="mh-tab active" id="tabBtnStatus" onclick="showTab('status')"><span data-i18n=")rawliteral" + String((int)L_TAB_STATUS) + R"rawliteral(">)rawliteral");
  sendChunk(t(L_TAB_STATUS));
  sendChunk(R"rawliteral(</span></button>
<button type="button" class="mh-tab" id="tabBtnSettings" onclick="showTab('settings')"><span data-i18n=")rawliteral" + String((int)L_TAB_SETTINGS) + R"rawliteral(">)rawliteral");
  sendChunk(t(L_TAB_SETTINGS));
  sendChunk(R"rawliteral(</span></button>
</div>

<div id="tab-status" class="mh-tabpanel active">
<div class="mh-clock">
<div class="mh-clock-num" id="clockTime">)rawliteral");
  sendChunk(timeSynced ? formatTime() : String("--:--"));
  sendChunk(R"rawliteral(</div>
<div class="mh-metarow">
<span><span data-i18n=")rawliteral" + String((int)L_UPTIME) + R"rawliteral(">)rawliteral");
  sendChunk(t(L_UPTIME));
  sendChunk(R"rawliteral(</span> <span id="uptimeVal">)rawliteral");
  sendChunk(formatUptime());
  sendChunk(R"rawliteral(</span></span>
<span class="sep">&middot;</span>
<span>NTP <span id="ntpVal">)rawliteral");
  sendChunk(timeSynced ? (String((millis() - lastNtpUpdate) / 1000) + " " + t(L_SEC_AGO)) : t(L_NEVER));
  sendChunk(R"rawliteral(</span></span>
</div>
</div>

<button type="button" class="mh-sysinfo-toggle" id="sysinfoBtn" onclick="toggleInfo()">
<span data-i18n=")rawliteral" + String((int)L_SYSINFO) + R"rawliteral(">)rawliteral");
  sendChunk(t(L_SYSINFO));
  sendChunk(R"rawliteral(</span>
<span class="mh-caret" id="sysinfoCaret">&darr;</span>
</button>

<div class="mh-sysinfo-wrap" id="sysinfoWrap">
)rawliteral");
  sendChunk("<div class=\"mh-row\"><span class=\"k\">SSID</span><span class=\"v\">" + WiFi.SSID() + "</span></div>");
  sendChunk("<div class=\"mh-row\"><span class=\"k\" data-i18n=\"" + String((int)L_FIRMWARE) + "\">" + t(L_FIRMWARE) + "</span><span class=\"v\">" FW_VERSION " #" STRINGIFY(BUILD_NUMBER) " by " BUILD_TAG " (" __DATE__ " " __TIME__ ")</span></div>");
  sendChunk("<div class=\"mh-row\"><span class=\"k\">IP</span><span class=\"v\">" + WiFi.localIP().toString() + "</span></div>");
  sendChunk("<div class=\"mh-row\"><span class=\"k\">mDNS</span><span class=\"v\"><a href=\"http://" MDNS_HOSTNAME ".local\">" MDNS_HOSTNAME ".local</a></span></div>");
  sendChunk("<div class=\"mh-row\"><span class=\"k\">MAC</span><span class=\"v\">" + WiFi.macAddress() + "</span></div>");
  sendChunk("<div class=\"mh-row\"><span class=\"k\">RSSI</span><span class=\"v\"><span id=\"rssiVal\">" + String(WiFi.RSSI()) + "</span> dBm</span></div>");
  sendChunk("<div class=\"mh-row\"><span class=\"k\">WiFi FSM</span><span class=\"v\"><span id=\"fsmVal\">" + wifiStatusText() + " (rec=" + String(wifiReconnectCount) + ")</span></span></div>");
  sendChunk("<div class=\"mh-row\"><span class=\"k\" data-i18n=\"" + String((int)L_FREE_RAM) + "\">" + t(L_FREE_RAM) + "</span><span class=\"v\"><span id=\"ramVal\">" + String(ESP.getFreeHeap() / 1024) + "</span> KB</span></div>");
  sendChunk("<div class=\"mh-row\"><span class=\"k\">Flash</span><span class=\"v\">" + String(ESP.getSketchSize() / 1024) + " / " + String(ESP.getFlashChipRealSize() / 1024) + " KB</span></div>");
  sendChunk("<div class=\"mh-row\"><span class=\"k\" data-i18n=\"" + String((int)L_BOOT) + "\">" + t(L_BOOT) + "</span><span class=\"v\">" + String(bootCount) + "</span></div>");
  sendChunk("<div class=\"mh-row\"><span class=\"k\" data-i18n=\"" + String((int)L_RESET_LABEL) + "\">" + t(L_RESET_LABEL) + "</span><span class=\"v\">" + resetReasonNow + " / <span data-i18n=\"" + String((int)L_PREV) + "\">" + t(L_PREV) + "</span>: " + resetReasonPrev + "</span></div>");
  sendChunk(R"rawliteral(</div>
</div>

<div id="tab-settings" class="mh-tabpanel mh-settings">
<div class="mh-switchrow">
<div>
<div class="t1" data-i18n=")rawliteral" + String((int)L_AUTO_SENSOR) + R"rawliteral(">)rawliteral");
  sendChunk(t(L_AUTO_SENSOR));
  sendChunk(R"rawliteral(</div>
<div class="t2" data-i18n=")rawliteral" + String((int)L_AUTO_SUBTITLE) + R"rawliteral(">)rawliteral");
  sendChunk(t(L_AUTO_SUBTITLE));
  sendChunk(R"rawliteral(</div>
</div>
<button type="button" class="mh-switch)rawliteral");
  sendChunk(autoBrightness ? " on" : "");
  sendChunk(R"rawliteral(" id="autoSwitch" onclick="toggleAuto()"><span class="mh-knob"></span></button>
</div>

<div class="mh-brightness)rawliteral");
  sendChunk(autoBrightness ? " dim" : "");
  sendChunk(R"rawliteral(" id="brightnessBlock">
<div class="mh-brightness-row">
<span class="lbl" data-i18n=")rawliteral" + String((int)L_BRIGHTNESS) + R"rawliteral(">)rawliteral");
  sendChunk(t(L_BRIGHTNESS));
  sendChunk(R"rawliteral(</span>
<span class="val"><span id="bval">)rawliteral");
  sendChunk(String(manualBrightness));
  sendChunk(R"rawliteral(</span>/15</span>
</div>
<input class="mh-range" type="range" id="brange" min="0" max="15" step="1" value=")rawliteral");
  sendChunk(String(manualBrightness));
  sendChunk(R"rawliteral(" oninput="document.getElementById('bval').textContent=this.value" onchange="saveBrightness()" )rawliteral");
  sendChunk(autoBrightness ? "disabled" : "");
  sendChunk(R"rawliteral(>
<div class="mh-current"><span data-i18n=")rawliteral" + String((int)L_CURRENT) + R"rawliteral(">)rawliteral");
  sendChunk(t(L_CURRENT));
  sendChunk(R"rawliteral(</span> <span id="curBrightness">)rawliteral");
  sendChunk(String(currentBrightness));
  sendChunk(R"rawliteral(</span>/15</div>
</div>

<div class="mh-langrow">
<span class="lbl" data-i18n=")rawliteral" + String((int)L_LANGUAGE) + R"rawliteral(">)rawliteral");
  sendChunk(t(L_LANGUAGE));
  sendChunk(R"rawliteral(</span>
<div class="mh-seg">
<span class="mh-seg-ind)rawliteral");
  sendChunk((lang == LANG_EN) ? " en" : "");
  sendChunk(R"rawliteral(" id="langInd"></span>
<button type="button" class="lang-ru)rawliteral");
  sendChunk((lang == LANG_RU) ? " active" : "");
  sendChunk(R"rawliteral(" onclick="pickLang('ru')">Русский</button>
<button type="button" class="lang-en)rawliteral");
  sendChunk((lang == LANG_EN) ? " active" : "");
  sendChunk(R"rawliteral(" onclick="pickLang('en')">English</button>
</div>
</div>

<div class="mh-grid2">
<form id="rebootForm" action="/reboot" method="POST" data-confirm=")rawliteral" + String((int)L_REBOOT_CONFIRM) + R"rawliteral(" data-mode="reboot">
<button type="submit" class="mh-card-btn"><span data-i18n=")rawliteral" + String((int)L_REBOOT) + R"rawliteral(">)rawliteral");
  sendChunk(t(L_REBOOT));
  sendChunk(R"rawliteral(</span><span>&#8635;</span></button>
</form>
<form id="resetForm" action="/reset" method="POST" data-confirm=")rawliteral" + String((int)L_RESET_WIFI_CONFIRM) + R"rawliteral(" data-mode="reset">
<button type="submit" class="mh-card-btn danger"><span data-i18n=")rawliteral" + String((int)L_RESET_WIFI) + R"rawliteral(">)rawliteral");
  sendChunk(t(L_RESET_WIFI));
  sendChunk(R"rawliteral(</span><span>&rarr;</span></button>
</form>
</div>

<div class="mh-update">
<div class="lbl" data-i18n=")rawliteral" + String((int)L_FIRMWARE_UPDATE) + R"rawliteral(">)rawliteral");
  sendChunk(t(L_FIRMWARE_UPDATE));
  sendChunk(R"rawliteral(</div>
<form id="updateForm" action="/update" method="POST" enctype="multipart/form-data" data-confirm=")rawliteral" + String((int)L_UPDATE_CONFIRM) + R"rawliteral(" data-mode="update">
<label class="mh-filepick">
<span class="name" id="fileName">)rawliteral");
  sendChunk(t(L_NO_FILE));
  sendChunk(R"rawliteral(</span>
<span class="choose" data-i18n=")rawliteral" + String((int)L_CHOOSE) + R"rawliteral(">)rawliteral");
  sendChunk(t(L_CHOOSE));
  sendChunk(R"rawliteral(</span>
<input type="file" id="fileInput" name="firmware" accept=".bin" required style="display:none" onchange="pickFile(this)">
</label>
<button type="submit" class="mh-upload-btn"><span data-i18n=")rawliteral" + String((int)L_UPLOAD) + R"rawliteral(">)rawliteral");
  sendChunk(t(L_UPLOAD));
  sendChunk(R"rawliteral(</span><span>&uarr;</span></button>
</form>
</div>
</div>

<div class="mh-footer"><span>mHome )rawliteral" FW_VERSION R"rawliteral(</span><span id="footerIp">)rawliteral");
  sendChunk(WiFi.localIP().toString());
  sendChunk(R"rawliteral(</span></div>

</div></div>

<div class="mh-overlay" id="rebootOverlay">
<div class="mh-overlay-card">
<div class="mh-spinner" id="rebootSpinner"></div>
<div class="mh-overlay-title" id="rebootTitle"></div>
<div class="mh-overlay-sub" id="rebootSub"></div>
</div>
</div>

<div class="mh-overlay" id="confirmOverlay">
<div class="mh-overlay-card">
<div class="mh-confirm-msg" id="confirmMsg"></div>
<div class="mh-confirm-actions">
<button type="button" class="mh-confirm-cancel" id="confirmCancelBtn" data-i18n=")rawliteral" + String((int)L_CANCEL) + R"rawliteral(">)rawliteral");
  sendChunk(t(L_CANCEL));
  sendChunk(R"rawliteral(</button>
<button type="button" class="mh-confirm-ok" id="confirmOkBtn" data-i18n=")rawliteral" + String((int)L_CONFIRM) + R"rawliteral(">)rawliteral");
  sendChunk(t(L_CONFIRM));
  sendChunk(R"rawliteral(</button>
</div>
</div>
</div>

<script>
var STR_EN=)rawliteral");
  sendChunk(jsStringArray(STR_EN));
  sendChunk(R"rawliteral(;
var STR_RU=)rawliteral");
  sendChunk(jsStringArray(STR_RU));
  sendChunk(R"rawliteral(;
var curLang = )rawliteral");
  sendChunk(String((int)lang));
  sendChunk(R"rawliteral(;
var NO_FILE_IDX = )rawliteral");
  sendChunk(String((int)L_NO_FILE));
  sendChunk(R"rawliteral(;
var REBOOTING_IDX = )rawliteral");
  sendChunk(String((int)L_REBOOTING));
  sendChunk(R"rawliteral(;
var RECONNECTING_IDX = )rawliteral");
  sendChunk(String((int)L_RECONNECTING));
  sendChunk(R"rawliteral(;
var RESET_DONE_IDX = )rawliteral");
  sendChunk(String((int)L_RESET_DONE));
  sendChunk(R"rawliteral(;
var UPDATING_IDX = )rawliteral");
  sendChunk(String((int)L_UPDATING));
  sendChunk(R"rawliteral(;
var UPDATING_SUB_IDX = )rawliteral");
  sendChunk(String((int)L_UPDATING_SUB));
  sendChunk(R"rawliteral(;
function T(idx) { return (curLang === 1 ? STR_RU : STR_EN)[idx]; }
function applyLang() {
  document.querySelectorAll('[data-i18n]').forEach(function(el) {
    el.textContent = T(parseInt(el.getAttribute('data-i18n'), 10));
  });
  var fi = document.getElementById('fileInput');
  var fn = document.getElementById('fileName');
  if (fi && fn && (!fi.files || !fi.files.length)) fn.textContent = T(NO_FILE_IDX);
  var ind = document.getElementById('langInd');
  if (ind) ind.classList.toggle('en', curLang === 0);
  var ru = document.querySelector('.lang-ru');
  var en = document.querySelector('.lang-en');
  if (ru) ru.classList.toggle('active', curLang === 1);
  if (en) en.classList.toggle('active', curLang === 0);
}
function showTab(id) {
  document.querySelectorAll('.mh-tabpanel').forEach(function(el) { el.classList.remove('active'); });
  document.querySelectorAll('.mh-tab').forEach(function(el) { el.classList.remove('active'); });
  document.getElementById('tab-' + id).classList.add('active');
  document.getElementById('tabBtn' + id.charAt(0).toUpperCase() + id.slice(1)).classList.add('active');
}
function toggleInfo() {
  var wrap = document.getElementById('sysinfoWrap');
  var caret = document.getElementById('sysinfoCaret');
  wrap.classList.toggle('open');
  caret.classList.toggle('open');
}
function toggleAuto() {
  var sw = document.getElementById('autoSwitch');
  var on = !sw.classList.contains('on');
  sw.classList.toggle('on', on);
  document.getElementById('brange').disabled = on;
  document.getElementById('brightnessBlock').classList.toggle('dim', on);
  saveBrightness();
}
function saveBrightness() {
  var auto = document.getElementById('autoSwitch').classList.contains('on');
  var val = document.getElementById('brange').value;
  var body = 'value=' + val + (auto ? '&auto=on' : '');
  fetch('/brightness', {method:'POST', headers:{'Content-Type':'application/x-www-form-urlencoded'}, body: body});
}
function pickLang(l) {
  curLang = (l === 'en') ? 0 : 1;
  applyLang();
  fetch('/language', {method:'POST', headers:{'Content-Type':'application/x-www-form-urlencoded'}, body: 'lang=' + l});
}
function pickFile(input) {
  document.getElementById('fileName').textContent = input.files && input.files[0] ? input.files[0].name : T(NO_FILE_IDX);
}
function showRebootOverlay(mode) {
  var ov = document.getElementById('rebootOverlay');
  var spinner = document.getElementById('rebootSpinner');
  var title = document.getElementById('rebootTitle');
  var sub = document.getElementById('rebootSub');
  if (mode === 'reset') {
    spinner.style.display = 'none';
    title.textContent = T(RESET_DONE_IDX);
    sub.textContent = '';
  } else if (mode === 'update') {
    spinner.style.display = '';
    title.textContent = T(UPDATING_IDX);
    sub.textContent = T(UPDATING_SUB_IDX);
  } else {
    spinner.style.display = '';
    title.textContent = T(REBOOTING_IDX);
    sub.textContent = T(RECONNECTING_IDX);
  }
  ov.classList.add('show');
}
function waitForDeviceThenRedirect() {
  fetch('/', {cache: 'no-store'}).then(function(r) {
    if (r.ok) { location.href = '/'; } else { setTimeout(waitForDeviceThenRedirect, 1500); }
  }).catch(function() { setTimeout(waitForDeviceThenRedirect, 1500); });
}
function showConfirm(idx, onConfirm) {
  var ov = document.getElementById('confirmOverlay');
  document.getElementById('confirmMsg').textContent = T(idx);
  var okBtn = document.getElementById('confirmOkBtn');
  var cancelBtn = document.getElementById('confirmCancelBtn');
  function cleanup() {
    ov.classList.remove('show');
    okBtn.removeEventListener('click', onOk);
    cancelBtn.removeEventListener('click', onCancel);
  }
  function onOk() { cleanup(); onConfirm(); }
  function onCancel() { cleanup(); }
  okBtn.addEventListener('click', onOk);
  cancelBtn.addEventListener('click', onCancel);
  ov.classList.add('show');
}
['rebootForm', 'resetForm', 'updateForm'].forEach(function(id) {
  var form = document.getElementById(id);
  if (!form) return;
  form.addEventListener('submit', function(e) {
    e.preventDefault();
    showConfirm(parseInt(form.dataset.confirm, 10), function() {
      var mode = form.dataset.mode;
      showRebootOverlay(mode);
      var opts = { method: 'POST' };
      if (form.enctype === 'multipart/form-data') {
        opts.body = new FormData(form);
      } else {
        opts.headers = { 'Content-Type': 'application/x-www-form-urlencoded' };
        opts.body = new URLSearchParams(new FormData(form)).toString();
      }
      var req = fetch(form.action, opts).catch(function() {});
      if (mode === 'update') {
        req.then(function() {
          showRebootOverlay('reboot');
          setTimeout(waitForDeviceThenRedirect, 1500);
        });
      } else if (mode !== 'reset') {
        setTimeout(waitForDeviceThenRedirect, 3000);
      }
    });
  });
});
function pollStatus() {
  fetch('/status').then(function(r) { return r.json(); }).then(function(d) {
    var el;
    if ((el = document.getElementById('clockTime'))) el.textContent = d.time;
    if ((el = document.getElementById('uptimeVal'))) el.textContent = d.uptime;
    if ((el = document.getElementById('ntpVal'))) el.textContent = d.ntp;
    if ((el = document.getElementById('rssiVal'))) el.textContent = d.rssi;
    if ((el = document.getElementById('fsmVal'))) el.textContent = d.wifiFsm;
    if ((el = document.getElementById('ramVal'))) el.textContent = d.freeRam;
    if ((el = document.getElementById('curBrightness'))) el.textContent = d.brightness;
  }).catch(function() {});
}
setInterval(pollStatus, 3000);
</script>
</body></html>)rawliteral");
  server.sendContent(""); // explicit end-of-chunked-response terminator
}

void handleReset() {
  if (!isLoggedIn()) {
    server.send(401, "text/html", loginPage(t(L_LOGIN_FIRST)));
    return;
  }
  Serial.println("[WEB] POST /reset - clearing EEPROM and restarting");
  for (int i = 0; i < EEPROM_SIZE; i++) {
    EEPROM.write(i, 0xFF);
  }
  EEPROM.commit();
  server.send(200, "text/html", "<h1>Reset! Restarting...</h1>");
  delay(1000);
  ESP.restart();
}

void handleBrightness() {
  if (!isLoggedIn()) {
    server.send(401, "text/html", loginPage(t(L_LOGIN_FIRST)));
    return;
  }
  autoBrightness = server.hasArg("auto");
  if (server.hasArg("value")) {
    manualBrightness = constrain(server.arg("value").toInt(), 0, 15);
  }
  EEPROM.write(EEPROM_BRIGHTNESS_MODE_ADDR, autoBrightness ? 0 : 1);
  EEPROM.write(EEPROM_BRIGHTNESS_VALUE_ADDR, manualBrightness);
  EEPROM.write(EEPROM_BRIGHTNESS_MAGIC_ADDR, EEPROM_BRIGHTNESS_MAGIC);
  EEPROM.commit();
  if (!autoBrightness) display.setIntensity(manualBrightness);
  Serial.println("[WEB] POST /brightness auto=" + String(autoBrightness) + " value=" + String(manualBrightness));
  server.sendHeader("Location", "/");
  server.send(302);
}

void handleReboot() {
  if (!isLoggedIn()) {
    server.send(401, "text/html", loginPage(t(L_LOGIN_FIRST)));
    return;
  }
  Serial.println("[WEB] POST /reboot");
  server.send(200, "text/html", "<h1>Rebooting...</h1>");
  delay(500);
  ESP.restart();
}

void handleLanguage() {
  if (!isLoggedIn()) {
    server.send(401, "text/html", loginPage(t(L_LOGIN_FIRST)));
    return;
  }
  lang = (server.hasArg("lang") && server.arg("lang") == "ru") ? LANG_RU : LANG_EN;
  EEPROM.write(EEPROM_LANG_ADDR, lang);
  EEPROM.write(EEPROM_LANG_MAGIC_ADDR, EEPROM_LANG_MAGIC);
  EEPROM.commit();
  Serial.println("[WEB] POST /language lang=" + String(lang));
  server.sendHeader("Location", "/");
  server.send(302);
}

void handleStatus() {
  if (!isLoggedIn()) {
    server.send(401, "application/json", "{}");
    return;
  }
  String json = "{";
  json += "\"time\":\"" + (timeSynced ? formatTime() : String("--:--")) + "\",";
  json += "\"uptime\":\"" + formatUptime() + "\",";
  json += "\"ntp\":\"" + (timeSynced ? (String((millis() - lastNtpUpdate) / 1000) + " " + t(L_SEC_AGO)) : t(L_NEVER)) + "\",";
  json += "\"rssi\":" + String(WiFi.RSSI()) + ",";
  json += "\"wifiFsm\":\"" + wifiStatusText() + " (rec=" + String(wifiReconnectCount) + ")\",";
  json += "\"freeRam\":" + String(ESP.getFreeHeap() / 1024) + ",";
  json += "\"brightness\":" + String(currentBrightness);
  json += "}";
  server.send(200, "application/json", json);
}

bool updateUnauthorized = false;

void handleUpdateUpload() {
  HTTPUpload& upload = server.upload();
  if (upload.status == UPLOAD_FILE_START) {
    if (!isLoggedIn()) {
      updateUnauthorized = true;
      return;
    }
    updateUnauthorized = false;
    Serial.println("[WEB] Update start: " + upload.filename);
    uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
    if (!Update.begin(maxSketchSpace)) {
      Update.printError(Serial);
    }
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (updateUnauthorized) return;
    if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
      Update.printError(Serial);
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    if (updateUnauthorized) return;
    if (Update.end(true)) {
      Serial.println("[WEB] Update success: " + String(upload.totalSize) + " bytes");
    } else {
      Update.printError(Serial);
    }
  }
  yield();
}

void handleUpdateFinish() {
  if (!isLoggedIn() || updateUnauthorized) {
    updateUnauthorized = false;
    server.send(401, "text/html", loginPage(t(L_LOGIN_FIRST)));
    return;
  }
  server.sendHeader("Connection", "close");
  server.send(200, "text/html", Update.hasError() ? t(L_UPDATE_FAIL) : t(L_UPDATE_SUCCESS));
  delay(500);
  ESP.restart();
}

void setupSTAWebServer() {
  server.collectHeaders("Cookie");
  server.on("/", handleSettings);
  server.on("/login", HTTP_POST, handleLogin);
  server.on("/reset", HTTP_POST, handleReset);
  server.on("/brightness", HTTP_POST, handleBrightness);
  server.on("/reboot", HTTP_POST, handleReboot);
  server.on("/language", HTTP_POST, handleLanguage);
  server.on("/status", HTTP_GET, handleStatus);
  server.on("/update", HTTP_POST, handleUpdateFinish, handleUpdateUpload);
  server.begin();
  Serial.println("Web server started on " + WiFi.localIP().toString());

  if (MDNS.begin(MDNS_HOSTNAME)) {
    MDNS.addService("http", "tcp", 80);
    Serial.println("mDNS started: " MDNS_HOSTNAME ".local");
  } else {
    Serial.println("mDNS start failed");
  }
}

void setupCaptivePortal() {
  WiFi.mode(WIFI_AP_STA);
  WiFi.disconnect();

  String apSsid = "mHome_" + String(random(100, 999));
  WiFi.softAP(apSsid.c_str());

  dnsServer.start(53, "*", WiFi.softAPIP());

  server.on("/", handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  server.onNotFound(handleRoot);
  server.begin();

  apMode = true;
  Serial.println("AP started: " + apSsid);
  Serial.println("AP IP: " + WiFi.softAPIP().toString());
}

bool loadWiFiCredentials(String &ssid, String &pass) {
  if (EEPROM.read(EEPROM_MAGIC_ADDR) != EEPROM_MAGIC) {
    return false;
  }

  char sbuf[EEPROM_SSID_LEN + 1];
  for (int i = 0; i < EEPROM_SSID_LEN; i++) {
    sbuf[i] = EEPROM.read(EEPROM_SSID_ADDR + i);
  }
  sbuf[EEPROM_SSID_LEN] = 0;
  ssid = String(sbuf);
  if (ssid.length() == 0) return false;

  char pbuf[EEPROM_PASS_LEN + 1];
  for (int i = 0; i < EEPROM_PASS_LEN; i++) {
    pbuf[i] = EEPROM.read(EEPROM_PASS_ADDR + i);
  }
  pbuf[EEPROM_PASS_LEN] = 0;
  pass = String(pbuf);

  if (EEPROM.read(EEPROM_ADMIN_MAGIC_ADDR) == EEPROM_ADMIN_MAGIC) {
    char abuf[EEPROM_ADMIN_LEN + 1];
    for (int i = 0; i < EEPROM_ADMIN_LEN; i++) {
      abuf[i] = EEPROM.read(EEPROM_ADMIN_ADDR + i);
    }
    abuf[EEPROM_ADMIN_LEN] = 0;
    adminPassword = String(abuf);
  } else {
    adminPassword = "";
  }

  return true;
}

bool connectToWiFi(const String &ssid, const String &pass) {
  Serial.println("Connecting to: " + ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), pass.c_str());

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - start > 15000) {
      Serial.println("WiFi connection timeout");
      return false;
    }
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected! IP: " + WiFi.localIP().toString());
  return true;
}

void sendNTPPacket() {
  memset(ntpBuffer, 0, NTP_PACKET_SIZE);
  ntpBuffer[0] = 0b11100011;
  ntpBuffer[1] = 0;
  ntpBuffer[2] = 6;
  ntpBuffer[3] = 0xEC;
  ntpBuffer[12] = 49;
  ntpBuffer[13] = 0x4E;
  ntpBuffer[14] = 49;
  ntpBuffer[15] = 0x52;

  ntpUDP.beginPacket(NTP_SERVER, 123);
  ntpUDP.write(ntpBuffer, NTP_PACKET_SIZE);
  ntpUDP.endPacket();
}

time_t getNtpTime() {
  sendNTPPacket();

  unsigned long startMs = millis();
  while (millis() - startMs < 1000) {
    if (ntpUDP.parsePacket()) {
      ntpUDP.read(ntpBuffer, NTP_PACKET_SIZE);
      unsigned long highWord = word(ntpBuffer[40], ntpBuffer[41]);
      unsigned long lowWord = word(ntpBuffer[42], ntpBuffer[43]);
      unsigned long secsSince1900 = highWord << 16 | lowWord;
      const unsigned long seventyYears = 2208988800UL;
      unsigned long epoch = secsSince1900 - seventyYears;
      return (time_t)epoch;
    }
    delay(10);
  }
  return 0;
}

void syncTime() {
  ntpUDP.begin(2390);
  time_t epoch = getNtpTime();
  if (epoch > 0) {
    timeval tv = { (time_t)(epoch + TIMEZONE_OFFSET * 3600 + DST_OFFSET * 3600), 0 };
    settimeofday(&tv, nullptr);
    timeSynced = true;
    lastNtpUpdate = millis();
  }
}

#define LDR_PIN A0
#define LDR_INTERVAL 1000
#define LDR_MIN 0
#define LDR_MAX 1024

unsigned long lastLdrUpdate = 0;
float ldrSmoothed = -1;
const float LDR_EMA_ALPHA = 0.2;

void loadBrightnessSettings() {
  if (EEPROM.read(EEPROM_BRIGHTNESS_MAGIC_ADDR) == EEPROM_BRIGHTNESS_MAGIC) {
    autoBrightness = EEPROM.read(EEPROM_BRIGHTNESS_MODE_ADDR) == 0;
    manualBrightness = constrain((int)EEPROM.read(EEPROM_BRIGHTNESS_VALUE_ADDR), 0, 15);
  } else {
    autoBrightness = true;
    manualBrightness = 8;
  }
}

void loadLangSettings() {
  if (EEPROM.read(EEPROM_LANG_MAGIC_ADDR) == EEPROM_LANG_MAGIC) {
    lang = EEPROM.read(EEPROM_LANG_ADDR) == LANG_RU ? LANG_RU : LANG_EN;
  } else {
    lang = LANG_EN;
  }
}

void updateBrightness() {
  if (millis() - lastLdrUpdate < LDR_INTERVAL) return;
  lastLdrUpdate = millis();

  if (!autoBrightness) {
    currentBrightness = manualBrightness;
    display.setIntensity(manualBrightness);
    return;
  }

  int ldr = analogRead(LDR_PIN);
  ldrSmoothed = (ldrSmoothed < 0) ? ldr : (LDR_EMA_ALPHA * ldr + (1 - LDR_EMA_ALPHA) * ldrSmoothed);

  int b = map((int)ldrSmoothed, LDR_MIN, LDR_MAX, 0, 15);
  b = constrain(b, 0, 15);
  currentBrightness = b;
  display.setIntensity(b);
  Serial.println("[LDR] raw=" + String(ldr) + " smoothed=" + String(ldrSmoothed) + " brightness=" + String(b));
}

void setup() {
  Serial.begin(115200);
  initBootDiagnostics();
  WiFi.persistent(false);
  EEPROM.begin(EEPROM_SIZE);
  loadBrightnessSettings();
  loadLangSettings();
  randomSeed(analogRead(0));

  gotIpHandler = WiFi.onStationModeGotIP([](const WiFiEventStationModeGotIP&) {
    if (wifiFirstGotIp) {
      wifiFirstGotIp = false;
    } else {
      wifiReconnectCount++;
    }
  });

  display.begin();
  display.setIntensity(1);
  display.displayClear();
  display.displayShutdown(false);
  useBigFont(false);
  display.setTextAlignment(PA_CENTER);
  display.print("mHome");
  delay(1500);

  String ssid, pass;
  bool connected = false;

  if (loadWiFiCredentials(ssid, pass)) {
    connected = connectToWiFi(ssid, pass);
  }

  if (!connected) {
    setupCaptivePortal();
  } else {
    setupSTAWebServer();
    syncTime();
  }
}

String lastTimeStr = "";

void loop() {
  if (apMode) {
    dnsServer.processNextRequest();
  }
  server.handleClient();
  MDNS.update();
  if (apMode) {
    useBigFont(false);
    display.setTextAlignment(PA_CENTER);
    display.print("WiFi?");
    return;
  }

  if (!timeSynced) {
    useBigFont(true);
    display.print("--:--");
    syncTime();
    delay(5000);
    return;
  }

  if (millis() - lastNtpUpdate > NTP_INTERVAL) {
    syncTime();
  }

  updateBrightness();

  String timeStr = formatTime();
  if (timeStr != lastTimeStr) {
    lastTimeStr = timeStr;
    useBigFont(true);
    display.displayClear();
    display.setTextAlignment(PA_CENTER);
    display.print(timeStr);
  }

  delay(10);
}
