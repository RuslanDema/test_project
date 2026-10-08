import re

with open(r'c:\Users\Yafarov_DS\Desktop\test_project\firmware\pn2026_esp32\pn2026_esp32.ino', 'r', encoding='utf-8') as f:
    code = f.read()

code = re.sub(r'const d1 = calcDeltaRE100\(ch1_ma, S_REF\);', 'const d1 = calcDeltaRE100(ch1_ma, getNearestTargetJS(mm1));', code)
code = re.sub(r'const d2 = calcDeltaRE100\(ch2_ma, S_REF\);', 'const d2 = calcDeltaRE100(ch2_ma, getNearestTargetJS(mm2));', code)
code = re.sub(r'const d3 = calcDeltaRE100\(ch3_ma, S_REF\);', 'const d3 = calcDeltaRE100(ch3_ma, getNearestTargetJS(mm3));', code)

with open(r'c:\Users\Yafarov_DS\Desktop\test_project\firmware\pn2026_esp32\pn2026_esp32.ino', 'w', encoding='utf-8') as f:
    f.write(code)

print("JS live delta patch FORCEFULLY applied!")
