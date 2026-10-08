import re

with open(r'c:\Users\Yafarov_DS\Desktop\test_project\firmware\pn2026_esp32\pn2026_esp32.ino', 'r', encoding='utf-8') as f:
    code = f.read()

# Change pins back to user's physical wiring
code = code.replace('#define TFT_CS    5', '#define TFT_CS    17')
code = code.replace('#define TFT_DC    2', '#define TFT_DC    15')
code = code.replace('#define TFT_MOSI 23', '#define TFT_MOSI  22')
code = code.replace('#define TFT_CLK  18', '#define TFT_CLK   5')

# Change the constructor to accept &SPI so it doesn't overwrite pins
code = code.replace('Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_RST);', 'Adafruit_ILI9341 tft = Adafruit_ILI9341(&SPI, TFT_DC, TFT_CS, TFT_RST);')

# Inject SPI.begin into setup right before initILI9341Display
code = code.replace('initILI9341Display();', 'SPI.begin(TFT_CLK, -1, TFT_MOSI, TFT_CS);\n  initILI9341Display();')

with open(r'c:\Users\Yafarov_DS\Desktop\test_project\firmware\pn2026_esp32\pn2026_esp32.ino', 'w', encoding='utf-8') as f:
    f.write(code)

print("Display pins correctly remapped!")
