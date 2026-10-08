import re

with open(r'c:\Users\Yafarov_DS\Desktop\test_project\firmware\pn2026_esp32\pn2026_esp32.ino', 'r', encoding='utf-8') as f:
    code = f.read()

old_bat = """    // Расчет процента заряда (примерно для 12V свинцового АКБ: 10.5V = 0%, 12.6V = 100%)
    float pct = (volts - 10.5f) / (12.6f - 10.5f) * 100.0f;"""

new_bat = """    // Расчет процента заряда для 3S Li-ion АКБ (9.6V = 0%, 12.6V = 100%)
    float pct = (volts - 9.6f) / (12.6f - 9.6f) * 100.0f;"""

code = code.replace(old_bat, new_bat)

with open(r'c:\Users\Yafarov_DS\Desktop\test_project\firmware\pn2026_esp32\pn2026_esp32.ino', 'w', encoding='utf-8') as f:
    f.write(code)

print("Li-ion 3S patch applied!")
