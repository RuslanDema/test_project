import re

with open(r'c:\Users\Yafarov_DS\Desktop\test_project\firmware\pn2026_esp32\pn2026_esp32.ino', 'r', encoding='utf-8') as f:
    code = f.read()

# For CH1
old_ch1 = 'tft.printf("S: %+6.2f mm", mm1);'
new_ch1 = 'tft.printf("S: %+6.2f mm", mm1);\n  float err1 = abs((mm1 - round(mm1)) / 4.0f) * 100.0f;\n  tft.setCursor(220, 68);\n  tft.printf("E:%4.1f%%", err1);'
code = code.replace(old_ch1, new_ch1)

# For CH2
old_ch2 = 'tft.printf("S: %+6.2f mm", mm2);'
new_ch2 = 'tft.printf("S: %+6.2f mm", mm2);\n  float err2 = abs((mm2 - round(mm2)) / 4.0f) * 100.0f;\n  tft.setCursor(220, 136);\n  tft.printf("E:%4.1f%%", err2);'
code = code.replace(old_ch2, new_ch2)

# For CH3 (it's similar)
old_ch3 = 'tft.printf("S: %+6.2f mm", mm3);'
new_ch3 = 'tft.printf("S: %+6.2f mm", mm3);\n  float err3 = abs((mm3 - round(mm3)) / 4.0f) * 100.0f;\n  tft.setCursor(220, 204);\n  tft.printf("E:%4.1f%%", err3);'
code = code.replace(old_ch3, new_ch3)

with open(r'c:\Users\Yafarov_DS\Desktop\test_project\firmware\pn2026_esp32\pn2026_esp32.ino', 'w', encoding='utf-8') as f:
    f.write(code)

print("Errors added to display!")
