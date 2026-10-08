import re

with open(r'c:\Users\Yafarov_DS\Desktop\test_project\firmware\pn2026_esp32\pn2026_esp32.ino', 'r', encoding='utf-8') as f:
    code = f.read()

code = code.replace('Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_RST);', 'Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_MOSI, TFT_CLK, TFT_RST, -1);')

# Also remove SPI.begin from setup since Software SPI doesn't need it
code = re.sub(r'SPI\.begin\(TFT_CLK, -1, TFT_MOSI, TFT_CS\);\n\s*', '', code)

with open(r'c:\Users\Yafarov_DS\Desktop\test_project\firmware\pn2026_esp32\pn2026_esp32.ino', 'w', encoding='utf-8') as f:
    f.write(code)

print("Switched to Software SPI!")
