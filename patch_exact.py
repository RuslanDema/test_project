import re

with open(r'c:\Users\Yafarov_DS\Desktop\test_project\firmware\pn2026_esp32\pn2026_esp32.ino', 'r', encoding='utf-8') as f:
    code = f.read()

# I will replace the current TFT defines and constructor with the exactly recovered ones.
old_block = """#define TFT_CS    5
#define TFT_RST   4
#define TFT_DC    2
#define TFT_MOSI 23
#define TFT_CLK  18

#if USE_ILI9341_DISPLAY
Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_RST);
#endif"""

new_block = """#define TFT_CS   17
#define TFT_DC   15
#define TFT_MOSI 22
#define TFT_CLK  5
#define TFT_RST  2

#if USE_ILI9341_DISPLAY
Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_MOSI, TFT_CLK, TFT_RST);
#endif"""

code = code.replace(old_block, new_block)

with open(r'c:\Users\Yafarov_DS\Desktop\test_project\firmware\pn2026_esp32\pn2026_esp32.ino', 'w', encoding='utf-8') as f:
    f.write(code)

print("Restored exact basic firmware pins!")
