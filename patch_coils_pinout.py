import re
import os

WEB_DIR = "web_dev"
HTML_PATH = os.path.join(WEB_DIR, "index.html")

with open(HTML_PATH, "r", encoding="utf-8") as f:
    code = f.read()

# 1. ADD CSS FOR PINOUT TABLE
css_patch = """
    /* === PINOUT === */
    .pinout-box { background: #080c13; border: 1px solid var(--border); border-radius: 8px; padding: 10px; margin-bottom: 12px; }
    .pinout-title { font-size: 11px; font-weight: 800; color: var(--ch1); margin-bottom: 6px; text-transform: uppercase; }
    .pinout-table { width: 100%; border-collapse: collapse; font-size: 11px; text-align: left; }
    .pinout-table th, .pinout-table td { border: 1px solid var(--border); padding: 4px 6px; }
    .pinout-table th { color: var(--subtext); background: #131a27; }
"""
code = code.replace("</style>", css_patch + "</style>")

# 2. INSERT HTML PLACEHOLDER
html_to_insert = """
      <div class="pinout-box" id="pinoutBox">
        <div class="pinout-title">Схема подключения (Разъем РС7ТВ)</div>
        <table class="pinout-table">
          <thead><tr><th>Контакт</th><th>Цепь</th></tr></thead>
          <tbody id="pinoutTbody"></tbody>
        </table>
      </div>
"""
# Insert before the measurement box
code = code.replace('<div class="coil-res-box">', html_to_insert + '\n      <div class="coil-res-box">')

# 3. JS TO UPDATE PINOUT
js_patch = """
      // PINOUT LOGIC
      const pinoutTbody = document.getElementById('pinoutTbody');
      let p_html = '';
      if (type.includes("DVT43") || type.includes("DVT40") || type.includes("DVT70")) {
        // Двухкатушечные датчики (Сигнальная + Возбуждение)
        p_html = `
          <tr><td>1, 2</td><td>Общий</td></tr>
          <tr><td>3, 5</td><td>Обмотка сигнальная (Wc)</td></tr>
          <tr><td style="color:var(--ch1);">6, 7</td><td style="color:var(--ch1);">Обмотка возбуждения (Wp)</td></tr>
        `;
      } else {
        // Однокатушечные датчики
        p_html = `
          <tr><td style="color:var(--ch1);">2, 5</td><td style="color:var(--ch1);">Обмотка возбуждения (Wp)</td></tr>
          <tr><td>1, 3</td><td>Общий</td></tr>
          <tr><td>6, 7</td><td>GND</td></tr>
        `;
      }
      pinoutTbody.innerHTML = p_html;
"""
# Insert inside updateCoilNominals()
code = code.replace('const r1Nom = cfg.r0 + cfg.k * l;', js_patch + '\n      const r1Nom = cfg.r0 + cfg.k * l;')


with open(HTML_PATH, "w", encoding="utf-8") as f:
    f.write(code)

print("Pinout patch applied to web_dev/index.html successfully!")
