# Local VM Auto-Scaling to GCP — CPU Stress Demo

Demonstrates **local VM resource monitoring with automatic scale-out to Google Cloud Platform** when CPU usage exceeds a configurable threshold (default: 75%).

---

## Objective

> Create a local VM and implement a mechanism to monitor resource usage. Configure it to auto-scale to a public cloud (GCP) when resource usage exceeds 75%.

This project fulfills that objective by:

1. Running CPU-burn worker processes on the **local machine**.
2. Continuously monitoring local CPU via `psutil`.
3. When local CPU ≥ 75%, automatically provisioning and loading **GCP VM instances** (via a Managed Instance Group) to absorb the workload.
4. Providing a clean **graceful shutdown** that kills all burners and scales the MIG back to a 1-instance standby.

---

## Architecture

```
┌─────────────────────────────────────────────────────────────────────┐
│                         LOCAL MACHINE                               │
│                                                                     │
│   ┌──────────────┐     CPU % via      ┌──────────────────────────┐ │
│   │  burner.py   │────── psutil ──────▶  Monitor Loop (5s tick)  │ │
│   │  main loop   │                    └──────────┬───────────────┘ │
│   └──────────────┘                               │                 │
│                                                  │ CPU < 75%?      │
│   ┌──────────────────────────┐                   │                 │
│   │  Local CPU-Burn Workers  │◀── spawn ─────────┤                 │
│   │  (multiprocessing, GIL-  │                   │ CPU ≥ 75%?      │
│   │   free, one per core)    │                   ▼                 │
│   └──────────────────────────┘         Scale-Out Decision          │
│                                                  │                 │
└──────────────────────────────────────────────────┼─────────────────┘
                                                   │ Local saturated
                                                   ▼
┌─────────────────────────────────────────────────────────────────────┐
│                   GOOGLE CLOUD PLATFORM (GCP)                       │
│                                                                     │
│   ┌─────────────────────────────────────────────────────────────┐  │
│   │           Managed Instance Group (MIG)                      │  │
│   │           apoorv-stress-test-managed-instance-group         │  │
│   │           Zone: us-central1-c   Max replicas: 5             │  │
│   │                                                             │  │
│   │   ┌────────────┐  ┌────────────┐  ┌────────────┐           │  │
│   │   │  GCP VM 1  │  │  GCP VM 2  │  │  GCP VM N  │  ...      │  │
│   │   │            │  │            │  │            │           │  │
│   │   │ CPU-burn   │  │ CPU-burn   │  │ CPU-burn   │           │  │
│   │   │ bash loop  │  │ bash loop  │  │ bash loop  │           │  │
│   │   │ (via SSH)  │  │ (via SSH)  │  │ (via SSH)  │           │  │
│   │   └────────────┘  └────────────┘  └────────────┘           │  │
│   └─────────────────────────────────────────────────────────────┘  │
│                                                                     │
│   Control plane: gcloud CLI  ←→  SSH (key-based, BatchMode)        │
└─────────────────────────────────────────────────────────────────────┘


Scale-Out Priority (inside scale_out()):
  1. Local CPU < 75%  →  add another local burner process
  2. Local CPU ≥ 75%, tracked GCP instance has headroom  →  add burner there
  3. Local saturated, untracked MIG instance is RUNNING  →  onboard it, add burner
  4. All instances saturated, MIG size < MAX  →  resize MIG +1, wait for SSH, add burner
  5. MIG at MAX_GCP_INSTANCES  →  hold steady


Shutdown flow (Ctrl-C / SIGINT):
  local burners ──kill──▶  remote burners ──kill──▶  MIG resize → 1 standby
```

---

## Files

| File | Purpose |
|---|---|
| `burner.py` | Main script — monitors local CPU, drives scale-out to GCP MIG, watchdog, graceful shutdown |
| `shutdown_instances.py` | Standalone cleanup — kills all remote burners, removes autoscaler, resizes MIG to 1 |

---

## Prerequisites

| Requirement | Notes |
|---|---|
| Python 3.8+ | Standard library + `psutil` |
| `psutil` | `pip install psutil` |
| `gcloud` CLI | Authenticated with project access |
| GCP Managed Instance Group | Must already exist (no autoscaler attached) |
| SSH key | `~/.ssh/google_compute_engine` with access to MIG instances |

---

## Configuration

Edit the constants at the top of `burner.py` (and mirror in `shutdown_instances.py` if needed):

```python
MIG_NAME          = "apoorv-stress-test-managed-instance-group"
ZONE              = "us-central1-c"
PROJECT           = "test-test-test-385516"
CPU_THRESHOLD     = 75.0   # % — scale-out triggers above this
MAX_GCP_INSTANCES = 5      # Hard cap on MIG replicas
SSH_KEY           = "/home/apoorv/.ssh/google_compute_engine"
SSH_USER          = "apoorv"
```

---

## How to Use

### 1. Run the stress scaler

```bash
python3 burner.py
```

**What happens:**

- Seeds half your local CPU cores with burn processes.
- Every 5 seconds: reads local CPU, prints a status line, and calls `scale_out()` if not fully saturated.
- When local CPU crosses 75%, it discovers running MIG instances (or creates new ones via `gcloud compute instance-groups managed resize`) and launches `nohup bash -c 'while :; do :; done'` burners on them over SSH.
- A watchdog (every 30 s) restarts any burner that dies unexpectedly.

**Sample output:**

```
🚀 CPU stress scaler starting

   Threshold      : 75.0%
   Local cores    : 8
   Max GCP inst.  : 5
   MIG            : apoorv-stress-test-managed-instance-group @ us-central1-c

Seeding with 4 local burner processes...
   [+] Local burner PID 12301  (total local: 1)
   ...

[14:02:10] Local CPU: 82.4%  |  Burners: 4
------------------------------------------------------------
   → Local saturated — onboarding existing GCP instance apoorv-stress-test-managed-instance-group-xxxx
   [⏳] Waiting for SSH on apoorv-stress-test-managed-instance-group-xxxx...
   [✓] apoorv-stress-test-managed-instance-group-xxxx is SSH-ready
   [+] Remote burner on apoorv-stress-test-managed-instance-group-xxxx: PID 3847
```

### 2. Stop the scaler (graceful)

Press **Ctrl-C**. The signal handler:

1. Kills all tracked remote burner PIDs via SSH.
2. Runs `pkill` on every running MIG instance to catch any orphaned burners.
3. Removes the MIG autoscaler (if present) so manual resize is not blocked.
4. Resizes MIG back to 1 standby instance.
5. Kills all local burner processes.

### 3. Emergency / standalone cleanup

If `burner.py` crashes or is killed without cleanup:

```bash
python3 shutdown_instances.py
```

This script independently:
- Lists all MIG instances.
- SSH-kills burner bash loops on every RUNNING instance.
- Removes the autoscaler.
- Resizes the MIG to 1.

---

## How It Satisfies the Objective

| Requirement | Implementation |
|---|---|
| Create a local VM | Local machine acts as the primary compute node; `multiprocessing` spawns GIL-free burn workers pinned to real CPU cores |
| Monitor resource usage | `psutil.cpu_percent(interval=1)` polled every 5 seconds in the main loop |
| Auto-scale to public cloud when > 75% | `scale_out()` triggers on `local_cpu >= CPU_THRESHOLD`; calls `gcloud compute instance-groups managed resize` to add GCP VMs |
| Cloud provider | Google Cloud Platform — Managed Instance Group in `us-central1-c` |
| Deployment | CPU burn workloads deployed to GCP VMs over SSH using a pre-authorised key; zero manual steps after `python3 burner.py` |

---

## Key Design Decisions

**Why `multiprocessing` instead of `threading` for local burners?**
Python's GIL prevents threads from running CPU-bound code in parallel. `multiprocessing` spawns real OS processes, each pinning a full physical core.

**Why direct SSH instead of `gcloud compute ssh`?**
`gcloud compute ssh` pushes OS-login metadata on every call, adding several seconds of overhead per command. Direct SSH with a pre-placed key (`-o BatchMode=yes`) is fast enough for the 5-second monitoring loop.

**Why awk for remote CPU measurement?**
The GCP instances are ubuntu-minimal images with no Python. The `awk` one-liner reads `/proc/stat` twice with a 1-second sleep and computes CPU% — zero dependencies, works everywhere.

**Why manual MIG resize instead of a GCP autoscaler?**
A GCP autoscaler competes with manual resize calls and can block or reverse them. Removing the autoscaler and resizing manually gives deterministic, immediate control from the local script.
