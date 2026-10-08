import re

with open(r'c:\Users\Yafarov_DS\Desktop\test_project\firmware\pn2026_esp32\pn2026_esp32.ino', 'r', encoding='utf-8') as f:
    code = f.read()

adc_setup_new = """#if USE_REAL_ADS1115
  if (!ads.begin(0x48)) {
    Serial.println("{\\"error\\": \\"ADS1115 not found!\\"}");
    adcFound = false;
  } else {
    ads.setGain(GAIN_TWO);
    ads.setDataRate(RATE_ADS1115_860SPS);
    adcFound = true;
  }
#endif"""

code = re.sub(r'#if USE_REAL_ADS1115\n  if \(!ads\.begin\(0x48\)\).*?#endif', adc_setup_new, code, flags=re.DOTALL)

with open(r'c:\Users\Yafarov_DS\Desktop\test_project\firmware\pn2026_esp32\pn2026_esp32.ino', 'w', encoding='utf-8') as f:
    f.write(code)

print("Patch applied for ADC")
