"""From the BUILD container, probe Aiven's internal network for a package mirror
or any service that could serve the kernel image. Read-only HTTP probes."""
import os, socket, subprocess

def sh(c, t=40):
    try:
        r=subprocess.run(["sh","-c",c],capture_output=True,text=True,timeout=t); return (r.stdout+r.stderr)[:8000]
    except Exception as e: return "ERR:"+repr(e)

S=["UID:%s"%os.getuid()]
S.append("===INTERNAL-HTTP===")
for host in ["169.254.1.2","172.31.64.1","172.31.73.110","host.containers.internal"]:
    for port in [80,443,8080,5000,3128,8000]:
        try:
            s=socket.socket(); s.settimeout(2); s.connect((host,port)); s.close()
            S.append(f"  {host}:{port} OPEN")
        except Exception: pass
S.append("===PKG-MIRROR-GUESSES===")
for h in ["mirror.amer-1.aivencloud.net","packages.aivencloud.net","repo.aivencloud.net","yum.aivencloud.net","rpm.aivencloud.net","koji.aivencloud.net","kernel.aivencloud.net"]:
    try:
        ip=socket.gethostbyname(h); S.append(f"  {h} -> {ip}")
    except Exception as e: S.append(f"  {h} -> NXDOMAIN")
S.append("===DNF-FROM-BUILD===")
S.append(sh("command -v dnf yum apt apk 2>&1; cat /etc/os-release 2>/dev/null | head -3; ls /etc/yum.repos.d /etc/apk/repositories 2>&1 | head"))
S.append("===TRY-FETCH-KERNEL===")
S.append(sh("for u in https://mirror.amer-1.aivencloud.net/ https://packages.aivencloud.net/ ; do echo \"## $u\"; curl -sk -m 8 -o /dev/null -w '%{http_code}\\n' $u; done"))
open("/tmp/bbh_recon.txt","w").write("\n".join(S)[:40000])
