import re
import os

WEB_DIR = "web_dev"
HTML_PATH = os.path.join(WEB_DIR, "index.html")

with open(HTML_PATH, "r", encoding="utf-8") as f:
    code = f.read()

# We already patched the HTML part in the previous run before it crashed.
# Let's just safely replace the JS part.

with open("patch_protocol_v3.py", "r", encoding="utf-8") as f:
    patch_code = f.read()

js_logic = patch_code.split('js_logic = """')[1].split('"""')[0]

code = re.sub(r'// === ПРОТОКОЛ LOGIC ===.*?// Initialize on load.*?}\);', 
              lambda m: js_logic, code, flags=re.DOTALL)

with open(HTML_PATH, "w", encoding="utf-8") as f:
    f.write(code)

print("JS Patch applied")
