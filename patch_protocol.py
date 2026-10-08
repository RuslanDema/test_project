import re
import os

WEB_DIR = "web_dev"
HTML_PATH = os.path.join(WEB_DIR, "index.html")

with open(HTML_PATH, "r", encoding="utf-8") as f:
    code = f.read()

# 1. ADD CSS
css_patch = """
    /* === ПРОТОКОЛ === */
    .prot-header { font-size: 13px; font-weight: 800; color: var(--ch1); margin-top: 10px; margin-bottom: 5px; }
    .channel-select { display: flex; gap: 4px; margin-bottom: 10px; }
    .ch-btn { flex: 1; padding: 8px; border-radius: 6px; border: 1px solid var(--border); background: #06090e; color: var(--subtext); font-weight: 800; cursor: pointer; }
    .ch-btn.active { background: var(--ch1); color: #000; }
"""
code = code.replace("</style>", css_patch + "</style>")

# 2. ADD TAB CONTENT
tab_patch = """
  <!-- ==================== ЭКРАН 3: ПРОТОКОЛ ==================== -->
  <div id="tab-protocol" class="tab-view">
    <div class="coil-card">
      <div class="coil-title">Настройки протокола</div>
      
      <div class="prot-header">Канал измерения:</div>
      <div class="channel-select">
        <button id="btnProtCh1" class="ch-btn active" onclick="setProtChannel(1)">CH1</button>
        <button id="btnProtCh2" class="ch-btn" onclick="setProtChannel(2)">CH2</button>
        <button id="btnProtCh3" class="ch-btn" onclick="setProtChannel(3)">CH3</button>
      </div>

      <div class="form-row">
        <div class="form-col">
          <span class="form-lbl">Тип датчика:</span>
          <select id="protSensorType" class="form-sel" onchange="updateProtInfo()">
            <!-- will be populated by JS -->
          </select>
        </div>
        <div class="form-col">
          <span class="form-lbl">Длина кабеля (м):</span>
          <input type="number" id="protCableLen" class="form-inp" value="5.0" onchange="updateProtInfo()">
        </div>
      </div>
      
      <div class="form-row">
        <div class="form-col">
          <span class="form-lbl">R возб. Wp (Ом):</span>
          <input type="number" id="protR1" class="form-inp" step="0.01">
        </div>
        <div class="form-col" id="colProtR2">
          <span class="form-lbl">R сигн. Wc (Ом):</span>
          <input type="number" id="protR2" class="form-inp" step="0.01">
        </div>
      </div>
      <button class="btn-calibr" style="margin-top:2px; padding:6px; font-size:10px;" onclick="captureProtR()">Захватить текущие R</button>
    </div>

    <div class="coil-card">
      <div class="coil-title">Прогонка для протокола</div>
      
      <div class="progonka-controls">
        <button id="btnProtProgonka" class="btn-progonka" onclick="doProtStep()">
          ▶ Записать точку 1
        </button>
        <button class="btn-undo" onclick="undoProtStep()">↩</button>
      </div>

      <table class="table-points">
        <thead>
          <tr>
            <th>№</th>
            <th>Ход (мм)</th>
            <th>I расч</th>
            <th style="color:var(--ch1)">I факт</th>
            <th style="color:var(--ch1)">δ (%)</th>
          </tr>
        </thead>
        <tbody id="tblProtPointsBody">
        </tbody>
      </table>

      <button class="btn-calibr" style="background:var(--ch1); color:#000; margin-top:15px; font-size: 14px;" onclick="generateDoc()">
        📝 СФОРМИРОВАТЬ ПРОТОКОЛ (DOC)
      </button>
    </div>
  </div>

  <!-- Навигация -->"""
code = code.replace("<!-- Навигация -->", tab_patch)

# 3. ADD NAV BUTTON
nav_patch = """
    <button class="nav-item" onclick="switchNav('tab-protocol', this)">
      <span class="nav-icon">📄</span><span>Протокол</span>
    </button>
  </div>"""
code = code.replace("  </div>\n\n  <script>", nav_patch + "\n\n  <script>")

# 4. ADD JS LOGIC
js_patch = """
    // === ПРОТОКОЛ LOGIC ===
    let protCh = 1;
    let protPtIndex = 0;
    let protData = [];

    function setProtChannel(ch) {
      protCh = ch;
      document.getElementById('btnProtCh1').classList.toggle('active', ch === 1);
      document.getElementById('btnProtCh2').classList.toggle('active', ch === 2);
      document.getElementById('btnProtCh3').classList.toggle('active', ch === 3);
      generateProtTable();
    }

    function initProtSensorTypes() {
      const sel = document.getElementById('protSensorType');
      sel.innerHTML = '';
      for (const [key, val] of Object.entries(COIL_DB)) {
        sel.add(new Option(val.name, key));
      }
      sel.value = 'DVT20';
    }

    function updateProtInfo() {
      const type = document.getElementById('protSensorType').value;
      const cfg = COIL_DB[type];
      document.getElementById('colProtR2').style.display = cfg.hasSig ? 'block' : 'none';
      savePresets();
    }

    function captureProtR() {
      const offset = parseFloat(document.getElementById('inpPnOffset').value) || 0;
      document.getElementById('protR1').value = Math.max(0, lastRawR1 - offset).toFixed(2);
      if (COIL_DB[document.getElementById('protSensorType').value].hasSig) {
        document.getElementById('protR2').value = Math.max(0, lastRawR2 - offset).toFixed(2);
      }
    }

    function generateProtTable() {
      const Kn = getKn();
      protData = ptArray.map((ptMm, idx) => {
        const targetOffset = ptMm - S_MIN;
        const iCalc = I_MIN + targetOffset * Kn;
        return { idx: idx + 1, ptMm, iCalc, iFact: null, delta: null };
      });
      protPtIndex = 0;
      updateProtButton();
      renderProtTable();
    }

    function updateProtButton() {
      const btn = document.getElementById('btnProtProgonka');
      if (protPtIndex < protData.length) {
        const pt = protData[protPtIndex];
        btn.innerText = `▶ ЗАПИСАТЬ: Точка ${pt.idx} (${pt.ptMm > 0 ? '+' : ''}${pt.ptMm.toFixed(2)} мм)`;
        btn.style.background = 'var(--ch1)';
        btn.style.color = '#000';
      } else {
        btn.innerText = `✅ Прогонка завершена (Сброс)`;
        btn.style.background = 'var(--green)';
        btn.style.color = '#fff';
      }
    }

    function doProtStep() {
      if (protPtIndex >= protData.length) {
        protPtIndex = 0;
        protData.forEach(p => { p.iFact = null; p.delta = null; });
        updateProtButton();
        renderProtTable();
        return;
      }
      const item = protData[protPtIndex];
      let liveMa = protCh === 1 ? lastLiveMa1 : (protCh === 2 ? lastLiveMa2 : lastLiveMa3);
      item.iFact = liveMa;
      item.delta = calcDeltaRE100(liveMa, item.ptMm);
      protPtIndex++;
      updateProtButton();
      renderProtTable();
    }

    function undoProtStep() {
      if (protPtIndex > 0) {
        protPtIndex--;
        protData[protPtIndex].iFact = null;
        protData[protPtIndex].delta = null;
        updateProtButton();
        renderProtTable();
      }
    }

    function renderProtTable() {
      const tbody = document.getElementById('tblProtPointsBody');
      tbody.innerHTML = '';
      protData.forEach((item, i) => {
        const tr = document.createElement('tr');
        if (i === protPtIndex) tr.className = 'active-pt';
        
        const f = item.iFact !== null ? item.iFact.toFixed(2) : '—';
        const d = item.delta !== null ? item.delta.toFixed(1) + '%' : '—';
        const styleD = getDeltaStyle(item.delta, '--ch1');

        tr.innerHTML = `
          <td>${item.idx}</td>
          <td><strong>${item.ptMm > 0 ? '+' : ''}${item.ptMm.toFixed(2)}</strong></td>
          <td>${item.iCalc.toFixed(2)}</td>
          <td style="color:var(--ch1); font-weight:bold;">${f}</td>
          <td style="${styleD}">${d}</td>
        `;
        tbody.appendChild(tr);
      });
    }

    function generateDoc() {
      const typeKey = document.getElementById('protSensorType').value;
      const cfg = COIL_DB[typeKey];
      const cableLen = document.getElementById('protCableLen').value;
      const r1 = document.getElementById('protR1').value || '—';
      const r2 = document.getElementById('protR2').value || '—';
      
      const r1Nom = (cfg.r0 + cfg.k * cableLen).toFixed(2);
      const r1Tol = cfg.tol.toFixed(2);
      let r2Str = '';
      if (cfg.hasSig) {
        const r2Nom = (cfg.sr0 + cfg.sk * cableLen).toFixed(2);
        const r2Tol = cfg.stol.toFixed(2);
        r2Str = `<tr><td>Сигнальная обмотка (Wc)</td><td>${r2} Ом</td><td>${r2Nom} ± ${r2Tol} Ом</td></tr>`;
      }

      let rows = '';
      let maxDelta = 0;
      protData.forEach(p => {
        if(p.delta > maxDelta) maxDelta = p.delta;
        const f = p.iFact !== null ? p.iFact.toFixed(2) : '—';
        const d = p.delta !== null ? p.delta.toFixed(2) : '—';
        rows += `<tr><td>${p.idx}</td><td>${p.ptMm.toFixed(2)}</td><td>${p.iCalc.toFixed(2)}</td><td>${f}</td><td>${d}</td></tr>`;
      });
      
      const verdict = maxDelta <= 1.5 ? "ГОДЕН" : "НЕ ГОДЕН";

      const html = `
      <html xmlns:o='urn:schemas-microsoft-com:office:office' xmlns:w='urn:schemas-microsoft-com:office:word' xmlns='http://www.w3.org/TR/REC-html40'>
      <head><meta charset='utf-8'><title>Протокол</title>
      <style>
        body { font-family: 'Times New Roman', serif; font-size: 14pt; }
        h1, h2 { text-align: center; }
        table { width: 100%; border-collapse: collapse; margin-bottom: 20px; }
        th, td { border: 1px solid black; padding: 5px; text-align: center; }
        .text-left { text-align: left; }
      </style>
      </head>
      <body>
        <h1>ПРОТОКОЛ</h1>
        <h2>поверки / настройки измерительного канала</h2>
        <p><strong>Тип датчика:</strong> ${cfg.name}</p>
        <p><strong>Диапазон:</strong> от ${S_MIN} до ${S_MAX} мм</p>
        <p><strong>Токовый выход:</strong> ${I_MIN} – ${I_MAX} мА</p>
        <p><strong>Длина кабеля:</strong> ${cableLen} м</p>
        
        <h3>1. Проверка сопротивления катушек</h3>
        <table>
          <tr><th>Параметр</th><th>Измерено</th><th>Паспортное значение</th></tr>
          <tr><td>Обмотка возбуждения (Wp)</td><td>${r1} Ом</td><td>${r1Nom} ± ${r1Tol} Ом</td></tr>
          ${r2Str}
        </table>
        
        <h3>2. Прогонка по точкам (Канал ${protCh})</h3>
        <table>
          <tr><th>№</th><th>Заданный ход (мм)</th><th>Расчетный ток (мА)</th><th>Фактический ток (мА)</th><th>Погрешность δ (%)</th></tr>
          ${rows}
        </table>
        
        <p><strong>Максимальная погрешность:</strong> ${maxDelta.toFixed(2)} %</p>
        <p><strong>Заключение:</strong> Измерительный канал <strong>${verdict}</strong>.</p>
        
        <br><br>
        <p>Наладчик: _______________________ / ___________________ /</p>
        <p>Дата: "____" _______________ 20___ г.</p>
      </body>
      </html>`;
      
      const blob = new Blob(['\\ufeff', html], { type: 'application/msword' });
      const url = URL.createObjectURL(blob);
      const a = document.createElement('a');
      a.href = url;
      a.download = `Protocol_CH${protCh}_${cfg.name.replace(/\\s*\\/\\s*/g, '_')}.doc`;
      document.body.appendChild(a);
      a.click();
      document.body.removeChild(a);
      URL.revokeObjectURL(url);
    }

    // Initialize on load
    document.addEventListener("DOMContentLoaded", () => {
      initProtSensorTypes();
      updateProtInfo();
      generateProtTable();
    });
"""
code = code.replace("loadPresets();\n    generatePointsTable();", js_patch + "\n    loadPresets();\n    generatePointsTable();")

# Hook into onPtCountChange to also update Protocol table if ptArray changes
code = code.replace("generatePointsTable();\n      savePresets();", "generatePointsTable();\n      generateProtTable();\n      savePresets();")

with open(HTML_PATH, "w", encoding="utf-8") as f:
    f.write(code)

print("Patch applied to web_dev/index.html successfully!")
