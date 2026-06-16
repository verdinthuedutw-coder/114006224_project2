"""Fixed UBGI benchmark harness — drains stdout while searching."""
import os
import sys
import subprocess
import threading
import time

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from gui.games.minichess_engine import MiniChessState
from gui.ubgi_client import UBGIEngine

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SUB = os.path.join(ROOT, "build", "minichess-ubgi.exe")

ENGINE_MAP = {
    "submission": (SUB, "submission", ["Algorithm=submission"]),
    "minimax-weak": (os.path.join(ROOT, "build", "minimax-weak-ubgi_rep.exe"), "minimax", []),
    "minimax-strong": (os.path.join(ROOT, "build", "minimax-strong-ubgi_rep.exe"), "minimax", []),
    "boss-pvs": (os.path.join(ROOT, "build", "boss-ubgi_rep.exe"), "pvs", []),
}


def get_move(path, algo, params, uci_moves, movetime_ms=2000, grace_s=10.0, hard_cap_s=30.0):
    """Spawn engine, drain stdout until bestmove or hard timeout. Returns (uci, forfeit)."""
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
    done = threading.Event()

    def reader():
        while not done.is_set():
            raw = proc.stdout.readline()
            if not raw:
                break
            line = raw.decode("utf-8", errors="replace").strip()
            if line.startswith("bestmove"):
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
    while True:
        line = proc.stdout.readline().decode("utf-8", errors="replace").strip()
        if line == "readyok":
            break

    t0 = time.time()
    send(f"go movetime {movetime_ms}")
    th = threading.Thread(target=reader, daemon=True)
    th.start()
    hard_cap = t0 + hard_cap_s
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
    return uci, forfeit, elapsed


def verify_submission_bestmove(trials=10, movetime_ms=2000):
    path, algo, params = ENGINE_MAP["submission"]
    missing = 0
    for _ in range(trials):
        uci, forfeit = get_move(path, algo, params, [], movetime_ms, grace_s=1.5)[:2]
        if forfeit or not uci:
            missing += 1
    return missing


def play_game(white_name, black_name):
    wpath, walgo, wparams = ENGINE_MAP[white_name]
    bpath, balgo, bparams = ENGINE_MAP[black_name]
    state = MiniChessState.initial()
    moves = []
    forfeits = 0
    sub_times = []

    for _ in range(200):
        state.get_legal_actions()
        result, winner = state.check_game_over()

        if result == "win":
            return ("white" if winner == 0 else "black", forfeits, sub_times)
        if result == "draw":
            return ("draw", forfeits, sub_times)
        if not state.legal_actions:
            return ("black" if state.player == 0 else "white", forfeits, sub_times)

        is_sub = (state.player == 0 and white_name == "submission") or (
            state.player == 1 and black_name == "submission"
        )
        if state.player == 0:
            path, algo, params = wpath, walgo, wparams
        else:
            path, algo, params = bpath, balgo, bparams

        uci, forfeit, elapsed = get_move(path, algo, params, moves, 2000)
        if is_sub:
            sub_times.append(elapsed)
        if forfeit:
            forfeits += 1
            return ("black" if state.player == 0 else "white", forfeits, sub_times)

        mv = UBGIEngine.uci_to_move(uci)
        if mv not in state.legal_actions:
            forfeits += 1
            return ("black" if state.player == 0 else "white", forfeits, sub_times)

        state = state.next_state(mv)
        moves.append(uci)

    return ("draw", forfeits, sub_times)


def run_matchup(opp_name, games=6):
    w = l = d = f = 0
    sub_times = []
    for g in range(games):
        if g % 2 == 0:
            result, ff, times = play_game("submission", opp_name)
            sub_white = True
        else:
            result, ff, times = play_game(opp_name, "submission")
            sub_white = False
        f += ff
        sub_times.extend(times)
        if result == "draw":
            d += 1
        elif (sub_white and result == "white") or (not sub_white and result == "black"):
            w += 1
        else:
            l += 1
    avg_ms = (sum(sub_times) / len(sub_times) * 1000.0) if sub_times else 0.0
    return w, l, d, f, avg_ms


if __name__ == "__main__":
    missing = verify_submission_bestmove(10)
    assert missing == 0, f"submission missing bestmove on {missing}/10 probes"

    opponents = ["minimax-weak", "minimax-strong", "boss-pvs"]
    for opp in opponents:
        w, l, d, f, avg_ms = run_matchup(opp, 6)
        assert f == 0, f"forfeits={f} vs {opp}"
        print(f"{opp}\tW={w}\tL={l}\tD={d}\tavg_move_ms={avg_ms:.0f}")
