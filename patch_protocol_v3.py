import re
import os

WEB_DIR = "web_dev"
HTML_PATH = os.path.join(WEB_DIR, "index.html")

with open(HTML_PATH, "r", encoding="utf-8") as f:
    code = f.read()

# Мы заменяем весь старый блок <div id="tab-protocol"...> до <!-- Навигация -->
# на новый продвинутый блок.

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
          <!-- Генерируется динамически -->
        </table>
      </div>

      <button class="btn-calibr" style="background:var(--ch1); color:#000; margin-top:15px; font-size: 14px;" onclick="generateDocGOST()">
        📝 СФОРМИРОВАТЬ ПРОТОКОЛ (Excel/Word ГОСТ)
      </button>
    </div>
  </div>
"""

code = re.sub(r'<!-- ==================== ЭКРАН 3: ПРОТОКОЛ.*?<!-- Навигация -->', new_tab_protocol + '\n  <!-- Навигация -->', code, flags=re.DOTALL)

# Добавляем базы данных и логику в JS
js_logic = """
    // === ПРОДВИНУТАЯ ЛОГИКА ПРОТОКОЛА ===
    const IP_DB = {
      "IP34_5": { name: "ИП34 (0..5 мм)", iMin: 4, iMax: 20, points: [0, 1.25, 2.5, 3.75, 5.0], tol: 2.5 },
      "IP43_2": { name: "ИП43 (0..2 мм)", iMin: 4, iMax: 20, points: [0, 0.5, 1.0, 1.5, 2.0], tol: 2.5 },
      "IP24_5": { name: "ИП24 (0..5 мм)", iMin: 1, iMax: 5,  points: [0, 1.25, 2.5, 3.75, 5.0], tol: 2.5 }
    };

    let protState = {
      count: 3,
      ch: [
        { dvt: 'DVT20', ip: 'IP34_5', snDvt: '101', snIp: '201', l: 5, r1: 0, r2: 0, data: [] },
        { dvt: 'DVT20', ip: 'IP34_5', snDvt: '102', snIp: '202', l: 5, r1: 0, r2: 0, data: [] },
        { dvt: 'DVT20', ip: 'IP34_5', snDvt: '103', snIp: '203', l: 5, r1: 0, r2: 0, data: [] }
      ],
      ptIndex: 0
    };

    function buildProtConfig() {
      protState.count = parseInt(document.getElementById('protCount').value);
      const area = document.getElementById('protConfigArea');
      let html = '';
      
      let dvtOpts = Object.keys(COIL_DB).map(k => `<option value="${k}">${COIL_DB[k].name}</option>`).join('');
      let ipOpts = Object.keys(IP_DB).map(k => `<option value="${k}">${IP_DB[k].name}</option>`).join('');

      for(let i=0; i<protState.count; i++) {
        html += `
          <div style="border-top:1px solid #24334a; margin-top:10px; padding-top:10px;">
            <div class="prot-header">Канал ${i+1}</div>
            <div class="form-row">
              <div class="form-col"><span class="form-lbl">Зав. № ДВТ:</span><input type="text" class="form-inp" id="p_sndvt_${i}" value="${protState.ch[i].snDvt}"></div>
              <div class="form-col"><span class="form-lbl">Датчик (ДВТ):</span><select class="form-sel" id="p_dvt_${i}">${dvtOpts}</select></div>
              <div class="form-col"><span class="form-lbl">Кабель (м):</span><input type="number" class="form-inp" id="p_l_${i}" value="${protState.ch[i].l}"></div>
            </div>
            <div class="form-row">
              <div class="form-col"><span class="form-lbl">Зав. № ИП:</span><input type="text" class="form-inp" id="p_snip_${i}" value="${protState.ch[i].snIp}"></div>
              <div class="form-col"><span class="form-lbl">ИП (Выход):</span><select class="form-sel" id="p_ip_${i}" onchange="resetProtGost()">${ipOpts}</select></div>
            </div>
          </div>
        `;
      }
      area.innerHTML = html;
      
      for(let i=0; i<protState.count; i++) {
        document.getElementById(`p_dvt_${i}`).value = protState.ch[i].dvt;
        document.getElementById(`p_ip_${i}`).value = protState.ch[i].ip;
      }
      resetProtGost();
    }

    function syncProtState() {
      for(let i=0; i<protState.count; i++) {
        protState.ch[i].snDvt = document.getElementById(`p_sndvt_${i}`).value;
        protState.ch[i].dvt = document.getElementById(`p_dvt_${i}`).value;
        protState.ch[i].l = parseFloat(document.getElementById(`p_l_${i}`).value) || 5;
        protState.ch[i].snIp = document.getElementById(`p_snip_${i}`).value;
        protState.ch[i].ip = document.getElementById(`p_ip_${i}`).value;
      }
    }

    function resetProtGost() {
      syncProtState();
      protState.ptIndex = 0;
      
      // Инициализируем массивы данных
      for(let i=0; i<protState.count; i++) {
        const ipCfg = IP_DB[protState.ch[i].ip];
        protState.ch[i].data = ipCfg.points.map(pt => ({ ptMm: pt, iFact: null, delta: null }));
      }
      updateProtButtonGOST();
      renderProtTableGOST();
    }

    function updateProtButtonGOST() {
      const btn = document.getElementById('btnProtProgonka');
      const basePoints = IP_DB[protState.ch[0].ip].points; // Берем точки первого канала как ориентир
      
      if (protState.ptIndex < basePoints.length) {
        btn.innerText = `▶ ЗАПИСАТЬ ТОЧКУ: ${basePoints[protState.ptIndex]} мм`;
        btn.style.background = 'var(--ch1)'; btn.style.color = '#000';
      } else {
        btn.innerText = `✅ Прогонка завершена (Сброс)`;
        btn.style.background = 'var(--green)'; btn.style.color = '#fff';
      }
    }

    function doProtStepGOST() {
      syncProtState();
      const basePoints = IP_DB[protState.ch[0].ip].points;
      
      if (protState.ptIndex >= basePoints.length) {
        resetProtGost();
        return;
      }

      let currentVals = [lastLiveMa1, lastLiveMa2, lastLiveMa3];
      
      for(let i=0; i<protState.count; i++) {
        const ipCfg = IP_DB[protState.ch[i].ip];
        const pt = protState.ch[i].data[protState.ptIndex];
        
        const S = ipCfg.points[ipCfg.points.length-1] - ipCfg.points[0];
        const Kn = (ipCfg.iMax - ipCfg.iMin) / S;
        
        const iCalc = ipCfg.iMin + (pt.ptMm - ipCfg.points[0]) * Kn;
        pt.iFact = currentVals[i];
        
        // Основная приведенная погрешность, %
        pt.delta = ((pt.iFact - iCalc) / (ipCfg.iMax - ipCfg.iMin)) * 100.0;
      }
      
      protState.ptIndex++;
      updateProtButtonGOST();
      renderProtTableGOST();
    }

    function undoProtStepGOST() {
      if (protState.ptIndex > 0) {
        protState.ptIndex--;
        for(let i=0; i<protState.count; i++) {
          protState.ch[i].data[protState.ptIndex].iFact = null;
          protState.ch[i].data[protState.ptIndex].delta = null;
        }
        updateProtButtonGOST();
        renderProtTableGOST();
      }
    }

    function renderProtTableGOST() {
      if(protState.count === 0) return;
      const tbody = document.getElementById('tblProtPointsGOST');
      const ipCfg = IP_DB[protState.ch[0].ip];
      
      let html = `<thead><tr><th>Датчик</th><th>ИП</th><th>Параметр</th>`;
      ipCfg.points.forEach(p => html += `<th>${p} мм</th>`);
      html += `</tr></thead><tbody>`;

      for(let i=0; i<protState.count; i++) {
        const c = protState.ch[i];
        const rowI = c.data.map(d => d.iFact !== null ? d.iFact.toFixed(3) : '—').join('</td><td>');
        const rowD = c.data.map(d => d.delta !== null ? d.delta.toFixed(2) : '—').join('</td><td>');
        
        html += `
          <tr>
            <td rowspan="2" style="background:#080c13;">${c.snDvt}</td>
            <td rowspan="2" style="background:#080c13;">${c.snIp}</td>
            <td>I факт, мА</td><td>${rowI}</td>
          </tr>
          <tr>
            <td>Погрешность, %</td><td>${rowD}</td>
          </tr>
        `;
      }
      html += `</tbody>`;
      tbody.innerHTML = html;
    }

    function generateDocGOST() {
      syncProtState();
      
      let table431 = `
      <h3>4.3.1. Определение основной приведенной погрешности</h3>
      <table>
        <tr>
          <th rowspan="2">Заводской номер Датчик</th>
          <th rowspan="2">Заводской номер Преобразователь</th>
          <th rowspan="2">Наименование параметра</th>
          <th colspan="${IP_DB[protState.ch[0].ip].points.length}">Диапазон измерения, мм</th>
        </tr>
        <tr>`;
        
      IP_DB[protState.ch[0].ip].points.forEach(p => {
        table431 += `<th>${p}</th>`;
      });
      table431 += `</tr>`;

      for(let i=0; i<protState.count; i++) {
        const c = protState.ch[i];
        const rowI = c.data.map(d => d.iFact !== null ? d.iFact.toFixed(3) : '—').join('</td><td>');
        const rowD = c.data.map(d => d.delta !== null ? d.delta.toFixed(2) : '—').join('</td><td>');
        table431 += `
          <tr>
            <td rowspan="3">${c.snDvt}</td>
            <td rowspan="3">${c.snIp}</td>
            <td>Показания выходного сигнала, мА</td>
            <td>${rowI}</td>
          </tr>
          <tr>
            <td>Основная приведенная погрешность измерения, %</td>
            <td>${rowD}</td>
          </tr>
          <tr>
            <td>Предельные отклонения, %</td>
            <td colspan="${c.data.length}">±${IP_DB[c.ip].tol}</td>
          </tr>
        `;
      }
      table431 += `</table>`;

      let table433 = `
      <h3>4.3.3. Определение активного сопротивления обмотки</h3>
      <table>
        <tr>
          <th>Зав. № Датчик</th><th>Зав. № ИП</th>
          <th>Наименование параметра</th>
          <th>Показания омметра, Ом</th>
          <th>Номинальное значение, Ом</th>
          <th>Предельные отклонения, Ом</th>
        </tr>`;

      for(let i=0; i<protState.count; i++) {
        const c = protState.ch[i];
        const cfg = COIL_DB[c.dvt];
        const r1Nom = (cfg.r0 + cfg.k * c.l).toFixed(2);
        
        table433 += `
          <tr>
            <td rowspan="2">${c.snDvt}</td><td rowspan="2">${c.snIp}</td>
            <td>Активное сопротивление обмотки возбуждения, Ом</td>
            <td>—</td><td>${r1Nom}</td><td>±${cfg.tol}</td>
          </tr>`;
        
        if (cfg.hasSig) {
          const r2Nom = (cfg.sr0 + cfg.sk * c.l).toFixed(2);
          table433 += `
          <tr>
            <td>Активное сопротивление обмотки сигнальной, Ом</td>
            <td>—</td><td>${r2Nom}</td><td>±${cfg.stol}</td>
          </tr>`;
        } else {
          table433 += `<tr><td colspan="4" style="background:#f0f0f0;">Нет сигнальной обмотки</td></tr>`;
        }
      }
      table433 += `</table>`;

      const html = `
      <html xmlns:o='urn:schemas-microsoft-com:office:office' xmlns:w='urn:schemas-microsoft-com:office:word' xmlns='http://www.w3.org/TR/REC-html40'>
      <head><meta charset='utf-8'><title>Протокол ГОСТ</title>
      <style>
        body { font-family: 'Times New Roman', serif; font-size: 11pt; }
        h1, h2, h3 { text-align: center; font-size: 12pt; }
        table { width: 100%; border-collapse: collapse; margin-bottom: 20px; font-size: 10pt; }
        th, td { border: 1px solid black; padding: 4px; text-align: center; vertical-align: middle; }
      </style>
      </head>
      <body>
        <div style="text-align:center;">ОБЩЕСТВО С ОГРАНИЧЕННОЙ ОТВЕТСТВЕННОСТЬЮ<br>НАУЧНО-ПРОИЗВОДСТВЕННОЕ ПРЕДПРИЯТИЕ «ВИБРОБИТ»</div>
        <hr>
        <h2>ПРОТОКОЛ КАЛИБРОВКИ № _________ от ___ . ___ . 20___ г.</h2>
        <p><strong>Наименование и тип СИ:</strong> Аппаратура «Вибробит 100»</p>
        
        ${table431}
        
        <h3>4.3.2. Определение электрического сопротивления изоляции обмотки</h3>
        <table>
          <tr><th>Зав. № Датчик</th><th>Зав. № ИП</th><th>Наименование параметра</th><th>Показания мегомметра, МОм</th><th>Предельные отклонения, МОм</th></tr>
          <tr><td></td><td></td><td>Электрическое сопротивление изоляции</td><td></td><td>не менее 1</td></tr>
        </table>
        
        ${table433}
        
        <br><br>
        <p>Калибровщик: _______________________ / ___________________ /</p>
      </body>
      </html>`;
      
      const blob = new Blob(['\\ufeff', html], { type: 'application/msword' });
      const url = URL.createObjectURL(blob);
      const a = document.createElement('a');
      a.href = url;
      a.download = `Protocol_Vibrobit_GOST.doc`;
      document.body.appendChild(a);
      a.click();
      document.body.removeChild(a);
      URL.revokeObjectURL(url);
    }

    // Инициализация при загрузке
    document.addEventListener("DOMContentLoaded", () => {
      buildProtConfig();
    });
"""

code = re.sub(r'// === ПРОТОКОЛ LOGIC ===.*?// Initialize on load.*?}\);', js_logic, code, flags=re.DOTALL)

with open(HTML_PATH, "w", encoding="utf-8") as f:
    f.write(code)

print("Patch v3 applied to web_dev/index.html successfully!")
