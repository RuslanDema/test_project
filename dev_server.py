import os
import re
import json
import urllib.parse
from http.server import HTTPServer, BaseHTTPRequestHandler

INO_PATH = r"firmware\pn2026_esp32\pn2026_esp32.ino"
WEB_DIR = "web_dev"

def extract():
    os.makedirs(WEB_DIR, exist_ok=True)
    with open(INO_PATH, "r", encoding="utf-8") as f:
        code = f.read()
    
    idx_match = re.search(r'const char INDEX_HTML\[\] PROGMEM = R"rawliteral\((.*?)\)rawliteral";', code, re.DOTALL)
    if idx_match:
        with open(os.path.join(WEB_DIR, "index.html"), "w", encoding="utf-8") as f:
            f.write(idx_match.group(1))
            
    cal_match = re.search(r'const char CALIB_HTML\[\] PROGMEM = R"rawliteral\((.*?)\)rawliteral";', code, re.DOTALL)
    if cal_match:
        with open(os.path.join(WEB_DIR, "calibrate.html"), "w", encoding="utf-8") as f:
            f.write(cal_match.group(1))
    print("HTML extracted to 'web_dev' folder.")

def inject():
    with open(INO_PATH, "r", encoding="utf-8") as f:
        code = f.read()
        
    try:
        with open(os.path.join(WEB_DIR, "index.html"), "r", encoding="utf-8") as f:
            idx_html = f.read()
        code = re.sub(r'(const char INDEX_HTML\[\] PROGMEM = R"rawliteral\().*?(\)rawliteral";)', 
                      lambda m: m.group(1) + idx_html + m.group(2), code, flags=re.DOTALL)
    except FileNotFoundError:
        pass

    try:
        with open(os.path.join(WEB_DIR, "calibrate.html"), "r", encoding="utf-8") as f:
            cal_html = f.read()
        code = re.sub(r'(const char CALIB_HTML\[\] PROGMEM = R"rawliteral\().*?(\)rawliteral";)', 
                      lambda m: m.group(1) + cal_html + m.group(2), code, flags=re.DOTALL)
    except FileNotFoundError:
        pass

    with open(INO_PATH, "w", encoding="utf-8") as f:
        f.write(code)
    print("HTML injected back into .ino file.")

class MockServer(BaseHTTPRequestHandler):
    def do_GET(self):
        parsed = urllib.parse.urlparse(self.path)
        path = parsed.path
        
        if path == "/":
            self.send_response(200)
            self.send_header("Content-type", "text/html; charset=utf-8")
            self.end_headers()
            with open(os.path.join(WEB_DIR, "index.html"), "rb") as f:
                self.wfile.write(f.read())
                
        elif path == "/calibrate":
            self.send_response(200)
            self.send_header("Content-type", "text/html; charset=utf-8")
            self.end_headers()
            with open(os.path.join(WEB_DIR, "calibrate.html"), "rb") as f:
                self.wfile.write(f.read())
                
        elif path == "/api/data":
            self.send_response(200)
            self.send_header("Content-type", "application/json")
            self.send_header("Access-Control-Allow-Origin", "*")
            self.end_headers()
            import random
            import math
            import time
            t = time.time()
            data = {
                "ch1": {"ma": round(12.0 + math.sin(t)*4, 2), "mm": round(math.sin(t)*10, 1)},
                "ch2": {"ma": round(8.0 + math.cos(t)*2, 2), "mm": round(math.cos(t)*5, 1)},
                "ch3": {"ma": 4.0, "mm": -5.0},
                "bat": {"volts": 11.5, "percent": 50}
            }
            self.wfile.write(json.dumps(data).encode("utf-8"))
            
        elif path.startswith("/api/set_points"):
            self.send_response(200)
            self.send_header("Content-type", "text/plain")
            self.end_headers()
            self.wfile.write(b"OK")
            print("Set points mock called:", parsed.query)
            
        else:
            self.send_response(404)
            self.end_headers()

    def do_POST(self):
        if self.path == "/api/calibrate":
            content_length = int(self.headers['Content-Length'])
            post_data = self.rfile.read(content_length)
            print("Calibration received:", post_data.decode("utf-8"))
            self.send_response(200)
            self.send_header("Content-type", "text/plain")
            self.end_headers()
            self.wfile.write(b"OK")
        else:
            self.send_response(404)
            self.end_headers()

    def log_message(self, format, *args):
        pass # Suppress excessive logs for API calls

if __name__ == "__main__":
    import sys
    if len(sys.argv) > 1 and sys.argv[1] == "inject":
        inject()
    elif len(sys.argv) > 1 and sys.argv[1] == "extract":
        extract()
    else:
        extract()
        server_address = ('', 8080)
        httpd = HTTPServer(server_address, MockServer)
        print("======================================================")
        print("Mock server running on http://localhost:8080")
        print("Edit HTML files directly in the 'web_dev/' folder.")
        print("Changes apply immediately on page refresh.")
        print("To save changes back to the .ino file, run:")
        print("python dev_server.py inject")
        print("======================================================")
        try:
            httpd.serve_forever()
        except KeyboardInterrupt:
            pass
