import subprocess
import functools

print = functools.partial(print, flush=True)

MIG_NAME = "apoorv-stress-test-managed-instance-group"
ZONE     = "us-central1-c"
PROJECT  = "test-test-test-385516"
SSH_KEY  = "/home/apoorv/.ssh/google_compute_engine"
SSH_USER = "apoorv"
MIN_SIZE = 1


def _run(cmd, timeout=30):
    try:
        return subprocess.run(cmd, capture_output=True, text=True, timeout=timeout)
    except subprocess.TimeoutExpired:
        return subprocess.CompletedProcess(cmd, returncode=1, stdout="", stderr="timeout")


def list_instances():
    r = _run([
        "gcloud", "compute", "instance-groups", "managed", "list-instances",
        MIG_NAME, f"--project={PROJECT}", f"--zone={ZONE}",
        "--format=csv[no-heading](instance.basename(),currentAction,status)"
    ])
    instances = []
    for line in r.stdout.strip().splitlines():
        parts = line.split(",")
        if len(parts) >= 3:
            instances.append({"name": parts[0].strip(), "action": parts[1].strip(), "status": parts[2].strip()})
    return instances


def get_instance_ip(name):
    r = _run([
        "gcloud", "compute", "instances", "describe", name,
        f"--project={PROJECT}", f"--zone={ZONE}",
        "--format=value(networkInterfaces[0].accessConfigs[0].natIP)"
    ])
    return r.stdout.strip() or None


def kill_burners(name):
    ip = get_instance_ip(name)
    if not ip:
        print(f"   [WARN] Could not get IP for {name}, skipping burner kill")
        return
    r = _run([
        "ssh", "-i", SSH_KEY,
        "-o", "StrictHostKeyChecking=no", "-o", "ConnectTimeout=8", "-o", "BatchMode=yes",
        f"{SSH_USER}@{ip}",
        "pkill -f 'while :; do :; done' 2>/dev/null; echo done"
    ], timeout=15)
    if r.returncode == 0:
        print(f"   [✓] Burners killed on {name}")
    else:
        print(f"   [WARN] Could not kill burners on {name}: {r.stderr.strip()[:80]}")


def delete_autoscaler():
    r = _run([
        "gcloud", "compute", "instance-groups", "managed", "stop-autoscaling",
        MIG_NAME, f"--project={PROJECT}", f"--zone={ZONE}"
    ])
    if r.returncode == 0:
        print("   [✓] Autoscaler removed")


def resize_to_min():
    r = _run([
        "gcloud", "compute", "instance-groups", "managed", "resize",
        MIG_NAME, f"--size={MIN_SIZE}", f"--project={PROJECT}", f"--zone={ZONE}"
    ])
    if r.returncode == 0:
        print(f"   [✓] MIG resized to {MIN_SIZE}")
    else:
        print(f"   [WARN] Resize failed: {r.stderr.strip()[:120]}")


def main():
    instances = list_instances()
    print(f"\nFound {len(instances)} instance(s) in MIG:\n")
    for inst in instances:
        print(f"   {inst['name']}  [{inst['status']} / {inst['action']}]")

    running = [i for i in instances if i["status"] == "RUNNING"]

    print(f"\nKilling burners on {len(running)} running instance(s)...")
    for inst in running:
        kill_burners(inst["name"])

    print("\nRemoving autoscaler (if any)...")
    delete_autoscaler()

    print(f"\nResizing MIG down to {MIN_SIZE}...")
    resize_to_min()

    print("\nDone. Current MIG state:\n")
    for inst in list_instances():
        print(f"   {inst['name']}  [{inst['status']} / {inst['action']}]")


if __name__ == "__main__":
    main()
