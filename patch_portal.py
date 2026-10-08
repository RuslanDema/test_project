import re

with open(r'c:\Users\Yafarov_DS\Desktop\test_project\firmware\pn2026_esp32\pn2026_esp32.ino', 'r', encoding='utf-8') as f:
    code = f.read()

# Replace any server.on for hotspot-detect and following lines up to connecttest.txt
old_pattern = r'\s*server\.on\("/hotspot-detect\.html"[\s\S]*?server\.on\("/connecttest\.txt"[\s\S]*?\}\);'

new_routes = """
    auto redirectHandler = [](AsyncWebServerRequest *request){
      request->redirect("http://192.168.4.1/");
    };
    server.on("/hotspot-detect.html", HTTP_GET, redirectHandler);
    server.on("/canonical.html", HTTP_GET, redirectHandler);
    server.on("/generate_204", HTTP_GET, redirectHandler);
    server.on("/connecttest.txt", HTTP_GET, redirectHandler);
    server.on("/redirect", HTTP_GET, redirectHandler);
    server.on("/ncsi.txt", HTTP_GET, redirectHandler);"""

code = re.sub(old_pattern, new_routes, code)

with open(r'c:\Users\Yafarov_DS\Desktop\test_project\firmware\pn2026_esp32\pn2026_esp32.ino', 'w', encoding='utf-8') as f:
    f.write(code)

print("Captive portal patch forcefully applied!")
