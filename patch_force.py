import re

with open(r'c:\Users\Yafarov_DS\Desktop\test_project\firmware\pn2026_esp32\pn2026_esp32.ino', 'r', encoding='utf-8') as f:
    code = f.read()

# Force replace defines
code = re.sub(r'#define TFT_CS\s+.*', '#define TFT_CS   17', code)
code = re.sub(r'#define TFT_RST\s+.*', '#define TFT_RST  2', code)
code = re.sub(r'#define TFT_DC\s+.*', '#define TFT_DC   15', code)
code = re.sub(r'#define TFT_MOSI\s+.*', '#define TFT_MOSI 22', code)
code = re.sub(r'#define TFT_CLK\s+.*', '#define TFT_CLK  5', code)

# Force replace constructor
code = re.sub(r'Adafruit_ILI9341 tft = .*?;', 'Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_MOSI, TFT_CLK, TFT_RST);', code)

# Remove SPI.begin if it exists
code = re.sub(r'\s*SPI\.begin\(.*?\);', '', code)

with open(r'c:\Users\Yafarov_DS\Desktop\test_project\firmware\pn2026_esp32\pn2026_esp32.ino', 'w', encoding='utf-8') as f:
    f.write(code)

print("Forced replacement successful!")
