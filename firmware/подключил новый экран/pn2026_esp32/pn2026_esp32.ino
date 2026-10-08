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

// =============================================================================
// НАСТРОЙКИ ОБОРУДОВАНИЯ
// =============================================================================
#define USE_REAL_ADS1115     1   // 1 = реальный замер с ADS1115, 0 = симуляция
#define USE_ILI9341_DISPLAY  1   // 1 = включить 2.8" TFT ILI9341 экран

// Пины подключения ILI9341 (SPI)
#define TFT_CS    5
#define TFT_RST   4
#define TFT_DC    2
#define TFT_MOSI 23
#define TFT_CLK  18

// Пины 4-кнопочной мембранной клавиатуры
#define BTN_CH1  13   // Кнопка 1 (Канал CH1)
#define BTN_CH2  12   // Кнопка 2 (Канал CH2)
#define BTN_CH3  14   // Кнопка 3 (Канал CH3)
#define BTN_MENU 27   // Кнопка 4 (Меню / Настройка / Выбор)

#if USE_REAL_ADS1115
Adafruit_ADS1115 ads; // I2C адрес 0x48
#endif

#if USE_ILI9341_DISPLAY
Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_RST);
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

  <!-- Навигация -->
  <div class="nav-bar">
    <button class="nav-item active" onclick="switchNav('tab-live', this)">
      <span class="nav-icon">⚡</span><span>Режим Live</span>
    </button>
    <button class="nav-item" onclick="switchNav('tab-coils', this)">
      <span class="nav-icon">🧲</span><span>Поверка катушек</span>
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

    loadPresets();
    generatePointsTable();
    updateCoilNominals();

    let ws;
    function connectWS() {
      const badge = document.getElementById('badgeWs');
      const host = window.location.hostname || "192.168.4.1";
      ws = new WebSocket(`ws://${host}/ws`);
      ws.binaryType = "arraybuffer";

      ws.onopen = () => {
        badge.innerText = "ОНЛАЙН";
        badge.className = "badge online";
      };
      ws.onclose = () => {
        badge.innerText = "ОФФЛАЙН";
        badge.className = "badge offline";
        setTimeout(connectWS, 1500);
      };

      ws.onmessage = (evt) => {
        if (!(evt.data instanceof ArrayBuffer) || evt.data.byteLength < 28) return;
        const dv = new DataView(evt.data);

        const ch1_ma = dv.getFloat32(0, true);
        const ch2_ma = dv.getFloat32(4, true);
        const ch3_ma = dv.getFloat32(8, true);
        lastRawR1 = dv.getFloat32(12, true);
        lastRawR2 = dv.getFloat32(16, true);
        const batt_v = dv.getFloat32(24, true);
        const batt_pct = dv.getUint8(28);

        lastLiveMa1 = ch1_ma;
        lastLiveMa2 = ch2_ma;
        lastLiveMa3 = ch3_ma;

        const mm1 = maToMm(ch1_ma);
        const mm2 = maToMm(ch2_ma);
        const mm3 = maToMm(ch3_ma);

        const d1 = calcDeltaRE100(ch1_ma, S_REF);
        const d2 = calcDeltaRE100(ch2_ma, S_REF);
        const d3 = calcDeltaRE100(ch3_ma, S_REF);

        document.getElementById('ch1_ma_text').innerText = `${ch1_ma.toFixed(2)} мА`;
        document.getElementById('ch1_mm_text').innerText = `${mm1 > 0 ? '+' : ''}${mm1.toFixed(2)} мм`;
        document.getElementById('ch1_gamma_text').innerText = `δ: ${d1.toFixed(1)}%`;
        updateDot('ch1_dot', d1);
        document.getElementById('ch1_adv').innerHTML = generateAdvisorMsg(1, ch1_ma, mm1, d1);

        document.getElementById('ch2_ma_text').innerText = `${ch2_ma.toFixed(2)} мА`;
        document.getElementById('ch2_mm_text').innerText = `${mm2 > 0 ? '+' : ''}${mm2.toFixed(2)} мм`;
        document.getElementById('ch2_gamma_text').innerText = `δ: ${d2.toFixed(1)}%`;
        updateDot('ch2_dot', d2);
        document.getElementById('ch2_adv').innerHTML = generateAdvisorMsg(2, ch2_ma, mm2, d2);

        document.getElementById('ch3_ma_text').innerText = `${ch3_ma.toFixed(2)} мА`;
        document.getElementById('ch3_mm_text').innerText = `${mm3 > 0 ? '+' : ''}${mm3.toFixed(2)} мм`;
        document.getElementById('ch3_gamma_text').innerText = `δ: ${d3.toFixed(1)}%`;
        updateDot('ch3_dot', d3);
        document.getElementById('ch3_adv').innerHTML = generateAdvisorMsg(3, ch3_ma, mm3, d3);

        document.getElementById('badgeBat').innerText = `АКБ: ${batt_pct}% (${batt_v.toFixed(1)}В)`;

        updateCoilNominals();
      };
    }

    function updateDot(id, delta) {
      const el = document.getElementById(id);
      if (delta <= 1.5) {
        el.className = 'status-dot dot-green';
      } else if (delta < 2.5) {
        el.className = 'status-dot dot-yellow';
      } else {
        el.className = 'status-dot dot-red';
      }
    }

    connectWS();
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

  server.on("/hotspot-detect.html", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/html", INDEX_HTML);
  });
  server.on("/canonical.html", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/html", INDEX_HTML);
  });
  server.on("/generate_204", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/html", INDEX_HTML);
  });
  server.on("/connecttest.txt", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/html", INDEX_HTML);
  });

  server.onNotFound([](AsyncWebServerRequest *request){
    request->send(200, "text/html", INDEX_HTML);
  });

  Wire.begin(21, 22);

#if USE_REAL_ADS1115
  if (!ads.begin(0x48)) {
    Serial.println("ВНИМАНИЕ: ADS1115 не найден на I2C (0x48)!");
  } else {
    ads.setGain(GAIN_TWO);
    ads.setDataRate(RATE_ADS1115_860SPS);
    Serial.println("ADS1115 готов к работе (PGA=2.048V, 860 SPS)");
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

  if (millis() - lastSend >= 50) {
    lastSend = millis();

    TelemetryPacket packet;

#if USE_REAL_ADS1115
    float raw1 = (ads.readADC_SingleEnded(1) * 0.0625f) / 100.0f;
    float raw2 = (ads.readADC_SingleEnded(2) * 0.0625f) / 100.0f;
    float raw3 = (ads.readADC_SingleEnded(3) * 0.0625f) / 100.0f;

    if (filt_ch1 == 0.0f) filt_ch1 = raw1;
    if (filt_ch2 == 0.0f) filt_ch2 = raw2;
    if (filt_ch3 == 0.0f) filt_ch3 = raw3;

    filt_ch1 = filt_ch1 * 0.82f + raw1 * 0.18f;
    filt_ch2 = filt_ch2 * 0.82f + raw2 * 0.18f;
    filt_ch3 = filt_ch3 * 0.82f + raw3 * 0.18f;

    packet.ch1_ma = filt_ch1;
    packet.ch2_ma = filt_ch2;
    packet.ch3_ma = filt_ch3;
#else
    simAngle += 0.06f;
    packet.ch1_ma = 12.0f + sin(simAngle) * 7.5f;
    packet.ch2_ma = 10.0f + cos(simAngle) * 3.0f;
    packet.ch3_ma = 8.5f  + sin(simAngle * 0.5f) * 1.5f;
#endif

    packet.r1_ohm = 1.35f;
    packet.r2_ohm = 4.82f;
    packet.gen_ma = currentGenMa;
    packet.batteryVolts = 11.9f;
    packet.batteryPct = 88;
    packet.genActive = isGenActive;

    if (ws.count() > 0) {
      ws.binaryAll((uint8_t*)&packet, sizeof(packet));
    }

#if USE_ILI9341_DISPLAY
    if (millis() - lastDisplayUpdate >= 120) {
      lastDisplayUpdate = millis();
      renderILI9341Display(packet.ch1_ma, packet.ch2_ma, packet.ch3_ma);
    }
#endif
  }
}
