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
      <div class="coil-title">Настройки партии приборов (как в Live)</div>
      
      <div class="form-row">
        <div class="form-col">
          <span class="form-lbl">Кол-во приборов:</span>
          <select id="protCount" class="form-sel" onchange="buildProtConfig()">
            <option value="1">1 канал (CH1)</option>
            <option value="2">2 канала (CH1, CH2)</option>
            <option value="3" selected>3 канала (CH1, CH2, CH3)</option>
          </select>
        </div>
      </div>
      
      <div class="form-row">
        <div class="form-col">
          <span class="form-lbl">Тип Датчика (ДВТ):</span>
          <select id="protDvtType" class="form-sel" onchange="updateProtIpOptions()">
             <!-- Populated dynamically -->
          </select>
        </div>
        <div class="form-col">
          <span class="form-lbl">Тип ИП:</span>
          <select id="protIpType" class="form-sel" onchange="resetProtGost()">
             <!-- Populated dynamically -->
          </select>
        </div>
      </div>
      
      <div class="form-row">
        <div class="form-col">
          <span class="form-lbl">Ток (мА):</span>
          <select id="protScale" class="form-sel" onchange="resetProtGost()">
            <option value="4-20">4..20 мА</option>
            <option value="1-5">1..5 мА</option>
          </select>
        </div>
        <div class="form-col">
          <span class="form-lbl">Кабель (м):</span>
          <input type="number" id="protCable" class="form-inp" value="5" step="1" onchange="resetProtGost()">
        </div>
      </div>
      
      <div class="form-row">
        <div class="form-col">
          <span class="form-lbl">Мин. зазор (мм):</span>
          <input type="number" id="protSmin" class="form-inp" value="0" onchange="resetProtGost()">
        </div>
        <div class="form-col">
          <span class="form-lbl">Макс. зазор (мм):</span>
          <input type="number" id="protSmax" class="form-inp" value="5" onchange="resetProtGost()">
        </div>
      </div>
      
      <div class="form-row">
        <div class="form-col">
          <span class="form-lbl">Точки прогона (мм через запятую):</span>
          <input type="text" id="protPointsInput" class="form-inp" value="0, 1.25, 2.5, 3.75, 5" onchange="resetProtGost()">
        </div>
      </div>

    </div>

    <div class="coil-card">
      <div class="coil-title">Заводские номера</div>
      <div id="protConfigArea"></div>
    </div>

    <div class="coil-card">
      <div class="coil-title">Прогонка и Формирование</div>
      <div class="progonka-controls">
        <button id="btnProtProgonka" class="btn-progonka" onclick="doProtStepGOST()">▶ Записать точку 1</button>
        <button class="btn-undo" onclick="undoProtStepGOST()">↩ Назад</button>
      </div>

      <div style="overflow-x: auto; margin-top: 10px;">
        <table class="table-points" id="tblProtPointsGOST">
        </table>
      </div>

      <button class="btn-calibr" style="background:var(--ch1); color:#000; margin-top:15px; font-size: 14px;" onclick="generateDocGOST()">
        📝 СФОРМИРОВАТЬ ПРОТОКОЛ (ГОСТ)
      </button>
    </div>
  </div>
"""

js_logic = """
    // === ПРОДВИНУТАЯ ЛОГИКА ПРОТОКОЛА V4 ===
    const IP_FAMILIES = {
        "DVT_10_20_30_50_60": [{ id: "ИП34", name: "ИП34" }, { id: "ИП36", name: "ИП36" }, { id: "ИП24", name: "ИП24" }],
        "DVT_40_43_70": [{ id: "ИП43", name: "ИП43" }, { id: "ИП36", name: "ИП36" }]
    };

    let protState = {
        count: 3, dvtType: 'DVT20', ipType: 'ИП34', cable: 5, iMin: 4, iMax: 20, sMin: 0, sMax: 5,
        points: [0, 1.25, 2.5, 3.75, 5],
        sn: [
            { dvt: '', ip: '', data: [] },
            { dvt: '', ip: '', data: [] },
            { dvt: '', ip: '', data: [] }
        ],
        ptIndex: 0
    };

    function initProtTab() {
        let dvtSel = document.getElementById('protDvtType');
        let opts = '';
        for(let k in COIL_DB) opts += `<option value="${k}">${COIL_DB[k].name}</option>`;
        dvtSel.innerHTML = opts;
        dvtSel.value = 'DVT20';
        updateProtIpOptions();
        buildProtConfig();
    }

    function updateProtIpOptions() {
        let dvt = document.getElementById('protDvtType').value;
        let family = (dvt.includes("43") || dvt.includes("40") || dvt.includes("70")) ? "DVT_40_43_70" : "DVT_10_20_30_50_60";
        let ipSel = document.getElementById('protIpType');
        let opts = '';
        IP_FAMILIES[family].forEach(ip => { opts += `<option value="${ip.id}">${ip.name}</option>`; });
        ipSel.innerHTML = opts;
        resetProtGost();
    }

    function buildProtConfig() {
        protState.count = parseInt(document.getElementById('protCount').value);
        const area = document.getElementById('protConfigArea');
        let html = '';
        for(let i=0; i<protState.count; i++) {
            let sdvt = protState.sn[i] ? protState.sn[i].dvt : '';
            let sip = protState.sn[i] ? protState.sn[i].ip : '';
            html += `
                <div class="form-row" style="border-top:1px solid #24334a; padding-top:8px;">
                    <div class="form-col" style="flex:0.2; justify-content:center; color:var(--ch1); font-weight:bold;">CH${i+1}</div>
                    <div class="form-col"><span class="form-lbl">№ ДВТ:</span><input type="text" class="form-inp" id="p_sndvt_${i}" value="${sdvt}"></div>
                    <div class="form-col"><span class="form-lbl">№ ИП:</span><input type="text" class="form-inp" id="p_snip_${i}" value="${sip}"></div>
                </div>
            `;
        }
        area.innerHTML = html;
        resetProtGost();
    }

    function syncProtState() {
        protState.dvtType = document.getElementById('protDvtType').value;
        protState.ipType = document.getElementById('protIpType').value;
        protState.cable = parseFloat(document.getElementById('protCable').value) || 5;
        let scale = document.getElementById('protScale').value.split('-');
        protState.iMin = parseFloat(scale[0]);
        protState.iMax = parseFloat(scale[1]);
        protState.sMin = parseFloat(document.getElementById('protSmin').value);
        protState.sMax = parseFloat(document.getElementById('protSmax').value);
        let pts = document.getElementById('protPointsInput').value.split(',').map(s => parseFloat(s.trim())).filter(n => !isNaN(n));
        if(pts.length === 0) pts = [0];
        protState.points = pts;
        
        for(let i=0; i<protState.count; i++) {
            if(!protState.sn[i]) protState.sn[i] = {dvt:'', ip:'', data:[]};
            let elDvt = document.getElementById(`p_sndvt_${i}`);
            let elIp = document.getElementById(`p_snip_${i}`);
            if(elDvt) protState.sn[i].dvt = elDvt.value;
            if(elIp) protState.sn[i].ip = elIp.value;
        }
    }

    function resetProtGost() {
        syncProtState();
        protState.ptIndex = 0;
        for(let i=0; i<protState.count; i++) {
            protState.sn[i].data = protState.points.map(pt => ({ ptMm: pt, iFact: null, delta: null }));
        }
        updateProtButtonGOST();
        renderProtTableGOST();
    }

    function updateProtButtonGOST() {
        const btn = document.getElementById('btnProtProgonka');
        if (protState.ptIndex < protState.points.length) {
            btn.innerText = `▶ ЗАПИСАТЬ ТОЧКУ: ${protState.points[protState.ptIndex]} мм`;
            btn.style.background = 'var(--ch1)'; btn.style.color = '#000';
        } else {
            btn.innerText = `✅ Прогонка завершена (Сброс)`;
            btn.style.background = 'var(--green)'; btn.style.color = '#fff';
        }
    }

    function doProtStepGOST() {
        syncProtState();
        if (protState.ptIndex >= protState.points.length) {
            resetProtGost();
            return;
        }
        let currentVals = [lastLiveMa1, lastLiveMa2, lastLiveMa3];
        const S = protState.sMax - protState.sMin;
        const Kn = (protState.iMax - protState.iMin) / S;

        for(let i=0; i<protState.count; i++) {
            const pt = protState.sn[i].data[protState.ptIndex];
            const iCalc = protState.iMin + (pt.ptMm - protState.sMin) * Kn;
            pt.iFact = currentVals[i];
            pt.delta = ((pt.iFact - iCalc) / (protState.iMax - protState.iMin)) * 100.0;
        }
        protState.ptIndex++;
        updateProtButtonGOST();
        renderProtTableGOST();
    }

    function undoProtStepGOST() {
        if (protState.ptIndex > 0) {
            protState.ptIndex--;
            for(let i=0; i<protState.count; i++) {
                protState.sn[i].data[protState.ptIndex].iFact = null;
                protState.sn[i].data[protState.ptIndex].delta = null;
            }
            updateProtButtonGOST();
            renderProtTableGOST();
        }
    }

    function renderProtTableGOST() {
        if(protState.count === 0) return;
        const tbody = document.getElementById('tblProtPointsGOST');
        let html = `<thead><tr><th>Датчик</th><th>ИП</th><th>Параметр</th>`;
        protState.points.forEach(p => html += `<th>${p} мм</th>`);
        html += `</tr></thead><tbody>`;

        for(let i=0; i<protState.count; i++) {
            const c = protState.sn[i];
            const rowI = c.data.map(d => d.iFact !== null ? d.iFact.toFixed(3) : '—').join('</td><td>');
            const rowD = c.data.map(d => d.delta !== null ? d.delta.toFixed(2) : '—').join('</td><td>');
            html += `
                <tr><td rowspan="2" style="background:#080c13;">${c.dvt||'—'}</td><td rowspan="2" style="background:#080c13;">${c.ip||'—'}</td>
                <td>I факт, мА</td><td>${rowI}</td></tr>
                <tr><td>Погрешность, %</td><td>${rowD}</td></tr>
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
            <tr><th rowspan="2">Заводской номер Датчик</th><th rowspan="2">Заводской номер Преобразователь</th><th rowspan="2">Наименование параметра</th><th colspan="${protState.points.length}">Диапазон измерения, мм</th></tr>
            <tr>`;
        protState.points.forEach(p => { table431 += `<th>${p}</th>`; });
        table431 += `</tr>`;

        for(let i=0; i<protState.count; i++) {
            const c = protState.sn[i];
            const rowI = c.data.map(d => d.iFact !== null ? d.iFact.toFixed(3) : '—').join('</td><td>');
            const rowD = c.data.map(d => d.delta !== null ? d.delta.toFixed(2) : '—').join('</td><td>');
            table431 += `
                <tr><td rowspan="3">${c.dvt||'—'}</td><td rowspan="3">${c.ip||'—'}</td><td>Показания выходного сигнала, мА</td><td>${rowI}</td></tr>
                <tr><td>Основная приведенная погрешность измерения, %</td><td>${rowD}</td></tr>
                <tr><td>Предельные отклонения, %</td><td colspan="${c.data.length}">±2,5</td></tr>
            `;
        }
        table431 += `</table>`;

        let table433 = `
        <h3>4.3.3. Определение активного сопротивления обмотки</h3>
        <table>
            <tr><th>Зав. № Датчик</th><th>Зав. № ИП</th><th>Наименование параметра</th><th>Показания омметра, Ом</th><th>Номинальное значение, Ом</th><th>Предельные отклонения, Ом</th></tr>`;
        for(let i=0; i<protState.count; i++) {
            const c = protState.sn[i];
            const cfg = COIL_DB[protState.dvtType];
            const r1Nom = (cfg.r0 + cfg.k * protState.cable).toFixed(2);
            table433 += `<tr><td rowspan="2">${c.dvt||'—'}</td><td rowspan="2">${c.ip||'—'}</td><td>Активное сопротивление обмотки возбуждения, Ом</td><td>—</td><td>${r1Nom}</td><td>±${cfg.tol}</td></tr>`;
            if (cfg.hasSig) {
                const r2Nom = (cfg.sr0 + cfg.sk * protState.cable).toFixed(2);
                table433 += `<tr><td>Активное сопротивление обмотки сигнальной, Ом</td><td>—</td><td>${r2Nom}</td><td>±${cfg.stol}</td></tr>`;
            } else {
                table433 += `<tr><td colspan="4" style="background:#f0f0f0;">Нет сигнальной обмотки</td></tr>`;
            }
        }
        table433 += `</table>`;

        const html = `
        <html xmlns:o='urn:schemas-microsoft-com:office:office' xmlns:w='urn:schemas-microsoft-com:office:word' xmlns='http://www.w3.org/TR/REC-html40'>
        <head><meta charset='utf-8'><title>Протокол ГОСТ</title>
        <style>body{font-family:'Times New Roman',serif;font-size:11pt;}h1,h2,h3{text-align:center;font-size:12pt;}table{width:100%;border-collapse:collapse;margin-bottom:20px;font-size:10pt;}th,td{border:1px solid black;padding:4px;text-align:center;vertical-align:middle;}</style>
        </head><body>
            <div style="text-align:center;">ОБЩЕСТВО С ОГРАНИЧЕННОЙ ОТВЕТСТВЕННОСТЬЮ<br>НАУЧНО-ПРОИЗВОДСТВЕННОЕ ПРЕДПРИЯТИЕ «ВИБРОБИТ»</div>
            <hr>
            <h2>ПРОТОКОЛ КАЛИБРОВКИ № _________ от ___ . ___ . 20___ г.</h2>
            <p><strong>Наименование и тип СИ:</strong> Аппаратура «Вибробит 100», Датчик ${COIL_DB[protState.dvtType].name}, Преобразователь ${protState.ipType}</p>
            ${table431}
            <h3>4.3.2. Определение электрического сопротивления изоляции обмотки</h3>
            <table><tr><th>Зав. № Датчик</th><th>Зав. № ИП</th><th>Наименование параметра</th><th>Показания мегомметра, МОм</th><th>Предельные отклонения, МОм</th></tr>
            <tr><td></td><td></td><td>Электрическое сопротивление изоляции</td><td></td><td>не менее 1</td></tr></table>
            ${table433}
            <br><br><p>Калибровщик: _______________________ / ___________________ /</p>
        </body></html>`;
        
        const blob = new Blob(['\\ufeff', html], { type: 'application/msword' });
        const url = URL.createObjectURL(blob);
        const a = document.createElement('a'); a.href = url; a.download = `Protocol_Vibrobit_GOST.doc`;
        document.body.appendChild(a); a.click(); document.body.removeChild(a); URL.revokeObjectURL(url);
    }
"""

# Replace HTML
code = re.sub(r'<!-- ==================== ЭКРАН 3: ПРОТОКОЛ.*?<!-- Навигация -->', new_tab_protocol + '\n  <!-- Навигация -->', code, flags=re.DOTALL)

# Replace JS logic safely
# Find the start of advanced protocol logic and replace till end of script
start_idx = code.find('// === ПРОДВИНУТАЯ ЛОГИКА ПРОТОКОЛА')
if start_idx == -1:
    start_idx = code.find('// === ПРОТОКОЛ LOGIC ===')

end_idx = code.find('</script>', start_idx)

if start_idx != -1 and end_idx != -1:
    # Also add initProtTab() call into DOMContentLoaded
    init_call = "      initProtTab();\n"
    code = code[:start_idx] + js_logic + '\n' + init_call + code[end_idx:]
    # insert initProtTab into DOMContentLoaded if not present
    if 'initProtTab()' not in code[end_idx:]:
        dom_idx = code.find('document.addEventListener("DOMContentLoaded"')
        if dom_idx != -1:
            ins_idx = code.find('{', dom_idx) + 1
            code = code[:ins_idx] + "\n      initProtTab();" + code[ins_idx:]

with open(HTML_PATH, "w", encoding="utf-8") as f:
    f.write(code)

print("V4 Patch applied cleanly!")
