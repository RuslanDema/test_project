import re

with open(r'c:\Users\Yafarov_DS\Desktop\test_project\firmware\pn2026_esp32\pn2026_esp32.ino', 'r', encoding='utf-8') as f:
    code = f.read()

# Replace hardcoded battery with analogRead
old_bat = """    packet.batteryVolts = 11.9f;
    packet.batteryPct = 88;"""

new_bat = """    // Чтение напряжения батареи с пина 34
    // Предполагается использование делителя напряжения, например R1=100k (к плюсу АКБ), R2=10k (к земле).
    // Коэффициент делителя: 11.0. 
    // Опорное напряжение ESP32 обычно около 3.3В при 4095.
    int rawAdc = analogRead(34);
    // Калибровочный коэффициент для перевода сырых значений в Вольты. 
    // (3.3 / 4095) * 11.0 = 0.00886
    float volts = rawAdc * 0.00886f; 
    
    packet.batteryVolts = volts;
    // Расчет процента заряда (примерно для 12V свинцового АКБ: 10.5V = 0%, 12.6V = 100%)
    float pct = (volts - 10.5f) / (12.6f - 10.5f) * 100.0f;
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
    packet.batteryPct = (uint8_t)pct;"""

code = code.replace(old_bat, new_bat)

with open(r'c:\Users\Yafarov_DS\Desktop\test_project\firmware\pn2026_esp32\pn2026_esp32.ino', 'w', encoding='utf-8') as f:
    f.write(code)

print("Battery patch applied!")
