import psutil
import threading
import multiprocessing
import time
import subprocess
import sys
import signal
import functools

# Flush every print immediately so log files are always up to date
print = functools.partial(print, flush=True)

# ─── Configuration ────────────────────────────────────────────────────────────
MIG_NAME          = "apoorv-stress-test-managed-instance-group"
ZONE              = "us-central1-c"
PROJECT           = "test-test-test-385516"
CPU_THRESHOLD     = 75.0
MAX_GCP_INSTANCES = 5   # Hard cap — matches original autoscaler maxNumReplicas

# ─── Global state ─────────────────────────────────────────────────────────────
running        = True
local_burners  = []   # list of multiprocessing.Process
remote_burners = {}   # {instance_name: [pid, pid, ...]}
state_lock     = threading.Lock()



# ══════════════════════════════════════════════════════════════════════════════
# LOCAL BURNER  (multiprocessing bypasses Python GIL → true multi-core burn)
# ══════════════════════════════════════════════════════════════════════════════

def _cpu_burn():
    """Runs in its own OS process — no GIL, pins one full core."""
    while True:
        x = 0
        for i in range(10_000_000):
            x += i * i

def get_local_cpu():
    return psutil.cpu_percent(interval=1)

def local_burner_add():
    p = multiprocessing.Process(target=_cpu_burn, daemon=True)
    p.start()
    with state_lock:
        local_burners.append(p)
    print(f"   [+] Local burner PID {p.pid}  (total local: {len(local_burners)})")
    return p

def local_burner_kill(p):
    p.terminate()
    p.join(timeout=3)
    if p.is_alive():
        p.kill()
    with state_lock:
        if p in local_burners:
            local_burners.remove(p)


# ══════════════════════════════════════════════════════════════════════════════
# SSH HELPER — direct ssh, bypasses gcloud compute ssh metadata push overhead
# ══════════════════════════════════════════════════════════════════════════════

SSH_KEY  = "/home/apoorv/.ssh/google_compute_engine"
SSH_USER = "apoorv"

def _run(cmd, timeout=30):
    """subprocess.run wrapper — returns a failed CompletedProcess on TimeoutExpired."""
    try:
        return subprocess.run(cmd, capture_output=True, text=True, timeout=timeout)
    except subprocess.TimeoutExpired:
        print(f"   [WARN] command timed out: {' '.join(str(x) for x in cmd[:4])}")
        return subprocess.CompletedProcess(cmd, returncode=1, stdout="", stderr="timeout")


def get_instance_ip(instance_name):
    r = _run(
        ["gcloud", "compute", "instances", "describe", instance_name,
         f"--project={PROJECT}", f"--zone={ZONE}",
         "--format=value(networkInterfaces[0].accessConfigs[0].natIP)"],
        timeout=15
    )
    return r.stdout.strip() or None


def ssh_run(instance_name, command, timeout=20):
    ip = get_instance_ip(instance_name)
    if not ip:
        return subprocess.CompletedProcess([], returncode=1, stdout="", stderr="no IP")
    return _run(
        ["ssh",
         "-i", SSH_KEY,
         "-o", "StrictHostKeyChecking=no",
         "-o", "ConnectTimeout=8",
         "-o", "BatchMode=yes",
         f"{SSH_USER}@{ip}",
         command],
        timeout=timeout
    )


# ══════════════════════════════════════════════════════════════════════════════
# GCP / MIG MANAGEMENT
# NOTE: MIG has NO autoscaler — manual resize works correctly.
# ══════════════════════════════════════════════════════════════════════════════

def mig_target_size():
    r = _run(
        ["gcloud", "compute", "instance-groups", "managed", "describe",
         MIG_NAME, f"--project={PROJECT}", f"--zone={ZONE}",
         "--format=value(targetSize)"],
        timeout=15
    )
    try:
        return int(r.stdout.strip())
    except Exception:
        return 0

def mig_resize(new_size):
    """Resize to an ABSOLUTE count — gcloud does not support relative +1 syntax."""
    r = _run(
        ["gcloud", "compute", "instance-groups", "managed", "resize",
         MIG_NAME, f"--size={new_size}",
         f"--project={PROJECT}", f"--zone={ZONE}"],
        timeout=30
    )
    if r.returncode != 0:
        print(f"   [WARN] mig_resize → {r.stderr.strip()[:120]}")
    return r.returncode == 0

def mig_running_instances():
    """Names of fully-running MIG instances (currentAction == NONE)."""
    r = _run(
        ["gcloud", "compute", "instance-groups", "managed", "list-instances",
         MIG_NAME, f"--project={PROJECT}", f"--zone={ZONE}",
         "--format=csv[no-heading](instance.basename(),currentAction)"],
        timeout=15
    )
    result = []
    for line in r.stdout.strip().splitlines():
        parts = line.split(",")
        if len(parts) >= 2 and parts[1].strip() == "NONE":
            result.append(parts[0].strip())
    return result

def mig_delete_instance(instance_name):
    r = _run(
        ["gcloud", "compute", "instance-groups", "managed", "delete-instances",
         MIG_NAME, f"--instances={instance_name}",
         f"--project={PROJECT}", f"--zone={ZONE}", "--force"],
        timeout=60
    )
    return r.returncode == 0

def wait_for_ssh(instance_name, timeout=180):
    print(f"   [⏳] Waiting for SSH on {instance_name}...")
    deadline = time.time() + timeout
    while time.time() < deadline:
        try:
            r = ssh_run(instance_name, "echo ok", timeout=15)
            if r.returncode == 0 and "ok" in r.stdout:
                print(f"   [✓] {instance_name} is SSH-ready")
                return True
        except subprocess.TimeoutExpired:
            pass
        time.sleep(8)
    print(f"   [✗] {instance_name}: SSH timed out")
    return False


# ══════════════════════════════════════════════════════════════════════════════
# REMOTE CPU MONITORING
# Pure awk with getline — no bash variable interpolation, no Python dependency,
# works on ubuntu-minimal out of the box.
# ══════════════════════════════════════════════════════════════════════════════

# Reads /proc/stat twice (1s apart) via awk getline and prints CPU %.
# Single-quoted awk program avoids all shell quoting issues.
_CPU_CMD = (
    "awk 'BEGIN{"
    'cmd="grep ^cpu /proc/stat";'
    "cmd|getline l1;close(cmd);"
    'system("sleep 1");'
    "cmd|getline l2;close(cmd);"
    "n=split(l1,a);for(i=2;i<=n;i++){t1+=a[i];if(i==5)i1=a[i]};"
    "n=split(l2,b);for(i=2;i<=n;i++){t2+=b[i];if(i==5)i2=b[i]};"
    'printf "%.1f\\n",100*((t2-t1)-(i2-i1))/(t2-t1)}'
    "'"
)

def get_instance_cpu(instance_name):
    """Returns CPU % of a GCP instance, or None if unreachable."""
    try:
        r = ssh_run(instance_name, _CPU_CMD, timeout=15)
        if r.returncode == 0:
            return float(r.stdout.strip())
    except Exception:
        pass
    return None


# ══════════════════════════════════════════════════════════════════════════════
# REMOTE BURNER MANAGEMENT
# Pure bash infinite loop — works on ubuntu-minimal, zero dependencies
# ══════════════════════════════════════════════════════════════════════════════

# Spins one core at 100%; nohup + & detaches from SSH session
_BURNER_CMD = "nohup bash -c 'while :; do :; done' >/dev/null 2>&1 & echo $!"

def remote_burner_start(instance_name):
    """Launch one CPU-burn process on remote instance; returns PID or None."""
    r = ssh_run(instance_name, _BURNER_CMD, timeout=30)
    if r.returncode == 0:
        try:
            pid = int(r.stdout.strip().split()[-1])
            print(f"   [+] Remote burner on {instance_name}: PID {pid}")
            return pid
        except Exception:
            pass
    print(f"   [WARN] remote_burner_start failed on {instance_name}: {r.stderr[:100]}")
    return None

def remote_burner_kill(instance_name, pid):
    ssh_run(instance_name, f"kill {pid} 2>/dev/null; true", timeout=10)

def remote_burner_alive(instance_name, pid):
    r = ssh_run(instance_name, f"kill -0 {pid} 2>/dev/null && echo Y || echo N", timeout=10)
    return r.returncode == 0 and "Y" in r.stdout

def remote_add(instance_name):
    pid = remote_burner_start(instance_name)
    if pid:
        with state_lock:
            remote_burners.setdefault(instance_name, []).append(pid)
    return pid

def remote_kill(instance_name, pid):
    remote_burner_kill(instance_name, pid)
    with state_lock:
        if pid in remote_burners.get(instance_name, []):
            remote_burners[instance_name].remove(pid)


# ══════════════════════════════════════════════════════════════════════════════
# WATCHDOG — restart any burner that dies without user instruction
# ══════════════════════════════════════════════════════════════════════════════

def watchdog():
    # Local
    with state_lock:
        dead_local = [p for p in local_burners if not p.is_alive()]
    for p in dead_local:
        print(f"   [↺] Local PID {p.pid} died — restarting")
        with state_lock:
            if p in local_burners:
                local_burners.remove(p)
        local_burner_add()

    # Remote
    for inst in list(remote_burners.keys()):
        with state_lock:
            pids = list(remote_burners.get(inst, []))
        for pid in pids:
            if not remote_burner_alive(inst, pid):
                print(f"   [↺] Remote PID {pid} on {inst} died — restarting")
                with state_lock:
                    if pid in remote_burners.get(inst, []):
                        remote_burners[inst].remove(pid)
                remote_add(inst)


# ══════════════════════════════════════════════════════════════════════════════
# STATUS DISPLAY
# ══════════════════════════════════════════════════════════════════════════════

def print_status():
    local_cpu = get_local_cpu()
    print(f"\n[{time.strftime('%H:%M:%S')}] Local CPU: {local_cpu:.1f}%  |  Burners: {len(local_burners)}")
    for inst, pids in remote_burners.items():
        cpu = get_instance_cpu(inst)
        cpu_str = f"{cpu:.1f}%" if cpu is not None else "unreachable"
        print(f"   ↳ GCP {inst}: CPU {cpu_str}  |  Burners: {len(pids)}")
    print("-" * 60)
    return local_cpu


# ══════════════════════════════════════════════════════════════════════════════
# SCALE-OUT LOGIC
# Priority: fill local → onboard existing MIG instances → fill them →
#           create new MIG instance → repeat until MAX_GCP_INSTANCES
# ══════════════════════════════════════════════════════════════════════════════

def scale_out():
    """Add one burner to wherever there is headroom, in priority order."""
    local_cpu = get_local_cpu()

    # 1. Local still has headroom → add another local burner
    if local_cpu < CPU_THRESHOLD:
        local_burner_add()
        return

    # 2. Check instances already being tracked for headroom
    for inst, pids in list(remote_burners.items()):
        cpu = get_instance_cpu(inst)
        if cpu is not None and cpu < CPU_THRESHOLD:
            print(f"   → {inst} at {cpu:.1f}% — adding burner")
            remote_add(inst)
            return

    # 3. LOCAL IS SATURATED: check for running MIG instances not yet tracked.
    #    This is the first GCP step — onboard the existing instance before
    #    creating any new ones.
    all_running = mig_running_instances()
    untracked = [i for i in all_running if i not in remote_burners]
    if untracked:
        inst = untracked[0]
        print(f"   → Local saturated — onboarding existing GCP instance {inst}")
        if wait_for_ssh(inst, timeout=120):
            with state_lock:
                remote_burners[inst] = []
            remote_add(inst)
        return

    # 4. All tracked instances are also saturated → create a new one
    cur_size = mig_target_size()
    if cur_size >= MAX_GCP_INSTANCES:
        print(f"   → At MAX_GCP_INSTANCES ({MAX_GCP_INSTANCES}), holding steady")
        return
    new_size = cur_size + 1
    print(f"   → All instances at threshold — scaling MIG {cur_size} → {new_size}")
    if not mig_resize(new_size):
        return

    # Wait for the new instance to reach RUNNING and accept SSH
    print("   [⏳] Waiting for new instance to reach RUNNING state...")
    for _ in range(24):   # up to ~4 minutes
        time.sleep(10)
        all_running = mig_running_instances()
        new_insts = [i for i in all_running if i not in remote_burners]
        if new_insts:
            new_inst = new_insts[0]
            if wait_for_ssh(new_inst, timeout=120):
                with state_lock:
                    remote_burners[new_inst] = []
                remote_add(new_inst)
            return

    print("   [WARN] Timed out waiting for new GCP instance")


# ══════════════════════════════════════════════════════════════════════════════
# GRACEFUL SHUTDOWN — kill burners one-by-one, delete empty instances
# ══════════════════════════════════════════════════════════════════════════════

def mig_delete_autoscaler():
    """Delete the MIG's autoscaler if one exists (required before manual resize)."""
    r = _run(
        ["gcloud", "compute", "instance-groups", "managed", "stop-autoscaling",
         MIG_NAME, f"--project={PROJECT}", f"--zone={ZONE}"],
        timeout=30
    )
    if r.returncode == 0:
        print("   [✓] Autoscaler deleted")
    # Ignore errors — autoscaler may not exist


def graceful_shutdown():
    global running
    running = False
    print("\n[🛑] Stop received. Killing burners one by one...\n")

    # Kill remote burners by tracked PIDs
    for inst in list(remote_burners.keys()):
        for pid in list(remote_burners.get(inst, [])):
            print(f"   [-] Kill remote PID {pid} on {inst}")
            remote_kill(inst, pid)
            time.sleep(0.5)

    # Also pkill any lingering burner bash processes on ALL running MIG instances
    # (catches burners if code crashed and PID tracking was lost)
    for inst in mig_running_instances():
        try:
            ssh_run(inst, "pkill -f 'while :; do :; done' 2>/dev/null; true", timeout=10)
        except Exception:
            pass

    # Remove autoscaler so manual resize is not blocked, then shrink to 1 standby
    print("   [→] Removing autoscaler and resizing MIG to 1...")
    mig_delete_autoscaler()
    mig_resize(1)

    # Kill local burners
    for p in list(local_burners):
        print(f"   [-] Kill local PID {p.pid}")
        local_burner_kill(p)
        time.sleep(0.3)

    print("\n[✓] All burners stopped. MIG resized to 1 standby.")


signal.signal(signal.SIGINT, lambda s, f: (graceful_shutdown(), sys.exit(0)))


# ══════════════════════════════════════════════════════════════════════════════
# MAIN LOOP
# ══════════════════════════════════════════════════════════════════════════════

def is_all_saturated(local_cpu):
    """
    True only when local AND every tracked GCP instance are all at threshold
    AND we've reached MAX_GCP_INSTANCES.

    BUG THAT WAS HERE: when remote_burners was empty and local >= threshold,
    the for-loop over remote_burners.keys() never ran, so all_saturated stayed
    True and scale_out() was never called → GCP instances never started.
    Fix: require at least MAX_GCP_INSTANCES tracked before declaring done.
    """
    if local_cpu < CPU_THRESHOLD:
        return False
    if len(remote_burners) < MAX_GCP_INSTANCES:
        return False   # ← KEY FIX: don't stop just because remote_burners is empty
    for inst in list(remote_burners.keys()):
        cpu = get_instance_cpu(inst)
        if cpu is None or cpu < CPU_THRESHOLD:
            return False
    return True


def main():
    global running
    n_cpus = multiprocessing.cpu_count()
    print("🚀 CPU stress scaler starting\n")
    print(f"   Threshold      : {CPU_THRESHOLD}%")
    print(f"   Local cores    : {n_cpus}")
    print(f"   Max GCP inst.  : {MAX_GCP_INSTANCES}")
    print(f"   MIG            : {MIG_NAME} @ {ZONE}\n")

    # Seed with half the local cores to start ramping toward threshold
    initial = max(2, n_cpus // 2)
    print(f"Seeding with {initial} local burner processes...")
    for _ in range(initial):
        local_burner_add()
        time.sleep(0.1)

    last_watchdog     = time.time()
    WATCHDOG_INTERVAL = 30   # seconds
    STABILIZE_DELAY   = 5    # seconds after adding a burner before re-checking

    while running:
        local_cpu = print_status()

        if not is_all_saturated(local_cpu):
            scale_out()
            time.sleep(STABILIZE_DELAY)

        # Periodic watchdog: replace any dead burners
        if time.time() - last_watchdog > WATCHDOG_INTERVAL:
            watchdog()
            last_watchdog = time.time()

        time.sleep(5)


try:
    main()
except KeyboardInterrupt:
    graceful_shutdown()
