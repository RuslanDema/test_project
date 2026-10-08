#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <DNSServer.h>
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_ADS1X15.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <Preferences.h>

Preferences preferences;
float calibMult1 = 1.0, calibOffset1 = 0.0;
float calibMult2 = 1.0, calibOffset2 = 0.0;
float calibMult3 = 1.0, calibOffset3 = 0.0;
float raw_ma_1 = 0, raw_ma_2 = 0, raw_ma_3 = 0;
float calib_ma_1 = 0, calib_ma_2 = 0, calib_ma_3 = 0;
bool adcFound = false;

const char CALIB_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="ru">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Калибровка ПН-2026</title>
  <style>
    body { background: #090d16; color: #f8fafc; font-family: sans-serif; padding: 20px; }
    .card { background: #131a27; border: 1px solid #24334a; border-radius: 10px; padding: 15px; margin-bottom: 15px; }
    h2 { margin-top: 0; color: #00e5ff; }
    .channel { display: flex; justify-content: space-between; margin-bottom: 10px; border-bottom: 1px solid #24334a; padding-bottom: 10px; }
    input { width: 80px; padding: 5px; background: #090d16; color: white; border: 1px solid #24334a; border-radius: 5px; }
    button { background: #10b981; color: white; border: none; padding: 10px 20px; border-radius: 5px; cursor: pointer; font-weight: bold; width: 100%; margin-top: 10px; }
    button:hover { background: #059669; }
    .val { font-size: 20px; font-weight: bold; color: #ffb300; }
    .error { color: #ef4444; font-weight: bold; margin-bottom: 15px; display: none; }
  </style>
</head>
<body>
  <h2>ПН-2026 | Калибровка</h2>
  <div id="adcError" class="error">ОШИБКА: АЦП (ADS1115) не найден!</div>
  <div class="card"><h3>Канал 1</h3>
    <div class="channel"><div>Сырое: <span id="raw1" class="val">0.0</span></div> <div>Итог: <span id="cal1" class="val">0.0</span></div></div>
    <div>Множ: <input type="number" step="0.001" id="m1"> Сдвиг: <input type="number" step="0.01" id="o1"></div>
  </div>
  <div class="card"><h3>Канал 2</h3>
    <div class="channel"><div>Сырое: <span id="raw2" class="val">0.0</span></div> <div>Итог: <span id="cal2" class="val">0.0</span></div></div>
    <div>Множ: <input type="number" step="0.001" id="m2"> Сдвиг: <input type="number" step="0.01" id="o2"></div>
  </div>
  <div class="card"><h3>Канал 3</h3>
    <div class="channel"><div>Сырое: <span id="raw3" class="val">0.0</span></div> <div>Итог: <span id="cal3" class="val">0.0</span></div></div>
    <div>Множ: <input type="number" step="0.001" id="m3"> Сдвиг: <input type="number" step="0.01" id="o3"></div>
  </div>
  <button onclick="saveCalib()">СОХРАНИТЬ В ПАМЯТЬ</button>
  <button style="background:#24334a; margin-top:10px" onclick="location.href='/'">НАЗАД НА ГЛАВНУЮ</button>

  <script>
    let isEditing = false;
    document.querySelectorAll('input').forEach(el => { el.addEventListener('focus', () => isEditing = true); el.addEventListener('blur', () => isEditing = false); });
    setInterval(() => {
      fetch('/api/data').then(r => r.json()).then(d => {
        if (d.adc_error) document.getElementById('adcError').style.display = 'block';
        document.getElementById('raw1').innerText = d.r1.toFixed(2); document.getElementById('cal1').innerText = d.c1.toFixed(2);
        document.getElementById('raw2').innerText = d.r2.toFixed(2); document.getElementById('cal2').innerText = d.c2.toFixed(2);
        document.getElementById('raw3').innerText = d.r3.toFixed(2); document.getElementById('cal3').innerText = d.c3.toFixed(2);
        if(!isEditing) {
          document.getElementById('m1').value = d.m1; document.getElementById('o1').value = d.o1;
          document.getElementById('m2').value = d.m2; document.getElementById('o2').value = d.o2;
          document.getElementById('m3').value = d.m3; document.getElementById('o3').value = d.o3;
        }
      }).catch(e => console.log(e));
    }, 500);

    function saveCalib() {
      const data = {
        m1: parseFloat(document.getElementById('m1').value)||1, o1: parseFloat(document.getElementById('o1').value)||0,
        m2: parseFloat(document.getElementById('m2').value)||1, o2: parseFloat(document.getElementById('o2').value)||0,
        m3: parseFloat(document.getElementById('m3').value)||1, o3: parseFloat(document.getElementById('o3').value)||0
      };
      fetch('/api/calibrate', { method: 'POST', body: JSON.stringify(data) }).then(() => alert('Сохранено!'));
    }
  </script>
</body>
</html>
)rawliteral";


// =============================================================================
// НАСТРОЙКИ ОБОРУДОВАНИЯ
// =============================================================================
#define USE_REAL_ADS1115     1   // 1 = реальный замер с ADS1115, 0 = симуляция
#define USE_ILI9341_DISPLAY  1   // 1 = включить 2.8" TFT ILI9341 экран

// Пины подключения ILI9341 (SPI)
#define TFT_CS   17
#define TFT_RST  2
#define TFT_DC   15
#define TFT_MOSI 22
#define TFT_CLK  5

// Пины 4-кнопочной мембранной клавиатуры
#define BTN_CH1  13   // Кнопка 1 (Канал CH1)
#define BTN_CH2  12   // Кнопка 2 (Канал CH2)
#define BTN_CH3  14   // Кнопка 3 (Канал CH3)
#define BTN_MENU 27   // Кнопка 4 (Меню / Настройка / Выбор)

#if USE_REAL_ADS1115
Adafruit_ADS1115 ads; // I2C адрес 0x48
#endif

#if USE_ILI9341_DISPLAY
Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_MOSI, TFT_CLK, TFT_RST);
uint8_t selectedChannel = 1; // Текущий выбранный кнопками канал (1, 2, 3)
unsigned long lastDisplayUpdate = 0;
#endif

// Фильтр токов (EMA)
float filt_ch1 = 0.0f;
float filt_ch2 = 0.0f;
float filt_ch3 = 0.0f;

const char* AP_SSID = "PN-2026_Device";
const char* AP_PASS = "12345678";

DNSServer dnsServer;
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

#pragma pack(push, 1)
struct TelemetryPacket {
  float ch1_ma;       // Ток Канала 1 (мА)
  float ch2_ma;       // Ток Канала 2 (мА)
  float ch3_ma;       // Ток Канала 3 (мА)
  float r1_ohm;       // Замер катушки 1 Wp (Ом)
  float r2_ohm;       // Замер катушки 2 Wc (Ом)
  float gen_ma;       // Установленный ток генератора (мА)
  float batteryVolts; // Напряжение АКБ (В)
  uint8_t batteryPct; // Процент АКБ (%)
  uint8_t genActive;  // 1 = Активный 24В, 0 = Пассивный
};
#pragma pack(pop)

float currentGenMa = 4.0f;
uint8_t isGenActive = 1;

// Вспомогательный пересчет тока мА в ход мм (4..20 мА -> -2.00..+2.00 мм)

float targetPoints[21] = {-2.0, -1.0, 0.0, 1.0, 2.0};
int targetPointsCount = 5;

float getNearestTarget(float mm) {
  if (targetPointsCount == 0) return round(mm);
  float best = targetPoints[0];
  float minDiff = abs(mm - best);
  for(int i=1; i<targetPointsCount; i++) {
    float diff = abs(mm - targetPoints[i]);
    if (diff < minDiff) {
      minDiff = diff;
      best = targetPoints[i];
    }
  }
  return best;
}

// END OF GLOBALS
float maToMm(float ma) {
  return -2.0f + (ma - 4.0f) * (4.0f / 16.0f);
}

// =============================================================================
// ВЕБ-ИНТЕРФЕЙС ПН-2026: LIVE РЕЖИМ (ФОРМУЛЫ РЭ100), ПРОГОНКА И КАТУШКИ
// =============================================================================
const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="ru">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no, viewport-fit=cover">
  <title>ПН-2026 | Полевой Наладчик</title>
  <style>
    :root {
      --bg: #090d16;
      --card-bg: #131a27;
      --border: #24334a;
      --text: #f8fafc;
      --subtext: #8fa0b5;
      --ch1: #00e5ff;
      --ch2: #ffb300;
      --ch3: #00e676;
      --green: #10b981;
      --yellow: #f59e0b;
      --red: #ef4444;
    }
    * { box-sizing: border-box; margin: 0; padding: 0; -webkit-tap-highlight-color: transparent; }
    body {
      background: var(--bg);
      color: var(--text);
      font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif;
      padding: max(8px, env(safe-area-inset-top)) max(8px, env(safe-area-inset-right)) max(58px, env(safe-area-inset-bottom)) max(8px, env(safe-area-inset-left));
      user-select: none;
    }

    /* Навбар */
    .nav-bar {
      position: fixed;
      bottom: 0; left: 0; right: 0;
      height: 50px;
      background: rgba(18, 26, 39, 0.98);
      backdrop-filter: blur(10px);
      border-top: 1px solid var(--border);
      display: flex;
      justify-content: space-around;
      align-items: center;
      z-index: 999;
      padding-bottom: env(safe-area-inset-bottom);
    }
    .nav-item {
      flex: 1; text-align: center; background: none; border: none;
      color: var(--subtext); font-size: 11px; font-weight: 700; cursor: pointer;
      display: flex; flex-direction: column; align-items: center; gap: 2px;
    }
    .nav-item.active { color: var(--ch1); }
    .nav-icon { font-size: 16px; }

    /* Шапка */
    .top-header {
      display: flex; justify-content: space-between; align-items: center;
      margin-bottom: 6px; padding: 2px 4px;
    }
    .logo { font-size: 17px; font-weight: 900; letter-spacing: 0.5px; }
    .logo span { color: var(--ch1); }
    .status-badges { display: flex; gap: 6px; align-items: center; }
    .badge {
      background: var(--card-bg); border: 1px solid var(--border);
      padding: 3px 8px; border-radius: 12px; font-size: 11px; font-weight: 700;
    }
    .badge.online { border-color: var(--green); color: var(--green); }
    .badge.offline { border-color: var(--red); color: var(--red); }
    .btn-browser-guide {
      background: #1e293b; border: 1px solid var(--border); color: #fff;
      padding: 3px 8px; border-radius: 12px; font-size: 11px; font-weight: 700; cursor: pointer;
    }

    /* Экраны */
    .tab-view { display: none; }
    .tab-view.active { display: block; }

    /* 3 БЛОКА LIVE РЕЖИМА */
    .channel-box {
      background: var(--card-bg);
      border: 1px solid var(--border);
      border-radius: 10px;
      padding: 8px 12px;
      margin-bottom: 8px;
    }
    .channel-box.c1 { border-left: 4px solid var(--ch1); }
    .channel-box.c2 { border-left: 4px solid var(--ch2); }
    .channel-box.c3 { border-left: 4px solid var(--ch3); }

    .row-metrics {
      display: grid;
      grid-template-columns: 1.15fr 1fr 1.05fr;
      align-items: center;
      padding: 4px 0;
      border-bottom: 1px solid rgba(255,255,255,0.06);
    }
    
    /* ПРАВКА 3: Увеличенный вытянутый шрифт показаний */
    .metric-val {
      font-size: 26px;
      font-weight: 900;
      font-variant-numeric: tabular-nums;
      display: inline-block;
      font-family: 'Arial Narrow', sans-serif-condensed, sans-serif;
      transform: scale(0.88, 1.25);
      transform-origin: left center;
      white-space: nowrap;
    }

    .col-ch { display: flex; align-items: baseline; gap: 4px; }
    .col-ch-name { font-size: 14px; font-weight: 900; margin-right: 2px; }
    .col-mm { text-align: center; color: #fff; }
    .col-mm .metric-val { transform-origin: center center; }
    .col-gamma { display: flex; align-items: center; justify-content: flex-end; gap: 4px; }
    .col-gamma .metric-val { transform-origin: right center; }

    .status-dot {
      width: 13px; height: 13px; border-radius: 50%; display: inline-block; flex-shrink: 0;
      box-shadow: 0 0 6px currentColor; margin-left: 2px;
    }
    .dot-green { background: var(--green); color: var(--green); }
    .dot-yellow { background: var(--yellow); color: var(--yellow); }
    .dot-red { background: var(--red); color: var(--red); }

    .row-advisor {
      margin-top: 5px; font-size: 11px; line-height: 1.3; color: #cbd5e1;
      display: flex; gap: 6px; align-items: flex-start;
    }
    .adv-tag {
      font-size: 10px; font-weight: 800; padding: 1px 4px; border-radius: 4px;
      background: rgba(0,229,255,0.15); color: var(--ch1); white-space: nowrap;
    }

    /* Выпадающие секции */
    .collapsible {
      background: #0d1420; border: 1px solid var(--border);
      border-radius: 10px; margin-top: 8px; overflow: hidden;
    }
    .collapsible-header {
      padding: 10px 12px; font-size: 12px; font-weight: 800; color: var(--subtext);
      text-transform: uppercase; display: flex; justify-content: space-between; align-items: center; cursor: pointer;
    }
    .collapsible-content { padding: 10px 12px; border-top: 1px solid var(--border); display: none; }
    .collapsible-content.show { display: block; }

    .form-row { display: flex; gap: 8px; margin-bottom: 8px; align-items: center; }
    .form-col { flex: 1; }
    .form-lbl { font-size: 10px; font-weight: 700; color: var(--subtext); margin-bottom: 3px; display: block; }
    .form-inp, .form-sel {
      width: 100%; background: #06090e; border: 1px solid var(--border);
      border-radius: 6px; padding: 7px 8px; color: #fff; font-size: 13px; font-weight: 700;
    }
    .btn-toggle-scale { display: flex; background: #06090e; border: 1px solid var(--border); border-radius: 6px; padding: 2px; }
    .scale-opt {
      flex: 1; border: none; background: none; color: var(--subtext);
      font-size: 11px; font-weight: 800; padding: 6px 0; border-radius: 4px; cursor: pointer;
    }
    .scale-opt.active { background: var(--ch1); color: #000; }

    /* ПРАВКА 1: Контейнер кнопок Прогонка и Назад */
    .progonka-controls { display: flex; gap: 8px; margin-bottom: 8px; }
    .btn-progonka {
      flex: 4; background: var(--ch1); color: #000; border: none;
      padding: 12px; border-radius: 8px; font-size: 14px; font-weight: 900; cursor: pointer;
    }
    .btn-progonka:active { opacity: 0.8; }
    .btn-undo {
      flex: 1; background: #24334a; color: #fff; border: 1px solid var(--border);
      padding: 12px 0; border-radius: 8px; font-size: 15px; font-weight: 900; cursor: pointer;
      display: flex; align-items: center; justify-content: center; gap: 4px;
    }
    .btn-undo:active { background: #334663; }

    /* Таблица прогонки для 3 датчиков */
    .table-points {
      width: 100%; border-collapse: collapse; font-size: 11px; text-align: center; margin-top: 6px;
    }
    .table-points th { background: #080c13; color: var(--subtext); padding: 5px 2px; border: 1px solid var(--border); }
    .table-points td { padding: 5px 2px; border: 1px solid var(--border); font-variant-numeric: tabular-nums; }
    .table-points tr.active-pt { background: rgba(0,229,255,0.12); }

    /* Экран Поверки Катушек */
    .coil-card {
      background: var(--card-bg); border: 1px solid var(--border);
      border-radius: 12px; padding: 12px; margin-bottom: 10px;
    }
    .coil-title { font-size: 12px; font-weight: 800; color: var(--subtext); text-transform: uppercase; margin-bottom: 8px; }
    .coil-res-box {
      background: #070b12; border: 1px solid var(--border);
      border-radius: 8px; padding: 10px; margin-top: 8px;
    }
    .coil-row { display: flex; justify-content: space-between; align-items: center; margin-bottom: 4px; font-size: 13px; }
    .coil-val { font-size: 16px; font-weight: 900; color: #fff; }
    .badge-verdict {
      padding: 4px 8px; border-radius: 6px; font-size: 11px; font-weight: 800; text-align: center; margin-top: 6px;
    }
    .btn-calibr {
      width: 100%; background: #1e293b; color: #fff; border: 1px solid var(--border);
      padding: 10px; border-radius: 8px; font-size: 12px; font-weight: 800; cursor: pointer; margin-top: 8px;
    }

    /* Модальное окно с инструкцией */
    .modal-overlay {
      display: none; position: fixed; top:0; left:0; right:0; bottom:0;
      background: rgba(0,0,0,0.8); z-index: 2000; align-items: center; justify-content: center; padding: 16px;
    }
    .modal-card {
      background: var(--card-bg); border: 1px solid var(--border); border-radius: 12px;
      padding: 16px; max-width: 380px; width: 100%; color: #fff; text-align: left;
    }
    .modal-title { font-size: 15px; font-weight: 800; color: var(--ch1); margin-bottom: 10px; }
    .modal-text { font-size: 12px; color: #cbd5e1; line-height: 1.5; margin-bottom: 14px; }
    .btn-close-modal {
      width: 100%; background: var(--ch1); color: #000; border: none;
      padding: 10px; border-radius: 8px; font-weight: 800; font-size: 13px; cursor: pointer;
    }
  
    /* === ПРОТОКОЛ === */
    .prot-header { font-size: 13px; font-weight: 800; color: var(--ch1); margin-top: 10px; margin-bottom: 5px; }
    .channel-select { display: flex; gap: 4px; margin-bottom: 10px; }
    .ch-btn { flex: 1; padding: 8px; border-radius: 6px; border: 1px solid var(--border); background: #06090e; color: var(--subtext); font-weight: 800; cursor: pointer; }
    .ch-btn.active { background: var(--ch1); color: #000; }

    /* === PINOUT === */
    .pinout-box { background: #080c13; border: 1px solid var(--border); border-radius: 8px; padding: 10px; margin-bottom: 12px; }
    .pinout-title { font-size: 11px; font-weight: 800; color: var(--ch1); margin-bottom: 6px; text-transform: uppercase; }
    .pinout-table { width: 100%; border-collapse: collapse; font-size: 11px; text-align: left; }
    .pinout-table th, .pinout-table td { border: 1px solid var(--border); padding: 4px 6px; }
    .pinout-table th { color: var(--subtext); background: #131a27; }
</style>
</head>
<body>

  <!-- Всплывающее окно инструкции -->
  <div id="guideModal" class="modal-overlay">
    <div class="modal-card">
      <div class="modal-title">📲 Открытие в системном браузере</div>
      <div class="modal-text">
        1. Вверху окна перехвата нажмите <strong>«Отмена»</strong> или <strong>«Готово»</strong>.<br>
        2. Выберите <strong>«Использовать без интернета»</strong>.<br>
        3. Откройте Safari / Chrome / Яндекс и перейдите по адресу:<br>
        <strong style="color:var(--ch1); font-size:14px;">http://192.168.4.1</strong>
      </div>
      <button class="btn-close-modal" onclick="closeBrowserGuide()">Понятно</button>
    </div>
  </div>

  <!-- Шапка -->
  <div class="top-header">
    <div class="logo">ПН-2026 <span>LIVE</span></div>
    <div class="status-badges">
      <button class="btn-browser-guide" onclick="location.href='/calibrate'" style="background:#00e5ff; color:#000;">⚙️ КАЛИБРОВКА</button>
      <button class="btn-browser-guide" onclick="showBrowserGuide()">📲 В браузер</button>
      <div id="badgeWs" class="badge offline">СВЯЗЬ...</div>
      <div id="badgeBat" class="badge">АКБ: --%</div>
    </div>
  </div>

  <!-- ==================== ЭКРАН 1: РЕЖИМ LIVE ==================== -->
  <div id="tab-live" class="tab-view active">

    <!-- БЛОК КАНАЛ 1 -->
    <div class="channel-box c1">
      <div class="row-metrics">
        <div class="col-ch">
          <span class="col-ch-name" style="color:var(--ch1);">CH1</span>
          <span id="ch1_ma_text" class="metric-val">0.00 мА</span>
        </div>
        <div class="col-mm">
          <span id="ch1_mm_text" class="metric-val">0.00 мм</span>
        </div>
        <div class="col-gamma">
          <span id="ch1_gamma_text" class="metric-val">δ: 0.0%</span>
          <span id="ch1_dot" class="status-dot dot-green"></span>
        </div>
      </div>
      <div class="row-advisor">
        <span class="adv-tag">СОВЕТНИК 1</span>
        <span id="ch1_adv">Датчик подключен. Сигнал стабилен.</span>
      </div>
    </div>

    <!-- БЛОК КАНАЛ 2 -->
    <div class="channel-box c2">
      <div class="row-metrics">
        <div class="col-ch">
          <span class="col-ch-name" style="color:var(--ch2);">CH2</span>
          <span id="ch2_ma_text" class="metric-val">0.00 мА</span>
        </div>
        <div class="col-mm">
          <span id="ch2_mm_text" class="metric-val">0.00 мм</span>
        </div>
        <div class="col-gamma">
          <span id="ch2_gamma_text" class="metric-val">δ: 0.0%</span>
          <span id="ch2_dot" class="status-dot dot-green"></span>
        </div>
      </div>
      <div class="row-advisor">
        <span class="adv-tag" style="color:var(--ch2); background:rgba(255,179,0,0.15);">СОВЕТНИК 2</span>
        <span id="ch2_adv">Датчик подключен. Сигнал стабилен.</span>
      </div>
    </div>

    <!-- БЛОК КАНАЛ 3 -->
    <div class="channel-box c3">
      <div class="row-metrics">
        <div class="col-ch">
          <span class="col-ch-name" style="color:var(--ch3);">CH3</span>
          <span id="ch3_ma_text" class="metric-val">0.00 мА</span>
        </div>
        <div class="col-mm">
          <span id="ch3_mm_text" class="metric-val">0.00 мм</span>
        </div>
        <div class="col-gamma">
          <span id="ch3_gamma_text" class="metric-val">δ: 0.0%</span>
          <span id="ch3_dot" class="status-dot dot-green"></span>
        </div>
      </div>
      <div class="row-advisor">
        <span class="adv-tag" style="color:var(--ch3); background:rgba(0,230,118,0.15);">СОВЕТНИК 3</span>
        <span id="ch3_adv">Датчик подключен. Сигнал стабилен.</span>
      </div>
    </div>

    <!-- ВЫПАДАЮЩЕЕ ОКНО: ПРОГОНКА 3 ДАТЧИКОВ ПО ТОЧКАМ -->
    <div class="collapsible">
      <div class="collapsible-header" onclick="toggleSection('secProgonka')">
        <span>📋 Поверка 3-х датчиков по точкам</span>
        <span id="secProgonkaArrow">▼</span>
      </div>
      <div id="secProgonka" class="collapsible-content show">
        
        <div class="form-row">
          <div class="form-col" style="flex:0.6;">
            <span class="form-lbl">Кол-во точек:</span>
            <input type="number" id="inpPtCount" class="form-inp" value="5" min="2" max="21" onchange="onPtCountChange()">
          </div>
          <div class="form-col" style="flex:1.4;">
            <span class="form-lbl">Точки прогона (мм):</span>
            <input type="text" id="inpPtList" class="form-inp" value="-2.0, -1.0, 0.0, 1.0, 2.0" onchange="parseCustomPoints()">
          </div>
        </div>

        <!-- ПРАВКА 1: Кнопка "Назад" возле кнопки ПРОГОНКА -->
        <div class="progonka-controls">
          <button id="btnProgonka" class="btn-progonka" onclick="doProgonkaStep()">
            ▶ ПРОГОНКА: Точка 1 (-2.00 мм)
          </button>
          <button id="btnUndo" class="btn-undo" onclick="undoProgonkaStep()" title="Шаг назад (Undo)">
            ↩ Назад
          </button>
        </div>

        <!-- Таблица для 3-х датчиков -->
        <table class="table-points">
          <thead>
            <tr>
              <th rowspan="2">№</th>
              <th rowspan="2">Ход (мм)</th>
              <th rowspan="2">I расч</th>
              <th colspan="2" style="color:var(--ch1)">CH1</th>
              <th colspan="2" style="color:var(--ch2)">CH2</th>
              <th colspan="2" style="color:var(--ch3)">CH3</th>
            </tr>
            <tr>
              <th style="color:var(--ch1)">I факт</th>
              <th style="color:var(--ch1)">δ (%)</th>
              <th style="color:var(--ch2)">I факт</th>
              <th style="color:var(--ch2)">δ (%)</th>
              <th style="color:var(--ch3)">I факт</th>
              <th style="color:var(--ch3)">δ (%)</th>
            </tr>
          </thead>
          <tbody id="tblPointsBody">
          </tbody>
        </table>

      </div>
    </div>

    <!-- НАСТРОЙКИ ДИАПАЗОНОВ -->
    <div class="collapsible">
      <div class="collapsible-header" onclick="toggleSection('secSettings')">
        <span>⚙️ Настройка диапазона хода и шкалы</span>
        <span id="secSettingsArrow">▼</span>
      </div>
      <div id="secSettings" class="collapsible-content">
        <div class="form-row">
          <div class="form-col">
            <span class="form-lbl">Токовая шкала:</span>
            <div class="btn-toggle-scale">
              <button id="btnScale420" class="scale-opt active" onclick="setScale(4, 20)">4–20 мА</button>
              <button id="btnScale15" class="scale-opt" onclick="setScale(1, 5)">1–5 мА</button>
            </div>
          </div>
          <div class="form-col">
            <span class="form-lbl">Текущий эталон микрометра S_i (мм):</span>
            <input type="number" id="inpRef" class="form-inp" value="0.00" step="0.05" oninput="updateRef()">
          </div>
        </div>

        <div class="form-row">
          <div class="form-col">
            <span class="form-lbl">Ход S_min (мм):</span>
            <input type="number" id="inpSMin" class="form-inp" value="-2.0" step="0.5" onchange="updateStroke()">
          </div>
          <div class="form-col">
            <span class="form-lbl">Ход S_max (мм):</span>
            <input type="number" id="inpSMax" class="form-inp" value="2.0" step="0.5" onchange="updateStroke()">
          </div>
        </div>
      </div>
    </div>

  </div>

  <!-- ==================== ЭКРАН 2: ПОВЕРКА КАТУШЕК ==================== -->
  <div id="tab-coils" class="tab-view">
    <div class="coil-card">
      <div class="coil-title">Опрос датчика (Таблица 30 РЭ Вибробит 100)</div>
      
      <div class="form-row">
        <div class="form-col">
          <span class="form-lbl">Тип датчика:</span>
          <select id="selSensorCoil" class="form-sel" onchange="updateCoilNominals()">
            <option value="DVT10">ДВТ10 / ДВТ10Ex (1 катушка)</option>
            <option value="DVT20" selected>ДВТ20 / ДВТ20Ex / ДВТ21 (1 катушка)</option>
            <option value="DVT23">ДВТ23 (1 катушка)</option>
            <option value="DVT30">ДВТ30 (1 катушка)</option>
            <option value="DVT40_10">ДВТ40.10 (2 катушки: Wp, Wc)</option>
            <option value="DVT40_20">ДВТ40.20 (2 катушки: Wp, Wc)</option>
            <option value="DVT40_30">ДВТ40.30 (2 катушки: Wp, Wc)</option>
            <option value="DVT40_40">ДВТ40.40 (1 катушка)</option>
            <option value="DVT40_50">ДВТ40.50 (2 катушки: Wp, Wc)</option>
            <option value="DVT43_20">ДВТ43.20 (2 катушки: Wp, Wc)</option>
            <option value="DVT43_30">ДВТ43.30 (2 катушки: Wp, Wc)</option>
            <option value="DVT43_40">ДВТ43.40 (1 катушка)</option>
            <option value="DVT43_50">ДВТ43.50 (2 катушки: Wp, Wc)</option>
            <option value="DVT50">ДВТ50 (1 катушка)</option>
            <option value="DVT60_10">ДВТ60.10 (1 катушка)</option>
            <option value="DVT60_16">ДВТ60.16 (1 катушка)</option>
            <option value="DVT60_20">ДВТ60.20 (1 катушка)</option>
            <option value="DVT70">ДВТ70 (2 катушки: Wp, Wc)</option>
          </select>
        </div>
      </div>

      <div class="form-row">
        <div class="form-col">
          <span class="form-lbl">Длина шлейфа кабеля l (м):</span>
          <input type="number" id="inpCableLen" class="form-inp" value="5.0" step="0.5" oninput="updateCoilNominals()">
        </div>
        <div class="form-col">
          <span class="form-lbl">Коррекция ПН (R_цепей, Ом):</span>
          <input type="number" id="inpPnOffset" class="form-inp" value="0.25" step="0.05" oninput="updateCoilNominals()">
        </div>
      </div>

      <button class="btn-calibr" onclick="autoCalibratePnZero()">
        ⚡ Автокалибровка нуля ПН (Тара через внутренний канал)
      </button>
    </div>

    <div class="coil-card">
      <div class="coil-title">Замер разъема РСГ7ТВ (с вычетом цепей ПН)</div>

      
      <div class="pinout-box" id="pinoutBox">
        <div class="pinout-title">Схема подключения (Разъем РС7ТВ)</div>
        <table class="pinout-table">
          <thead><tr><th>Контакт</th><th>Цепь</th></tr></thead>
          <tbody id="pinoutTbody"></tbody>
        </table>
      </div>

      <div class="coil-res-box">
        <div class="coil-row">
          <span>Обмотка возбуждения (Wp):</span>
          <span class="coil-val" id="r1_meas_text">-- Ом</span>
        </div>
        <div class="coil-row" style="font-size:11px; color:var(--subtext);">
          <span>Паспортное по формуле:</span>
          <span id="r1_nom_text">1.13 ± 0.10 Ом</span>
        </div>
        <div id="r1_verdict" class="badge-verdict" style="background:rgba(16,185,129,0.2); color:var(--green);">НОРМА</div>
      </div>

      <div class="coil-res-box" id="boxSignalCoil" style="display:none;">
        <div class="coil-row">
          <span>Сигнальная обмотка (Wc):</span>
          <span class="coil-val" id="r2_meas_text">-- Ом</span>
        </div>
        <div class="coil-row" style="font-size:11px; color:var(--subtext);">
          <span>Паспортное по формуле:</span>
          <span id="r2_nom_text">4.75 ± 0.80 Ом</span>
        </div>
        <div id="r2_verdict" class="badge-verdict" style="background:rgba(16,185,129,0.2); color:var(--green);">НОРМА</div>
      </div>
    </div>
  </div>

  
  
  
  <!-- ==================== ЭКРАН 3: ПРОТОКОЛ (ГОСТ) ==================== -->
  <div id="tab-protocol" class="tab-view">
    <div class="coil-card">
      <div class="coil-title">Настройки партии приборов (как в Live)</div>
      
      <div class="form-row">
        <div class="form-col">
          <span class="form-lbl">Кол-во приборов:</span>
          <select id="protCount" class="form-sel" onchange="buildProtConfig()">
            <option value="1">1 канал (CH1)</option>
            <option value="2">2 канала (CH1, CH2)</option>
            <option value="3" selected>3 канала (CH1, CH2, CH3)</option>
          </select>
        </div>
      </div>
      
      <div class="form-row">
        <div class="form-col">
          <span class="form-lbl">Тип Датчика (ДВТ):</span>
          <select id="protDvtType" class="form-sel" onchange="updateProtIpOptions()">
             <!-- Populated dynamically -->
          </select>
        </div>
        <div class="form-col">
          <span class="form-lbl">Тип ИП:</span>
          <select id="protIpType" class="form-sel" onchange="resetProtGost()">
             <!-- Populated dynamically -->
          </select>
        </div>
      </div>
      
      <div class="form-row">
        <div class="form-col">
          <span class="form-lbl">Ток (мА):</span>
          <select id="protScale" class="form-sel" onchange="resetProtGost()">
            <option value="4-20">4..20 мА</option>
            <option value="1-5">1..5 мА</option>
          </select>
        </div>
        <div class="form-col">
          <span class="form-lbl">Кабель (м):</span>
          <input type="number" id="protCable" class="form-inp" value="5" step="1" onchange="resetProtGost()">
        </div>
      </div>
      
      <div class="form-row">
        <div class="form-col">
          <span class="form-lbl">Мин. зазор (мм):</span>
          <input type="number" id="protSmin" class="form-inp" value="0" onchange="resetProtGost()">
        </div>
        <div class="form-col">
          <span class="form-lbl">Макс. зазор (мм):</span>
          <input type="number" id="protSmax" class="form-inp" value="5" onchange="resetProtGost()">
        </div>
      </div>
      
      <div class="form-row">
        <div class="form-col">
          <span class="form-lbl">Точки прогона (мм через запятую):</span>
          <input type="text" id="protPointsInput" class="form-inp" value="0, 1.25, 2.5, 3.75, 5" onchange="resetProtGost()">
        </div>
      </div>

    </div>

    <div class="coil-card">
      <div class="coil-title">Заводские номера</div>
      <div id="protConfigArea"></div>
    </div>

    <div class="coil-card">
      <div class="coil-title">Прогонка и Формирование</div>
      <div class="progonka-controls">
        <button id="btnProtProgonka" class="btn-progonka" onclick="doProtStepGOST()">▶ Записать точку 1</button>
        <button class="btn-undo" onclick="undoProtStepGOST()">↩ Назад</button>
      </div>

      <div style="overflow-x: auto; margin-top: 10px;">
        <table class="table-points" id="tblProtPointsGOST">
        </table>
      </div>

      <button class="btn-calibr" style="background:var(--ch1); color:#000; margin-top:15px; font-size: 14px;" onclick="generateDocGOST()">
        📝 СФОРМИРОВАТЬ ПРОТОКОЛ (ГОСТ)
      </button>
    </div>
  </div>

  <!-- Навигация -->
  <div class="nav-bar">
    <button class="nav-item active" onclick="switchNav('tab-live', this)">
      <span class="nav-icon">⚡</span><span>Режим Live</span>
    </button>
    <button class="nav-item" onclick="switchNav('tab-coils', this)">
      <span class="nav-icon">🧲</span><span>Поверка катушек</span>
    </button>

    <button class="nav-item" onclick="switchNav('tab-protocol', this)">
      <span class="nav-icon">📄</span><span>Протокол</span>
    </button>
  </div>

  <script>
    function savePresets() {
      const presets = {
        I_MIN, I_MAX, S_MIN, S_MAX, S_REF,
        ptCount: document.getElementById('inpPtCount').value,
        ptList: document.getElementById('inpPtList').value,
        sensorCoil: document.getElementById('selSensorCoil').value,
        cableLen: document.getElementById('inpCableLen').value,
        pnOffset: document.getElementById('inpPnOffset').value
      };
      localStorage.setItem('PN2026_PRESETS', JSON.stringify(presets));
    }

    function loadPresets() {
      const raw = localStorage.getItem('PN2026_PRESETS');
      if (!raw) return;
      try {
        const p = JSON.parse(raw);
        if (p.I_MIN !== undefined && p.I_MAX !== undefined) {
          I_MIN = p.I_MIN; I_MAX = p.I_MAX;
          document.getElementById('btnScale420').classList.toggle('active', I_MIN === 4);
          document.getElementById('btnScale15').classList.toggle('active', I_MIN === 1);
        }
        if (p.S_MIN !== undefined) { S_MIN = parseFloat(p.S_MIN); document.getElementById('inpSMin').value = S_MIN; }
        if (p.S_MAX !== undefined) { S_MAX = parseFloat(p.S_MAX); document.getElementById('inpSMax').value = S_MAX; }
        if (p.S_REF !== undefined) { S_REF = parseFloat(p.S_REF); document.getElementById('inpRef').value = S_REF; }
        if (p.ptCount !== undefined) document.getElementById('inpPtCount').value = p.ptCount;
        if (p.ptList !== undefined) document.getElementById('inpPtList').value = p.ptList;
        if (p.sensorCoil !== undefined) document.getElementById('selSensorCoil').value = p.sensorCoil;
        if (p.cableLen !== undefined) document.getElementById('inpCableLen').value = p.cableLen;
        if (p.pnOffset !== undefined) document.getElementById('inpPnOffset').value = p.pnOffset;
      } catch(e) { console.error("Ошибка автозагрузки пресетов:", e); }
    }

    function showBrowserGuide() {
      document.getElementById('guideModal').style.display = 'flex';
    }
    function closeBrowserGuide() {
      document.getElementById('guideModal').style.display = 'none';
    }

    function switchNav(id, btn) {
      document.querySelectorAll('.tab-view').forEach(e => e.classList.remove('active'));
      document.querySelectorAll('.nav-item').forEach(e => e.classList.remove('active'));
      document.getElementById(id).classList.add('active');
      btn.classList.add('active');
    }

    function toggleSection(id) {
      const el = document.getElementById(id);
      const isShown = el.classList.toggle('show');
      document.getElementById(id + 'Arrow').innerText = isShown ? '▲' : '▼';
    }

    let I_MIN = 4.0, I_MAX = 20.0;
    let S_MIN = -2.0, S_MAX = 2.0;
    let S_REF = 0.0;

    function setScale(minI, maxI) {
      I_MIN = minI; I_MAX = maxI;
      document.getElementById('btnScale420').classList.toggle('active', minI === 4);
      document.getElementById('btnScale15').classList.toggle('active', minI === 1);
      generatePointsTable();
      generateProtTable();
      savePresets();
    }
    function updateStroke() {
      S_MIN = parseFloat(document.getElementById('inpSMin').value) || -2.0;
      S_MAX = parseFloat(document.getElementById('inpSMax').value) || 2.0;
      onPtCountChange();
      savePresets();
    }
    function updateRef() {
      S_REF = parseFloat(document.getElementById('inpRef').value) || 0.0;
      savePresets();
    }

    function getKn() {
      const S = Math.abs(S_MAX - S_MIN);
      if (S === 0) return 1.0;
      return (I_MAX - I_MIN) / S;
    }

    function maToMm(ma) {
      const Kn = getKn();
      return S_MIN + (ma - I_MIN) / Kn;
    }

    
      function getNearestTargetJS(mm) {
        if (!ptArray || ptArray.length === 0) return Math.round(mm);
        let best = ptArray[0];
        let minDiff = Math.abs(mm - best);
        for (let i = 1; i < ptArray.length; i++) {
          let diff = Math.abs(mm - ptArray[i]);
          if (diff < minDiff) {
            minDiff = diff;
            best = ptArray[i];
          }
        }
        return best;
      }
      function calcDeltaRE100(ma, si_mm) {

      const S = Math.abs(S_MAX - S_MIN);
      if (S === 0) return 0;
      const Kn = getKn();
      const calcOffset = (ma - I_MIN) / Kn;
      const targetOffset = si_mm - S_MIN;
      return Math.abs((calcOffset - targetOffset) / S) * 100.0;
    }

    function generateAdvisorMsg(chNum, ma, mm, delta) {
      const diffMa = ma - I_MIN;
      if (Math.abs(S_REF - S_MIN) < 0.1) {
        if (diffMa > 0.04) {
          const turns = (diffMa * 2.2).toFixed(1);
          return `Ток завышен (+${diffMa.toFixed(2)} мА). Поверните «НОЛЬ» <strong>ПРОТИВ часовой</strong> (~${turns} об.).`;
        } else if (diffMa < -0.04) {
          const turns = (Math.abs(diffMa) * 2.2).toFixed(1);
          return `Ток занижен (${diffMa.toFixed(2)} мА). Поверните «НОЛЬ» <strong>ПО часовой</strong> (~${turns} об.).`;
        } else {
          return `Ноль выставлен точно (${ma.toFixed(2)} мА).`;
        }
      }

      if (delta <= 1.5) {
        return `Канал ${chNum} в допуске (δ=${delta.toFixed(1)}% ≤ 1.5%). Линейность отличная.`;
      } else if (delta < 2.5) {
        return `Внимание: δ=${delta.toFixed(1)}% (1.5%..2.5%). Подстройте винт «НОЛЬ».`;
      } else {
        return `БРАК: δ=${delta.toFixed(1)}% ≥ 2.5%. Проверьте зазор или подстройте «ДИАПАЗОН».`;
      }
    }

    let ptArray = [-2.0, -1.0, 0.0, 1.0, 2.0];
    let ptData = [];
    let currentPtIndex = 0;

    function onPtCountChange() {
      let cnt = parseInt(document.getElementById('inpPtCount').value) || 5;
      cnt = Math.max(2, Math.min(21, cnt));
      ptArray = [];
      const span = S_MAX - S_MIN;
      for (let i = 0; i < cnt; i++) {
        const val = S_MIN + (span * i) / (cnt - 1);
        ptArray.push(parseFloat(val.toFixed(2)));
      }
      document.getElementById('inpPtList').value = ptArray.join(', ');
      generatePointsTable();
      generateProtTable();
      savePresets();
    }

    function parseCustomPoints() {
      const raw = document.getElementById('inpPtList').value;
      const parsed = raw.split(',').map(s => parseFloat(s.trim())).filter(n => !isNaN(n));
      if (parsed.length >= 2) {
        ptArray = parsed;
        document.getElementById('inpPtCount').value = ptArray.length;
      }
      generatePointsTable();
      generateProtTable();
      savePresets();
    }

    function generatePointsTable() {
      const Kn = getKn();
      ptData = ptArray.map((ptMm, idx) => {
        const targetOffset = ptMm - S_MIN;
        const iCalc = I_MIN + targetOffset * Kn;
        return {
          idx: idx + 1,
          ptMm: ptMm,
          iCalc: iCalc,
          ch1: { iFact: null, delta: null },
          ch2: { iFact: null, delta: null },
          ch3: { iFact: null, delta: null }
        };
      });
      currentPtIndex = 0;
      updateProgonkaButton();
      renderPointsTable();
    }

    function updateProgonkaButton() {
      const btn = document.getElementById('btnProgonka');
      if (currentPtIndex < ptData.length) {
        const pt = ptData[currentPtIndex];
        btn.innerText = `▶ ПРОГОНКА: Точка ${pt.idx} (${pt.ptMm > 0 ? '+' : ''}${pt.ptMm.toFixed(2)} мм)`;
        btn.style.background = 'var(--ch1)';
        btn.style.color = '#000';
      } else {
        btn.innerText = `✅ Прогонка завершена (Начать заново)`;
        btn.style.background = 'var(--green)';
        btn.style.color = '#fff';
      }
    }

    let lastLiveMa1 = 0, lastLiveMa2 = 0, lastLiveMa3 = 0;

    function doProgonkaStep() {
      if (currentPtIndex >= ptData.length) {
        currentPtIndex = 0;
        ptData.forEach(p => {
          p.ch1 = { iFact: null, delta: null };
          p.ch2 = { iFact: null, delta: null };
          p.ch3 = { iFact: null, delta: null };
        });
        updateProgonkaButton();
        renderPointsTable();
        return;
      }

      const item = ptData[currentPtIndex];
      item.ch1 = { iFact: lastLiveMa1, delta: calcDeltaRE100(lastLiveMa1, item.ptMm) };
      item.ch2 = { iFact: lastLiveMa2, delta: calcDeltaRE100(lastLiveMa2, item.ptMm) };
      item.ch3 = { iFact: lastLiveMa3, delta: calcDeltaRE100(lastLiveMa3, item.ptMm) };

      currentPtIndex++;
      updateProgonkaButton();
      renderPointsTable();
    }

    function undoProgonkaStep() {
      if (currentPtIndex > 0) {
        currentPtIndex--;
        const item = ptData[currentPtIndex];
        item.ch1 = { iFact: null, delta: null };
        item.ch2 = { iFact: null, delta: null };
        item.ch3 = { iFact: null, delta: null };
        updateProgonkaButton();
        renderPointsTable();
      }
    }

    function getDeltaStyle(delta, defaultVar) {
      if (delta === null) return `color: var(--subtext);`;
      if (delta >= 2.5) {
        return `color: #ffffff; background: rgba(239, 68, 68, 0.85); font-weight: 900; border-radius: 4px; border: 1px solid var(--red);`;
      } else if (delta > 1.5) {
        return `color: var(--yellow); background: rgba(245, 158, 11, 0.18); font-weight: 800; border-radius: 4px;`;
      }
      return `color: var(${defaultVar}); font-weight: 800;`;
    }

    function renderPointsTable() {
      const tbody = document.getElementById('tblPointsBody');
      tbody.innerHTML = '';

      ptData.forEach((item, i) => {
        const tr = document.createElement('tr');
        if (i === currentPtIndex) tr.className = 'active-pt';

        const f1 = item.ch1.iFact !== null ? `${item.ch1.iFact.toFixed(2)}` : '—';
        const d1 = item.ch1.delta !== null ? `${item.ch1.delta.toFixed(1)}%` : '—';

        const f2 = item.ch2.iFact !== null ? `${item.ch2.iFact.toFixed(2)}` : '—';
        const d2 = item.ch2.delta !== null ? `${item.ch2.delta.toFixed(1)}%` : '—';

        const f3 = item.ch3.iFact !== null ? `${item.ch3.iFact.toFixed(2)}` : '—';
        const d3 = item.ch3.delta !== null ? `${item.ch3.delta.toFixed(1)}%` : '—';

        const styleD1 = getDeltaStyle(item.ch1.delta, '--ch1');
        const styleD2 = getDeltaStyle(item.ch2.delta, '--ch2');
        const styleD3 = getDeltaStyle(item.ch3.delta, '--ch3');

        tr.innerHTML = `
          <td>${item.idx}</td>
          <td><strong>${item.ptMm > 0 ? '+' : ''}${item.ptMm.toFixed(2)}</strong></td>
          <td>${item.iCalc.toFixed(2)}</td>
          <td style="color:var(--ch1);">${f1}</td>
          <td style="${styleD1}">${d1}</td>
          <td style="color:var(--ch2);">${f2}</td>
          <td style="${styleD2}">${d2}</td>
          <td style="color:var(--ch3);">${f3}</td>
          <td style="${styleD3}">${d3}</td>
        `;
        tbody.appendChild(tr);
      });
    }

    const COIL_DB = {
      "DVT10":    { name: "ДВТ10 / ДВТ10Ex",  r0: 0.900, k: 0.084, tol: 0.100, hasSig: false },
      "DVT20":    { name: "ДВТ20 / 20Ex / 21",r0: 0.710, k: 0.084, tol: 0.100, hasSig: false },
      "DVT23":    { name: "ДВТ23",            r0: 0.780, k: 0.084, tol: 0.100, hasSig: false },
      "DVT30":    { name: "ДВТ30",            r0: 0.900, k: 0.084, tol: 0.100, hasSig: false },
      "DVT40_10": { name: "ДВТ40.10",         r0: 1.540, k: 0.084, tol: 0.100, hasSig: true,  sr0: 0.860, sk: 0.190, stol: 0.100 },
      "DVT40_20": { name: "ДВТ40.20",         r0: 1.600, k: 0.084, tol: 0.100, hasSig: true,  sr0: 0.910, sk: 0.190, stol: 0.100 },
      "DVT40_30": { name: "ДВТ40.30",         r0: 2.190, k: 0.084, tol: 0.100, hasSig: true,  sr0: 1.110, sk: 0.190, stol: 0.100 },
      "DVT40_40": { name: "ДВТ40.40",         r0: 1.320, k: 0.084, tol: 0.100, hasSig: false },
      "DVT40_50": { name: "ДВТ40.50",         r0: 2.100, k: 0.084, tol: 0.100, hasSig: true,  sr0: 1.350, sk: 0.190, stol: 0.100 },
      "DVT43_20": { name: "ДВТ43.20",         r0: 1.550, k: 0.084, tol: 0.350, hasSig: true,  sr0: 3.800, sk: 0.190, stol: 0.800 },
      "DVT43_30": { name: "ДВТ43.30",         r0: 1.700, k: 0.084, tol: 0.500, hasSig: true,  sr0: 5.000, sk: 0.190, stol: 1.000 },
      "DVT43_40": { name: "ДВТ43.40",         r0: 1.060, k: 0.084, tol: 0.300, hasSig: false },
      "DVT43_50": { name: "ДВТ43.50",         r0: 1.900, k: 0.084, tol: 0.300, hasSig: true,  sr0: 4.900, sk: 0.190, stol: 1.000 },
      "DVT50":    { name: "ДВТ50",            r0: 0.970, k: 0.084, tol: 0.100, hasSig: false },
      "DVT60_10": { name: "ДВТ60.10",         r0: 0.330, k: 0.084, tol: 0.100, hasSig: false },
      "DVT60_16": { name: "ДВТ60.16",         r0: 0.550, k: 0.084, tol: 0.100, hasSig: false },
      "DVT60_20": { name: "ДВТ60.20",         r0: 0.670, k: 0.084, tol: 0.100, hasSig: false },
      "DVT70":    { name: "ДВТ70",            r0: 3.520, k: 0.084, tol: 0.100, hasSig: true,  sr0: 12.44, sk: 0.190, stol: 0.100 }
    };

    let curPnOffset = 0.25;
    function autoCalibratePnZero() {
      curPnOffset = lastRawR1 > 0 ? lastRawR1 : 0.25;
      document.getElementById('inpPnOffset').value = curPnOffset.toFixed(2);
      alert(`Автокалибровка нуля ПН-2026 завершена!\nСопротивление цепей: ${curPnOffset.toFixed(2)} Ом учтено как ТАРА.`);
      updateCoilNominals();
    }

    let lastRawR1 = 0, lastRawR2 = 0;
    function updateCoilNominals() {
      const type = document.getElementById('selSensorCoil').value;
      const l = parseFloat(document.getElementById('inpCableLen').value) || 0;
      curPnOffset = parseFloat(document.getElementById('inpPnOffset').value) || 0;
      const cfg = COIL_DB[type] || COIL_DB["DVT20"];

      
      // PINOUT LOGIC
      const pinoutTbody = document.getElementById('pinoutTbody');
      let p_html = '';
      if (type.includes("DVT43") || type.includes("DVT40") || type.includes("DVT70")) {
        // Двухкатушечные датчики (Сигнальная + Возбуждение)
        p_html = `
          <tr><td>1, 2</td><td>Общий</td></tr>
          <tr><td>3, 5</td><td>Обмотка сигнальная (Wc)</td></tr>
          <tr><td style="color:var(--ch1);">6, 7</td><td style="color:var(--ch1);">Обмотка возбуждения (Wp)</td></tr>
        `;
      } else {
        // Однокатушечные датчики
        p_html = `
          <tr><td style="color:var(--ch1);">2, 5</td><td style="color:var(--ch1);">Обмотка возбуждения (Wp)</td></tr>
          <tr><td>1, 3</td><td>Общий</td></tr>
          <tr><td>6, 7</td><td>GND</td></tr>
        `;
      }
      pinoutTbody.innerHTML = p_html;

      const r1Nom = cfg.r0 + cfg.k * l;
      document.getElementById('r1_nom_text').innerText = `${r1Nom.toFixed(2)} ± ${cfg.tol.toFixed(2)} Ом`;

      const r1Clean = Math.max(0, lastRawR1 - curPnOffset);
      document.getElementById('r1_meas_text').innerText = `${r1Clean.toFixed(2)} Ом`;

      const r1Pass = Math.abs(r1Clean - r1Nom) <= cfg.tol;
      const r1Verd = document.getElementById('r1_verdict');
      r1Verd.innerText = r1Pass ? "НОРМА" : (r1Clean < r1Nom - cfg.tol ? "КЗ ОБМОТКИ" : "ОБРЫВ / ВЫСОКОЕ R");
      r1Verd.style.background = r1Pass ? "rgba(16,185,129,0.2)" : "rgba(239,68,68,0.2)";
      r1Verd.style.color = r1Pass ? "var(--green)" : "var(--red)";

      const boxSig = document.getElementById('boxSignalCoil');
      if (cfg.hasSig) {
        boxSig.style.display = 'block';
        const r2Nom = cfg.sr0 + cfg.sk * l;
        document.getElementById('r2_nom_text').innerText = `${r2Nom.toFixed(2)} ± ${cfg.stol.toFixed(2)} Ом`;

        const r2Clean = Math.max(0, lastRawR2 - curPnOffset);
        document.getElementById('r2_meas_text').innerText = `${r2Clean.toFixed(2)} Ом`;

        const r2Pass = Math.abs(r2Clean - r2Nom) <= cfg.stol;
        const r2Verd = document.getElementById('r2_verdict');
        r2Verd.innerText = r2Pass ? "НОРМА" : (r2Clean < r2Nom - cfg.stol ? "КЗ ОБМОТКИ" : "ОБРЫВ / ВЫСОКОЕ R");
        r2Verd.style.background = r2Pass ? "rgba(16,185,129,0.2)" : "rgba(239,68,68,0.2)";
        r2Verd.style.color = r2Pass ? "var(--green)" : "var(--red)";
      } else {
        boxSig.style.display = 'none';
      }
      savePresets();
    }

    
    
    
    // === ПРОДВИНУТАЯ ЛОГИКА ПРОТОКОЛА V4 ===
    const IP_FAMILIES = {
        "DVT_10_20_30_50_60": [{ id: "ИП34", name: "ИП34" }, { id: "ИП36", name: "ИП36" }, { id: "ИП24", name: "ИП24" }],
        "DVT_40_43_70": [{ id: "ИП43", name: "ИП43" }, { id: "ИП36", name: "ИП36" }]
    };

    let protState = {
        count: 3, dvtType: 'DVT20', ipType: 'ИП34', cable: 5, iMin: 4, iMax: 20, sMin: 0, sMax: 5,
        points: [0, 1.25, 2.5, 3.75, 5],
        sn: [
            { dvt: '', ip: '', data: [] },
            { dvt: '', ip: '', data: [] },
            { dvt: '', ip: '', data: [] }
        ],
        ptIndex: 0
    };

    function initProtTab() {
        let dvtSel = document.getElementById('protDvtType');
        let opts = '';
        for(let k in COIL_DB) opts += `<option value="${k}">${COIL_DB[k].name}</option>`;
        dvtSel.innerHTML = opts;
        dvtSel.value = 'DVT20';
        updateProtIpOptions();
        buildProtConfig();
    }

    function updateProtIpOptions() {
        let dvt = document.getElementById('protDvtType').value;
        let family = (dvt.includes("43") || dvt.includes("40") || dvt.includes("70")) ? "DVT_40_43_70" : "DVT_10_20_30_50_60";
        let ipSel = document.getElementById('protIpType');
        let opts = '';
        IP_FAMILIES[family].forEach(ip => { opts += `<option value="${ip.id}">${ip.name}</option>`; });
        ipSel.innerHTML = opts;
        resetProtGost();
    }

    function buildProtConfig() {
        protState.count = parseInt(document.getElementById('protCount').value);
        const area = document.getElementById('protConfigArea');
        let html = '';
        for(let i=0; i<protState.count; i++) {
            let sdvt = protState.sn[i] ? protState.sn[i].dvt : '';
            let sip = protState.sn[i] ? protState.sn[i].ip : '';
            html += `
                <div class="form-row" style="border-top:1px solid #24334a; padding-top:8px;">
                    <div class="form-col" style="flex:0.2; justify-content:center; color:var(--ch1); font-weight:bold;">CH${i+1}</div>
                    <div class="form-col"><span class="form-lbl">№ ДВТ:</span><input type="text" class="form-inp" id="p_sndvt_${i}" value="${sdvt}"></div>
                    <div class="form-col"><span class="form-lbl">№ ИП:</span><input type="text" class="form-inp" id="p_snip_${i}" value="${sip}"></div>
                </div>
            `;
        }
        area.innerHTML = html;
        resetProtGost();
    }

    function syncProtState() {
        protState.dvtType = document.getElementById('protDvtType').value;
        protState.ipType = document.getElementById('protIpType').value;
        protState.cable = parseFloat(document.getElementById('protCable').value) || 5;
        let scale = document.getElementById('protScale').value.split('-');
        protState.iMin = parseFloat(scale[0]);
        protState.iMax = parseFloat(scale[1]);
        protState.sMin = parseFloat(document.getElementById('protSmin').value);
        protState.sMax = parseFloat(document.getElementById('protSmax').value);
        let pts = document.getElementById('protPointsInput').value.split(',').map(s => parseFloat(s.trim())).filter(n => !isNaN(n));
        if(pts.length === 0) pts = [0];
        protState.points = pts;
        
        for(let i=0; i<protState.count; i++) {
            if(!protState.sn[i]) protState.sn[i] = {dvt:'', ip:'', data:[]};
            let elDvt = document.getElementById(`p_sndvt_${i}`);
            let elIp = document.getElementById(`p_snip_${i}`);
            if(elDvt) protState.sn[i].dvt = elDvt.value;
            if(elIp) protState.sn[i].ip = elIp.value;
        }
    }

    function resetProtGost() {
        syncProtState();
        protState.ptIndex = 0;
        for(let i=0; i<protState.count; i++) {
            protState.sn[i].data = protState.points.map(pt => ({ ptMm: pt, iFact: null, delta: null }));
        }
        updateProtButtonGOST();
        renderProtTableGOST();
    }

    function updateProtButtonGOST() {
        const btn = document.getElementById('btnProtProgonka');
        if (protState.ptIndex < protState.points.length) {
            btn.innerText = `▶ ЗАПИСАТЬ ТОЧКУ: ${protState.points[protState.ptIndex]} мм`;
            btn.style.background = 'var(--ch1)'; btn.style.color = '#000';
        } else {
            btn.innerText = `✅ Прогонка завершена (Сброс)`;
            btn.style.background = 'var(--green)'; btn.style.color = '#fff';
        }
    }

    function doProtStepGOST() {
        syncProtState();
        if (protState.ptIndex >= protState.points.length) {
            resetProtGost();
            return;
        }
        let currentVals = [lastLiveMa1, lastLiveMa2, lastLiveMa3];
        const S = protState.sMax - protState.sMin;
        const Kn = (protState.iMax - protState.iMin) / S;

        for(let i=0; i<protState.count; i++) {
            const pt = protState.sn[i].data[protState.ptIndex];
            const iCalc = protState.iMin + (pt.ptMm - protState.sMin) * Kn;
            pt.iFact = currentVals[i];
            pt.delta = ((pt.iFact - iCalc) / (protState.iMax - protState.iMin)) * 100.0;
        }
        protState.ptIndex++;
        updateProtButtonGOST();
        renderProtTableGOST();
    }

    function undoProtStepGOST() {
        if (protState.ptIndex > 0) {
            protState.ptIndex--;
            for(let i=0; i<protState.count; i++) {
                protState.sn[i].data[protState.ptIndex].iFact = null;
                protState.sn[i].data[protState.ptIndex].delta = null;
            }
            updateProtButtonGOST();
            renderProtTableGOST();
        }
    }

    function renderProtTableGOST() {
        if(protState.count === 0) return;
        const tbody = document.getElementById('tblProtPointsGOST');
        let html = `<thead><tr><th>Датчик</th><th>ИП</th><th>Параметр</th>`;
        protState.points.forEach(p => html += `<th>${p} мм</th>`);
        html += `</tr></thead><tbody>`;

        for(let i=0; i<protState.count; i++) {
            const c = protState.sn[i];
            const rowI = c.data.map(d => d.iFact !== null ? d.iFact.toFixed(3) : '—').join('</td><td>');
            const rowD = c.data.map(d => d.delta !== null ? d.delta.toFixed(2) : '—').join('</td><td>');
            html += `
                <tr><td rowspan="2" style="background:#080c13;">${c.dvt||'—'}</td><td rowspan="2" style="background:#080c13;">${c.ip||'—'}</td>
                <td>I факт, мА</td><td>${rowI}</td></tr>
                <tr><td>Погрешность, %</td><td>${rowD}</td></tr>
            `;
        }
        html += `</tbody>`;
        tbody.innerHTML = html;
    }

    function generateDocGOST() {
        syncProtState();
        let table431 = `
        <h3>4.3.1. Определение основной приведенной погрешности</h3>
        <table>
            <tr><th rowspan="2">Заводской номер Датчик</th><th rowspan="2">Заводской номер Преобразователь</th><th rowspan="2">Наименование параметра</th><th colspan="${protState.points.length}">Диапазон измерения, мм</th></tr>
            <tr>`;
        protState.points.forEach(p => { table431 += `<th>${p}</th>`; });
        table431 += `</tr>`;

        for(let i=0; i<protState.count; i++) {
            const c = protState.sn[i];
            const rowI = c.data.map(d => d.iFact !== null ? d.iFact.toFixed(3) : '—').join('</td><td>');
            const rowD = c.data.map(d => d.delta !== null ? d.delta.toFixed(2) : '—').join('</td><td>');
            table431 += `
                <tr><td rowspan="3">${c.dvt||'—'}</td><td rowspan="3">${c.ip||'—'}</td><td>Показания выходного сигнала, мА</td><td>${rowI}</td></tr>
                <tr><td>Основная приведенная погрешность измерения, %</td><td>${rowD}</td></tr>
                <tr><td>Предельные отклонения, %</td><td colspan="${c.data.length}">±2,5</td></tr>
            `;
        }
        table431 += `</table>`;

        let table433 = `
        <h3>4.3.3. Определение активного сопротивления обмотки</h3>
        <table>
            <tr><th>Зав. № Датчик</th><th>Зав. № ИП</th><th>Наименование параметра</th><th>Показания омметра, Ом</th><th>Номинальное значение, Ом</th><th>Предельные отклонения, Ом</th></tr>`;
        for(let i=0; i<protState.count; i++) {
            const c = protState.sn[i];
            const cfg = COIL_DB[protState.dvtType];
            const r1Nom = (cfg.r0 + cfg.k * protState.cable).toFixed(2);
            table433 += `<tr><td rowspan="2">${c.dvt||'—'}</td><td rowspan="2">${c.ip||'—'}</td><td>Активное сопротивление обмотки возбуждения, Ом</td><td>—</td><td>${r1Nom}</td><td>±${cfg.tol}</td></tr>`;
            if (cfg.hasSig) {
                const r2Nom = (cfg.sr0 + cfg.sk * protState.cable).toFixed(2);
                table433 += `<tr><td>Активное сопротивление обмотки сигнальной, Ом</td><td>—</td><td>${r2Nom}</td><td>±${cfg.stol}</td></tr>`;
            } else {
                table433 += `<tr><td colspan="4" style="background:#f0f0f0;">Нет сигнальной обмотки</td></tr>`;
            }
        }
        table433 += `</table>`;

        const html = `
        <html xmlns:o='urn:schemas-microsoft-com:office:office' xmlns:w='urn:schemas-microsoft-com:office:word' xmlns='http://www.w3.org/TR/REC-html40'>
        <head><meta charset='utf-8'><title>Протокол ГОСТ</title>
        <style>body{font-family:'Times New Roman',serif;font-size:11pt;}h1,h2,h3{text-align:center;font-size:12pt;}table{width:100%;border-collapse:collapse;margin-bottom:20px;font-size:10pt;}th,td{border:1px solid black;padding:4px;text-align:center;vertical-align:middle;}</style>
        </head><body>
            <div style="text-align:center;">ОБЩЕСТВО С ОГРАНИЧЕННОЙ ОТВЕТСТВЕННОСТЬЮ<br>НАУЧНО-ПРОИЗВОДСТВЕННОЕ ПРЕДПРИЯТИЕ «ВИБРОБИТ»</div>
            <hr>
            <h2>ПРОТОКОЛ КАЛИБРОВКИ № _________ от ___ . ___ . 20___ г.</h2>
            <p><strong>Наименование и тип СИ:</strong> Аппаратура «Вибробит 100», Датчик ${COIL_DB[protState.dvtType].name}, Преобразователь ${protState.ipType}</p>
            ${table431}
            <h3>4.3.2. Определение электрического сопротивления изоляции обмотки</h3>
            <table><tr><th>Зав. № Датчик</th><th>Зав. № ИП</th><th>Наименование параметра</th><th>Показания мегомметра, МОм</th><th>Предельные отклонения, МОм</th></tr>
            <tr><td></td><td></td><td>Электрическое сопротивление изоляции</td><td></td><td>не менее 1</td></tr></table>
            ${table433}
            <br><br><p>Калибровщик: _______________________ / ___________________ /</p>
        </body></html>`;
        
        const blob = new Blob(['\ufeff', html], { type: 'application/msword' });
        const url = URL.createObjectURL(blob);
        const a = document.createElement('a'); a.href = url; a.download = `Protocol_Vibrobit_GOST.doc`;
        document.body.appendChild(a); a.click(); document.body.removeChild(a); URL.revokeObjectURL(url);
    }

      initProtTab();
</script>
</body>
</html>
)rawliteral";

// =============================================================================
// ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ ДЛЯ ЭКРАНА ILI9341 И КЛАВИАТУРЫ
// =============================================================================
#if USE_ILI9341_DISPLAY

void initILI9341Display() {
  tft.begin();
  tft.setRotation(1); // Альбомный режим 320x240
  tft.fillScreen(ILI9341_BLACK);

  // Заставка при старте (3 секунды)
  tft.drawRect(10, 10, 300, 220, ILI9341_CYAN);
  tft.drawRect(12, 12, 296, 216, ILI9341_BLUE);

  tft.setTextColor(ILI9341_CYAN);
  tft.setTextSize(3);
  tft.setCursor(80, 50);
  tft.print("PN-2026");

  tft.setTextColor(ILI9341_WHITE);
  tft.setTextSize(2);
  tft.setCursor(45, 100);
  tft.print("POLEVOY NALADCHIK");

  tft.setTextColor(ILI9341_GREEN);
  tft.setTextSize(1);
  tft.setCursor(95, 150);
  tft.print("System Initialization...");

  tft.setCursor(85, 180);
  tft.setTextColor(ILI9341_YELLOW);
  tft.print("IP: 192.168.4.1");

  delay(3000); // Показ заставки 3 секунды
  tft.fillScreen(ILI9341_BLACK);
}

// Чтение кнопок мембранной клавиатуры
void checkButtonsInput() {
  static unsigned long lastBtnPress = 0;
  if (millis() - lastBtnPress < 180) return; // Простой антидребезг

  if (digitalRead(BTN_CH1) == LOW) {
    selectedChannel = 1;
    lastBtnPress = millis();
    Serial.println("Клавиатура: Выбран CH1");
  } else if (digitalRead(BTN_CH2) == LOW) {
    selectedChannel = 2;
    lastBtnPress = millis();
    Serial.println("Клавиатура: Выбран CH2");
  } else if (digitalRead(BTN_CH3) == LOW) {
    selectedChannel = 3;
    lastBtnPress = millis();
    Serial.println("Клавиатура: Выбран CH3");
  } else if (digitalRead(BTN_MENU) == LOW) {
    selectedChannel = (selectedChannel % 3) + 1; // Переключение по кругу (1->2->3->1)
    lastBtnPress = millis();
    Serial.printf("Клавиатура: Меню -> Выбран CH%d\n", selectedChannel);
  }
}

// Отрисовка данных всех 3-х каналов на экран 2.8"
void renderILI9341Display(float ch1_ma, float ch2_ma, float ch3_ma) {
  // Шапка
  tft.setTextSize(2);
  tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
  tft.setCursor(10, 5);
  tft.print("PN-2026 LIVE");

  tft.setTextSize(1);
  tft.setTextColor(ILI9341_GREEN, ILI9341_BLACK);
  tft.setCursor(195, 10);
  tft.print("IP: 192.168.4.1");

  tft.drawFastHLine(0, 26, 320, ILI9341_WHITE);

  float mm1 = maToMm(ch1_ma);
  float mm2 = maToMm(ch2_ma);
  float mm3 = maToMm(ch3_ma);

  // --- БЛОК КАНАЛА 1 ---
  uint16_t border1 = (selectedChannel == 1) ? ILI9341_CYAN : 0x2104;
  tft.drawRoundRect(5, 32, 310, 62, 6, border1);
  if (selectedChannel == 1) tft.drawRoundRect(6, 33, 308, 60, 5, ILI9341_CYAN);
  else tft.drawRoundRect(6, 33, 308, 60, 5, ILI9341_BLACK);

  tft.setTextSize(2);
  tft.setTextColor(ILI9341_CYAN, ILI9341_BLACK);
  tft.setCursor(15, 42);
  tft.print("CH1");

  tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
  tft.setCursor(75, 42);
  tft.printf("I: %5.2f mA", ch1_ma);
  tft.setCursor(75, 68);
  tft.printf("S: %+6.2f mm", mm1);
  float err1 = abs((mm1 - getNearestTarget(mm1)) / 4.0f) * 100.0f;
  tft.setCursor(220, 68);
  tft.printf("E:%4.1f%%", err1);

  // --- БЛОК КАНАЛА 2 ---
  uint16_t border2 = (selectedChannel == 2) ? ILI9341_YELLOW : 0x2104;
  tft.drawRoundRect(5, 100, 310, 62, 6, border2);
  if (selectedChannel == 2) tft.drawRoundRect(6, 101, 308, 60, 5, ILI9341_YELLOW);
  else tft.drawRoundRect(6, 101, 308, 60, 5, ILI9341_BLACK);

  tft.setTextSize(2);
  tft.setTextColor(ILI9341_YELLOW, ILI9341_BLACK);
  tft.setCursor(15, 110);
  tft.print("CH2");

  tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
  tft.setCursor(75, 110);
  tft.printf("I: %5.2f mA", ch2_ma);
  tft.setCursor(75, 136);
  tft.printf("S: %+6.2f mm", mm2);
  float err2 = abs((mm2 - getNearestTarget(mm2)) / 4.0f) * 100.0f;
  tft.setCursor(220, 136);
  tft.printf("E:%4.1f%%", err2);

  // --- БЛОК КАНАЛА 3 ---
  uint16_t border3 = (selectedChannel == 3) ? ILI9341_GREEN : 0x2104;
  tft.drawRoundRect(5, 168, 310, 62, 6, border3);
  if (selectedChannel == 3) tft.drawRoundRect(6, 169, 308, 60, 5, ILI9341_GREEN);
  else tft.drawRoundRect(6, 169, 308, 60, 5, ILI9341_BLACK);

  tft.setTextSize(2);
  tft.setTextColor(ILI9341_GREEN, ILI9341_BLACK);
  tft.setCursor(15, 178);
  tft.print("CH3");

  tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
  tft.setCursor(75, 178);
  tft.printf("I: %5.2f mA", ch3_ma);
  tft.setCursor(75, 204);
  tft.printf("S: %+6.2f mm", mm3);
  float err3 = abs((mm3 - getNearestTarget(mm3)) / 4.0f) * 100.0f;
  tft.setCursor(220, 204);
  tft.printf("E:%4.1f%%", err3);
}
#endif

// =============================================================================
// ОБРАБОТЧИК СОБЫТИЙ WEBSOCKET
// =============================================================================
void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
  if (type == WS_EVT_CONNECT) {
    Serial.printf("Клиент подключен: ID #%u\n", client->id());
  } else if (type == WS_EVT_DISCONNECT) {
    Serial.printf("Клиент отключен: ID #%u\n", client->id());
  }
}

void setup() {
  Serial.begin(115200);

  // Инициализация пинов кнопок клавиатуры
  pinMode(BTN_CH1, INPUT_PULLUP);
  pinMode(BTN_CH2, INPUT_PULLUP);
  pinMode(BTN_CH3, INPUT_PULLUP);
  pinMode(BTN_MENU, INPUT_PULLUP);

  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASS);
  Serial.print("Точка доступа: ");
  Serial.println(AP_SSID);
  Serial.print("IP адрес: ");
  Serial.println(WiFi.softAPIP());

  dnsServer.start(53, "*", WiFi.softAPIP());

  ws.onEvent(onWsEvent);
  server.addHandler(&ws);

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/html", INDEX_HTML);
  });
    auto redirectHandler = [](AsyncWebServerRequest *request){
      request->redirect("http://192.168.4.1/");
    };
    server.on("/hotspot-detect.html", HTTP_GET, redirectHandler);
    server.on("/canonical.html", HTTP_GET, redirectHandler);
    server.on("/generate_204", HTTP_GET, redirectHandler);
    server.on("/connecttest.txt", HTTP_GET, redirectHandler);
    server.on("/redirect", HTTP_GET, redirectHandler);
    server.on("/ncsi.txt", HTTP_GET, redirectHandler);

  
  
    server.on("/api/set_points", HTTP_GET, [](AsyncWebServerRequest *request){
      if(request->hasParam("pts")) {
        String ptsStr = request->getParam("pts")->value();
        int cnt = 0;
        int lastIndex = 0;
        for (int i = 0; i <= ptsStr.length(); i++) {
          if (i == ptsStr.length() || ptsStr.charAt(i) == ',') {
            if (cnt < 21) {
              targetPoints[cnt++] = ptsStr.substring(lastIndex, i).toFloat();
            }
            lastIndex = i + 1;
          }
        }
        targetPointsCount = cnt;
        preferences.begin("calib", false);
        preferences.putInt("ptCnt", targetPointsCount);
        preferences.putBytes("pts", targetPoints, sizeof(targetPoints));
        preferences.end();
        request->send(200, "text/plain", "OK");
      } else {
        request->send(400, "text/plain", "Missing pts");
      }
    });

    server.on("/calibrate", HTTP_GET, [](AsyncWebServerRequest *request){

    request->send(200, "text/html", CALIB_HTML);
  });
  server.on("/api/data", HTTP_GET, [](AsyncWebServerRequest *request){
    String json = "{";
    json += "\"adc_error\":" + String(adcFound ? "false" : "true") + ",";
    json += "\"r1\":" + String(raw_ma_1) + ",\"c1\":" + String(calib_ma_1) + ",";
    json += "\"r2\":" + String(raw_ma_2) + ",\"c2\":" + String(calib_ma_2) + ",";
    json += "\"r3\":" + String(raw_ma_3) + ",\"c3\":" + String(calib_ma_3) + ",";
    json += "\"m1\":" + String(calibMult1) + ",\"o1\":" + String(calibOffset1) + ",";
    json += "\"m2\":" + String(calibMult2) + ",\"o2\":" + String(calibOffset2) + ",";
    json += "\"m3\":" + String(calibMult3) + ",\"o3\":" + String(calibOffset3) + "}";
    request->send(200, "application/json", json);
  });
  server.on("/api/calibrate", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
    String body = "";
    for(size_t i=0; i<len; i++) body += (char)data[i];
    auto getVal = [&](String key, float def) -> float {
      int idx = body.indexOf("\"" + key + "\":");
      if (idx == -1) return def;
      idx += key.length() + 3;
      int endIdx = body.indexOf(",", idx);
      if (endIdx == -1) endIdx = body.indexOf("}", idx);
      return body.substring(idx, endIdx).toFloat();
    };
    calibMult1 = getVal("m1", calibMult1); calibOffset1 = getVal("o1", calibOffset1);
    calibMult2 = getVal("m2", calibMult2); calibOffset2 = getVal("o2", calibOffset2);
    calibMult3 = getVal("m3", calibMult3); calibOffset3 = getVal("o3", calibOffset3);
    preferences.begin("calib", false);
    preferences.putFloat("m1", calibMult1); preferences.putFloat("o1", calibOffset1);
    preferences.putFloat("m2", calibMult2); preferences.putFloat("o2", calibOffset2);
    preferences.putFloat("m3", calibMult3); preferences.putFloat("o3", calibOffset3);
    preferences.end();
    request->send(200, "text/plain", "OK");
  });

  server.onNotFound([](AsyncWebServerRequest *request){
    request->send(200, "text/html", INDEX_HTML);
  });

  
  
  preferences.begin("calib", true);
  calibMult1 = preferences.getFloat("m1", 1.0); calibOffset1 = preferences.getFloat("o1", 0.0);
  calibMult2 = preferences.getFloat("m2", 1.0); calibOffset2 = preferences.getFloat("o2", 0.0);
  calibMult3 = preferences.getFloat("m3", 1.0); calibOffset3 = preferences.getFloat("o3", 0.0);
  int savedCnt = preferences.getInt("ptCnt", 0);
  if (savedCnt > 0 && savedCnt <= 21) {
    targetPointsCount = savedCnt;
    preferences.getBytes("pts", targetPoints, sizeof(targetPoints));
  }
  preferences.end();


  Wire.begin(19, 21);

#if USE_REAL_ADS1115
  if (!ads.begin(0x48)) {
    Serial.println("{\"error\": \"ADS1115 not found!\"}");
    adcFound = false;
  } else {
    ads.setGain(GAIN_TWO);
    ads.setDataRate(RATE_ADS1115_860SPS);
    adcFound = true;
  }
#endif

#if USE_ILI9341_DISPLAY
  initILI9341Display();
#endif

  server.begin();
  Serial.println("Веб-сервер ПН-2026 запущен.");
}

unsigned long lastSend = 0;
float simAngle = 0.0;

void loop() {
  dnsServer.processNextRequest();
  ws.cleanupClients();

#if USE_ILI9341_DISPLAY
  checkButtonsInput();
#endif

  
  while (Serial.available() > 0) {
    String incoming = Serial.readStringUntil('\n');
    incoming.trim();
    if (incoming.length() > 0 && incoming.indexOf("trans_id") != -1) {
      int transIndex = incoming.indexOf("\"trans_id\":");
      if (transIndex != -1) {
        int startIndex = incoming.indexOf(':', transIndex) + 1;
        int endIndex = incoming.indexOf(',', startIndex);
        if (endIndex == -1) endIndex = incoming.indexOf('}', startIndex);
        String transIdStr = incoming.substring(startIndex, endIndex);
        transIdStr.trim();
        Serial.print("{\"ack\": "); Serial.print(transIdStr); Serial.println(", \"status\": \"SUCCESS\"}");
      }
    }
  }

  if (millis() - lastSend >= 50) {

    lastSend = millis();

    TelemetryPacket packet;

#if USE_REAL_ADS1115
    if (adcFound) {
      raw_ma_1 = (ads.readADC_SingleEnded(1) * 0.0625f) / 100.0f;
      raw_ma_2 = (ads.readADC_SingleEnded(2) * 0.0625f) / 100.0f;
      raw_ma_3 = (ads.readADC_SingleEnded(3) * 0.0625f) / 100.0f;
    } else {
      raw_ma_1 = 0; raw_ma_2 = 0; raw_ma_3 = 0;
    }
    
    if (filt_ch1 == 0.0f) filt_ch1 = raw_ma_1;
    if (filt_ch2 == 0.0f) filt_ch2 = raw_ma_2;
    if (filt_ch3 == 0.0f) filt_ch3 = raw_ma_3;

    filt_ch1 = filt_ch1 * 0.82f + raw_ma_1 * 0.18f;
    filt_ch2 = filt_ch2 * 0.82f + raw_ma_2 * 0.18f;
    filt_ch3 = filt_ch3 * 0.82f + raw_ma_3 * 0.18f;
    
    calib_ma_1 = (filt_ch1 * calibMult1) + calibOffset1;
    calib_ma_2 = (filt_ch2 * calibMult2) + calibOffset2;
    calib_ma_3 = (filt_ch3 * calibMult3) + calibOffset3;

    packet.ch1_ma = calib_ma_1;
    packet.ch2_ma = calib_ma_2;
    packet.ch3_ma = calib_ma_3;
#else
    simAngle += 0.06f;
    packet.ch1_ma = 12.0f + sin(simAngle) * 7.5f;
    packet.ch2_ma = 10.0f + cos(simAngle) * 3.0f;
    packet.ch3_ma = 8.5f  + sin(simAngle * 0.5f) * 1.5f;
#endif

    packet.r1_ohm = 1.35f;
    packet.r2_ohm = 4.82f;
    packet.gen_ma = currentGenMa;
    // Чтение напряжения батареи с пина 34
    // Предполагается использование делителя напряжения, например R1=100k (к плюсу АКБ), R2=10k (к земле).
    // Коэффициент делителя: 11.0. 
    // Опорное напряжение ESP32 обычно около 3.3В при 4095.
    int rawAdc = analogRead(34);
    // Калибровочный коэффициент для перевода сырых значений в Вольты. 
    // (3.3 / 4095) * 11.0 = 0.00886
    float volts = rawAdc * 0.00886f; 
    
    packet.batteryVolts = volts;
    // Расчет процента заряда для 3S Li-ion АКБ (9.6V = 0%, 12.6V = 100%)
    float pct = (volts - 9.6f) / (12.6f - 9.6f) * 100.0f;
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
    packet.batteryPct = (uint8_t)pct;
    packet.genActive = isGenActive;

    
    if (ws.count() > 0) {
      ws.binaryAll((uint8_t*)&packet, sizeof(packet));
    }
    
    // Выдача JSON для Flutter по USB
    Serial.print("{");
    Serial.print("\"ch1\":{\"ma\":"); Serial.print(packet.ch1_ma, 2); Serial.print(",\"mm\":0.0},");
    Serial.print("\"ch2\":{\"ma\":"); Serial.print(packet.ch2_ma, 2); Serial.print(",\"mm\":0.0},");
    Serial.print("\"ch3\":{\"ma\":"); Serial.print(packet.ch3_ma, 2); Serial.print(",\"mm\":0.0},");
    Serial.print("\"bat\":{\"volts\":"); Serial.print(packet.batteryVolts, 2); Serial.print(",\"percent\":"); Serial.print(packet.batteryPct); Serial.print("}");
    Serial.println("}");


#if USE_ILI9341_DISPLAY
    if (millis() - lastDisplayUpdate >= 120) {
      lastDisplayUpdate = millis();
      renderILI9341Display(packet.ch1_ma, packet.ch2_ma, packet.ch3_ma);
    }
#endif
  }
}
