import re
import os

WEB_DIR = "web_dev"
HTML_PATH = os.path.join(WEB_DIR, "index.html")

with open(HTML_PATH, "r", encoding="utf-8") as f:
    code = f.read()

new_tab_protocol = """
  <!-- ==================== ЭКРАН 3: ПРОТОКОЛ (ГОСТ) ==================== -->
  <div id="tab-protocol" class="tab-view">
    <div class="coil-card">
      <div class="coil-title">Настройка приборов для протокола</div>
      
      <div class="form-row">
        <div class="form-col">
          <span class="form-lbl">Кол-во приборов в протоколе:</span>
          <select id="protCount" class="form-sel" onchange="buildProtConfig()">
            <option value="1">1 канал (Только CH1)</option>
            <option value="2">2 канала (CH1, CH2)</option>
            <option value="3" selected>3 канала (CH1, CH2, CH3)</option>
          </select>
        </div>
      </div>
      
      <div id="protConfigArea"></div>
    </div>

    <div class="coil-card">
      <div class="coil-title">Прогонка и Формирование</div>
      <div class="progonka-controls">
        <button id="btnProtProgonka" class="btn-progonka" onclick="doProtStepGOST()">▶ Записать точку 1</button>
        <button class="btn-undo" onclick="undoProtStepGOST()">↩</button>
      </div>

      <div style="overflow-x: auto; margin-top: 10px;">
        <table class="table-points" id="tblProtPointsGOST">
        </table>
      </div>

      <button class="btn-calibr" style="background:var(--ch1); color:#000; margin-top:15px; font-size: 14px;" onclick="generateDocGOST()">
        📝 СФОРМИРОВАТЬ ПРОТОКОЛ (Excel/Word ГОСТ)
      </button>
    </div>
  </div>
"""

# Replace HTML
code = re.sub(r'<!-- ==================== ЭКРАН 3: ПРОТОКОЛ.*?<!-- Навигация -->', new_tab_protocol + '\n  <!-- Навигация -->', code, flags=re.DOTALL)

with open(HTML_PATH, "w", encoding="utf-8") as f:
    f.write(code)

print("HTML V3 Patch applied!")
