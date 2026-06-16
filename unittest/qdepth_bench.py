"""Benchmark submission quiescence depth at movetime 2000ms."""
import importlib.util
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from gui.ubgi_client import UBGIEngine

spec = importlib.util.spec_from_file_location(
    "verify_bench",
    os.path.join(os.path.dirname(__file__), "verify_bench.py"),
)
vb = importlib.util.module_from_spec(spec)
spec.loader.exec_module(vb)

ROOT = vb.ROOT
SUB = vb.ENGINE_MAP["submission"][0]
PROBES = 10
MOVETIME = 2000
QDEPTHS = [2, 4, 6, 8, 10]


def probe(qdepth):
    path, algo, _ = vb.ENGINE_MAP["submission"]
    params = ["Algorithm=submission", f"QuiescenceDepth={qdepth}"]
    depths = []
    times = []
    for _ in range(PROBES):
        uci, forfeit, elapsed, depth = get_move_stats(path, algo, params, [], MOVETIME)
        if forfeit or not uci:
            raise RuntimeError(f"missing bestmove at QuiescenceDepth={qdepth}")
        depths.append(depth)
        times.append(elapsed * 1000.0)
    return sum(depths) / len(depths), sum(times) / len(times)


def get_move_stats(path, algo, params, uci_moves, movetime_ms):
    import subprocess
    import threading
    import time

    kwargs = {
        "args": [os.path.abspath(path)],
        "stdin": subprocess.PIPE,
        "stdout": subprocess.PIPE,
        "stderr": subprocess.DEVNULL,
    }
    if sys.platform == "win32":
        kwargs["creationflags"] = subprocess.CREATE_NO_WINDOW

    proc = subprocess.Popen(**kwargs)
    bestmove = [None]
    last_depth = [0]
    done = threading.Event()

    def reader():
        while not done.is_set():
            raw = proc.stdout.readline()
            if not raw:
                break
            line = raw.decode("utf-8", errors="replace").strip()
            if line.startswith("info "):
                info = UBGIEngine.parse_info(line)
                if info.get("depth") is not None:
                    last_depth[0] = info["depth"]
            elif line.startswith("bestmove"):
                parts = line.split()
                if len(parts) >= 2:
                    bestmove[0] = parts[1]
                done.set()
                break

    def send(cmd):
        proc.stdin.write((cmd + "\n").encode())
        proc.stdin.flush()

    send("ubgi")
    while True:
        line = proc.stdout.readline().decode("utf-8", errors="replace").strip()
        if line in ("ubgiok", "uciok"):
            break

    send(f"setoption name Algorithm value {algo}")
    for pr in params or []:
        k, v = pr.split("=", 1)
        send(f"setoption name {k} value {v}")

    pos = "position startpos"
    if uci_moves:
        pos += " moves " + " ".join(uci_moves)
    send(pos)
    send("isready")
    while proc.stdout.readline().decode("utf-8", errors="replace").strip() != "readyok":
        pass

    t0 = time.time()
    send(f"go movetime {movetime_ms}")
    th = threading.Thread(target=reader, daemon=True)
    th.start()
    hard_cap = t0 + 30.0
    while time.time() < hard_cap and not done.is_set():
        time.sleep(0.01)

    done.set()
    proc.kill()
    try:
        proc.wait(timeout=1)
    except subprocess.TimeoutExpired:
        proc.kill()

    uci = bestmove[0]
    elapsed = time.time() - t0
    forfeit = uci is None or uci == "0000"
    return uci, forfeit, elapsed, last_depth[0]


if __name__ == "__main__":
    print("QuiescenceDepth\tavg_depth\tavg_move_ms")
    for q in QDEPTHS:
        avg_depth, avg_ms = probe(q)
        print(f"{q}\t{avg_depth:.1f}\t{avg_ms:.0f}")
