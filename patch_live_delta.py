import re

with open(r'c:\Users\Yafarov_DS\Desktop\test_project\firmware\pn2026_esp32\pn2026_esp32.ino', 'r', encoding='utf-8') as f:
    code = f.read()

# Add getNearestTargetJS to Javascript
js_func = """
      function getNearestTargetJS(mm) {
        if (!ptArray || ptArray.length === 0) return Math.round(mm);
        let best = ptArray[0];
        let minDiff = Math.abs(mm - best);
        for (let i = 1; i < ptArray.length; i++) {
          let diff = Math.abs(mm - ptArray[i]);
          if (diff < minDiff) {
            minDiff = diff;
            best = ptArray[i];
          }
        }
        return best;
      }
      function calcDeltaRE100(ma, si_mm) {
"""

code = code.replace("function calcDeltaRE100(ma, si_mm) {", js_func)

# Replace S_REF with nearest target in live section
old_live = """
          const d1 = calcDeltaRE100(ch1_ma, S_REF);
          const d2 = calcDeltaRE100(ch2_ma, S_REF);
          const d3 = calcDeltaRE100(ch3_ma, S_REF);
"""

new_live = """
          const d1 = calcDeltaRE100(ch1_ma, getNearestTargetJS(mm1));
          const d2 = calcDeltaRE100(ch2_ma, getNearestTargetJS(mm2));
          const d3 = calcDeltaRE100(ch3_ma, getNearestTargetJS(mm3));
"""
code = code.replace(old_live, new_live)

with open(r'c:\Users\Yafarov_DS\Desktop\test_project\firmware\pn2026_esp32\pn2026_esp32.ino', 'w', encoding='utf-8') as f:
    f.write(code)

print("JS live delta patch applied!")
