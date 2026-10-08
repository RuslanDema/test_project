import re

with open(r'c:\Users\Yafarov_DS\Desktop\test_project\firmware\pn2026_esp32\pn2026_esp32.ino', 'r', encoding='utf-8') as f:
    code = f.read()

# 1. Add globals and getNearestTarget function
globals_code = """
float targetPoints[21] = {-2.0, -1.0, 0.0, 1.0, 2.0};
int targetPointsCount = 5;

float getNearestTarget(float mm) {
  if (targetPointsCount == 0) return round(mm);
  float best = targetPoints[0];
  float minDiff = abs(mm - best);
  for(int i=1; i<targetPointsCount; i++) {
    float diff = abs(mm - targetPoints[i]);
    if (diff < minDiff) {
      minDiff = diff;
      best = targetPoints[i];
    }
  }
  return best;
}

// END OF GLOBALS
"""
code = re.sub(r'float maToMm\(float ma\) {', globals_code + 'float maToMm(float ma) {', code)

# 2. Add endpoint to setup
endpoint_code = """
    server.on("/api/set_points", HTTP_GET, [](AsyncWebServerRequest *request){
      if(request->hasParam("pts")) {
        String ptsStr = request->getParam("pts")->value();
        int cnt = 0;
        int lastIndex = 0;
        for (int i = 0; i <= ptsStr.length(); i++) {
          if (i == ptsStr.length() || ptsStr.charAt(i) == ',') {
            if (cnt < 21) {
              targetPoints[cnt++] = ptsStr.substring(lastIndex, i).toFloat();
            }
            lastIndex = i + 1;
          }
        }
        targetPointsCount = cnt;
        preferences.begin("calib", false);
        preferences.putInt("ptCnt", targetPointsCount);
        preferences.putBytes("pts", targetPoints, sizeof(targetPoints));
        preferences.end();
        request->send(200, "text/plain", "OK");
      } else {
        request->send(400, "text/plain", "Missing pts");
      }
    });

    server.on("/calibrate", HTTP_GET, [](AsyncWebServerRequest *request){
"""
code = code.replace('server.on("/calibrate", HTTP_GET, [](AsyncWebServerRequest *request){', endpoint_code)

# 3. Add to Preferences load in setup
pref_load = """
  preferences.begin("calib", true);
  calibMult1 = preferences.getFloat("m1", 1.0); calibOffset1 = preferences.getFloat("o1", 0.0);
  calibMult2 = preferences.getFloat("m2", 1.0); calibOffset2 = preferences.getFloat("o2", 0.0);
  calibMult3 = preferences.getFloat("m3", 1.0); calibOffset3 = preferences.getFloat("o3", 0.0);
  int savedCnt = preferences.getInt("ptCnt", 0);
  if (savedCnt > 0 && savedCnt <= 21) {
    targetPointsCount = savedCnt;
    preferences.getBytes("pts", targetPoints, sizeof(targetPoints));
  }
  preferences.end();
"""
code = re.sub(r'preferences\.begin\("calib", true\);[\s\S]*?preferences\.end\(\);', pref_load, code, count=1)

# 4. Update JS savePresets in INDEX_HTML
js_old = """    function savePresets() {
      localStorage.setItem('pn2026_preset', JSON.stringify({
        sMin: S_MIN, sMax: S_MAX, sRef: S_REF,
        ptArray: ptArray
      }));
    }"""
js_new = """    function savePresets() {
      localStorage.setItem('pn2026_preset', JSON.stringify({
        sMin: S_MIN, sMax: S_MAX, sRef: S_REF,
        ptArray: ptArray
      }));
      fetch('/api/set_points?pts=' + ptArray.join(',')).catch(e=>console.log(e));
    }"""
code = code.replace(js_old, js_new)

# 5. Update renderILI9341Display to use getNearestTarget
code = code.replace('round(mm1)', 'getNearestTarget(mm1)')
code = code.replace('round(mm2)', 'getNearestTarget(mm2)')
code = code.replace('round(mm3)', 'getNearestTarget(mm3)')

with open(r'c:\Users\Yafarov_DS\Desktop\test_project\firmware\pn2026_esp32\pn2026_esp32.ino', 'w', encoding='utf-8') as f:
    f.write(code)

print("Sync points patch applied!")
