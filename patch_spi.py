import re

with open(r'c:\Users\Yafarov_DS\Desktop\test_project\firmware\pn2026_esp32\pn2026_esp32.ino', 'r', encoding='utf-8') as f:
    code = f.read()

code = re.sub(r'#if USE_ILI9341_DISPLAY\n\s*initILI9341Display\(\);\n#endif', '#if USE_ILI9341_DISPLAY\n  SPI.begin(TFT_CLK, -1, TFT_MOSI, TFT_CS);\n  initILI9341Display();\n#endif', code)

with open(r'c:\Users\Yafarov_DS\Desktop\test_project\firmware\pn2026_esp32\pn2026_esp32.ino', 'w', encoding='utf-8') as f:
    f.write(code)

print("Injected SPI.begin!")
