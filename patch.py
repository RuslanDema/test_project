import re

with open(r'c:\Users\Yafarov_DS\Desktop\test_project\firmware\old_pn2026.ino', 'r', encoding='utf-8') as f:
    code = f.read()

# Restore original code
includes = """#include <Preferences.h>

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
"""

code = code.replace("#include <Adafruit_ILI9341.h>", "#include <Adafruit_ILI9341.h>\n" + includes)

api_code = """
  server.on("/calibrate", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/html", CALIB_HTML);
  });
  server.on("/api/data", HTTP_GET, [](AsyncWebServerRequest *request){
    String json = "{";
    json += "\\"adc_error\\":" + String(adcFound ? "false" : "true") + ",";
    json += "\\"r1\\":" + String(raw_ma_1) + ",\\"c1\\":" + String(calib_ma_1) + ",";
    json += "\\"r2\\":" + String(raw_ma_2) + ",\\"c2\\":" + String(calib_ma_2) + ",";
    json += "\\"r3\\":" + String(raw_ma_3) + ",\\"c3\\":" + String(calib_ma_3) + ",";
    json += "\\"m1\\":" + String(calibMult1) + ",\\"o1\\":" + String(calibOffset1) + ",";
    json += "\\"m2\\":" + String(calibMult2) + ",\\"o2\\":" + String(calibOffset2) + ",";
    json += "\\"m3\\":" + String(calibMult3) + ",\\"o3\\":" + String(calibOffset3) + "}";
    request->send(200, "application/json", json);
  });
  server.on("/api/calibrate", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
    String body = "";
    for(size_t i=0; i<len; i++) body += (char)data[i];
    auto getVal = [&](String key, float def) -> float {
      int idx = body.indexOf("\\"" + key + "\\":");
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
"""
code = code.replace("server.onNotFound", api_code + "\n  server.onNotFound")

setup_inject = """
  preferences.begin("calib", true);
  calibMult1 = preferences.getFloat("m1", 1.0); calibOffset1 = preferences.getFloat("o1", 0.0);
  calibMult2 = preferences.getFloat("m2", 1.0); calibOffset2 = preferences.getFloat("o2", 0.0);
  calibMult3 = preferences.getFloat("m3", 1.0); calibOffset3 = preferences.getFloat("o3", 0.0);
  preferences.end();
"""
code = code.replace("Wire.begin(21, 22);", setup_inject + "\n  Wire.begin(21, 22);")

# Safely replace only the setup part of ADS
ads_setup_original = """  if (!ads.begin(0x48)) {
    Serial.println("Ошибка : ADS1115 не найден на I2C (0x48)!");
  } else {
    ads.setGain(GAIN_TWO);
    ads.setDataRate(RATE_ADS1115_860SPS);
    Serial.println("ADS1115 найден и настроен (PGA=2.048V, 860 SPS)");
  }"""
ads_setup_new = """  if (!ads.begin(0x48)) {
    Serial.println("{\\"error\\": \\"ADS1115 not found!\\"}");
    adcFound = false;
  } else {
    ads.setGain(GAIN_TWO);
    ads.setDataRate(RATE_ADS1115_860SPS);
    adcFound = true;
  }"""
code = code.replace(ads_setup_original, ads_setup_new)

# Loop replacement
loop_read_original = """    float raw1 = (ads.readADC_SingleEnded(1) * 0.0625f) / 100.0f;
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
    packet.ch3_ma = filt_ch3;"""

loop_read_new = """    if (adcFound) {
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
    packet.ch3_ma = calib_ma_3;"""

code = code.replace(loop_read_original, loop_read_new)

serial_logic = """
  while (Serial.available() > 0) {
    String incoming = Serial.readStringUntil('\\n');
    incoming.trim();
    if (incoming.length() > 0 && incoming.indexOf("trans_id") != -1) {
      int transIndex = incoming.indexOf("\\"trans_id\\":");
      if (transIndex != -1) {
        int startIndex = incoming.indexOf(':', transIndex) + 1;
        int endIndex = incoming.indexOf(',', startIndex);
        if (endIndex == -1) endIndex = incoming.indexOf('}', startIndex);
        String transIdStr = incoming.substring(startIndex, endIndex);
        transIdStr.trim();
        Serial.print("{\\"ack\\": "); Serial.print(transIdStr); Serial.println(", \\"status\\": \\"SUCCESS\\"}");
      }
    }
  }

  if (millis() - lastSend >= 50) {
"""
code = code.replace("if (millis() - lastSend >= 50) {", serial_logic)

json_out = """
    if (ws.count() > 0) {
      ws.binaryAll((uint8_t*)&packet, sizeof(packet));
    }
    
    // Выдача JSON для Flutter по USB
    Serial.print("{");
    Serial.print("\\"ch1\\":{\\"ma\\":"); Serial.print(packet.ch1_ma, 2); Serial.print(",\\"mm\\":0.0},");
    Serial.print("\\"ch2\\":{\\"ma\\":"); Serial.print(packet.ch2_ma, 2); Serial.print(",\\"mm\\":0.0},");
    Serial.print("\\"ch3\\":{\\"ma\\":"); Serial.print(packet.ch3_ma, 2); Serial.print(",\\"mm\\":0.0},");
    Serial.print("\\"bat\\":{\\"volts\\":"); Serial.print(packet.batteryVolts, 2); Serial.print(",\\"percent\\":"); Serial.print(packet.batteryPct); Serial.print("}");
    Serial.println("}");
"""
code = code.replace("if (ws.count() > 0) {\n      ws.binaryAll((uint8_t*)&packet, sizeof(packet));\n    }", json_out)

with open(r'c:\Users\Yafarov_DS\Desktop\test_project\firmware\pn2026_esp32\pn2026_esp32.ino', 'w', encoding='utf-8') as f:
    f.write(code)

print("Patch applied carefully!")
