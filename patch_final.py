import re

with open(r'c:\Users\Yafarov_DS\Desktop\test_project\firmware\pn2026_esp32\pn2026_esp32.ino', 'r', encoding='utf-8') as f:
    code = f.read()

# 1. Revert display pins
code = code.replace('#define TFT_CS    17', '#define TFT_CS    5')
code = code.replace('#define TFT_DC    15', '#define TFT_DC    2')
code = code.replace('#define TFT_MOSI  22', '#define TFT_MOSI 23')
code = code.replace('#define TFT_CLK   5', '#define TFT_CLK  18')
code = code.replace('Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_MOSI, TFT_CLK, TFT_RST, -1);', 'Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_RST);')

# 2. Add button to main page HTML
# The main page has: <button class="btn-browser-guide" onclick="showBrowserGuide()">📲 В браузер</button>
# I will add a Calibration button next to it.
btn_calib = '<button class="btn-browser-guide" onclick="location.href=\'/calibrate\'" style="background:#00e5ff; color:#000;">⚙️ КАЛИБРОВКА</button>'
code = code.replace('<button class="btn-browser-guide" onclick="showBrowserGuide()">📲 В браузер</button>', btn_calib + '\n      <button class="btn-browser-guide" onclick="showBrowserGuide()">📲 В браузер</button>')

with open(r'c:\Users\Yafarov_DS\Desktop\test_project\firmware\pn2026_esp32\pn2026_esp32.ino', 'w', encoding='utf-8') as f:
    f.write(code)

print("Display reverted and button added!")
