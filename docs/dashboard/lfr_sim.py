#!/usr/bin/env python3
"""
LFR Simulator — ponte WebSocket para o dashboard, no protocolo BLE v4 do firmware 4.0.0.

Fala o mesmo JSON que o ESP32 troca por BLE (spec §4.5):
  • Notify (ESP32 → app, característica 0xABCD)  →  servidor WS → cliente
  • Write  (app → ESP32, característica 0xABCE)  →  cliente WS → servidor

Protocolo (cada mensagem ≤ 180 bytes)
──────────────────────────────────────
  Servidor → cliente
    {"t":"info","name":"LFR-RACER-01","fw":"4.0.0-sim","mode":"SIM","cfgerr":0,"stk":[0,0]}
                                     ao conectar e em resposta a get, antes dos cfg
    {"t":"tel", "pos":erro [-1,1], "corr":saída do PID externo [-1,1],
                "vL":duty, "vR":duty (±1023), "dt":µs, "rpmL","rpmR","spL","spR":int,
                "s":[8 × 0–100], "st":estado (0 IDLE … 5 ERROR), "lost":0|1}      30 Hz
    {"t":"sta", "bat":%, "vb":V, "lap":s, "laps":[s…], "err":código, "prof":n,
                "dirty":0|1, "ovr":µs}                                             2 Hz
    {"t":"cfg","g":"hw"|"pid"|"vel"|"spd"|"misc", <chave>:<valor>…}
    {"t":"cfg","g":"meta","prof":n,"dirty":0|1,"fw":"4.0.0-sim"}
    {"t":"ack","k":<chave ou comando>,"ok":1[,"v":valor]}
    {"t":"ack","k":<chave ou comando>,"ok":0,
     "err":"range"|"key"|"running"|"dirty"|"parse"|"locked"}
                                     "locked": lock = 1 e carro em ARMED/RUNNING (regra 5.9)
    {"t":"calres","ok":1}            fim da calibração dos sensores (scal)
    {"t":"ecal","side":"L"|"R","ok":1,"ppr":600.0}   fim da calibração de encoder

  Cliente → servidor
    {"t":"set","k":"kp","v":1.2}     valida e aplica ao vivo (não persiste)
    {"t":"get"}                      info + configuração inteira (5 grupos + meta)
    {"t":"save"} {"t":"load"} {"t":"defaults"}
    {"t":"prof","n":2[,"f":1]}       troca de perfil (recusa com "dirty" sem f:1)
    {"t":"scal"}                     calibração dos sensores
    {"t":"start"} {"t":"stop"} {"t":"reset"}
    Legados: {"t":"pid",…} {"t":"pidv",…} {"t":"spd",…} {"t":"rpm",…} {"t":"cal",…}
──────────────────────────────────────
Uso:
    pip install websockets           # só o servidor precisa; o ConfigModel não
    python3 lfr_sim.py
    # Em outra aba: python3 -m http.server 8765
    # Abrir: http://localhost:8765/lfr-cockpit-mock.html
"""

from __future__ import annotations

import asyncio
import json
import math
import random
import time

try:
    import websockets
except ImportError:  # o modelo de configuração (e os testes) não precisa do servidor
    websockets = None

HOST = "localhost"
PORT = 8766
RATE = 30          # Hz — taxa do frame tel
STATUS_EVERY = 15  # frames tel por frame sta (2 Hz)
FW = "4.0.0-sim"

# Tabela de chaves — espelha src/config/RobotConfig.cpp (chave, grupo, mín, máx, hw, ao vivo)
KEYS = [
    ("invL", "hw", 0, 1, True, False), ("invR", "hw", 0, 1, True, False),
    ("swap", "hw", 0, 1, True, False), ("einvL", "hw", 0, 1, True, False),
    ("einvR", "hw", 0, 1, True, False), ("pol", "hw", 0, 1, True, False),
    ("sinv", "hw", 0, 1, True, False), ("chem", "hw", 0, 1, True, False),
    ("lock", "hw", 0, 1, True, False),
    ("vcut", "hw", 5.6, 7.6, True, False), ("vbk", "hw", 0.8, 1.2, True, False),
    ("pprL", "hw", 1, 10000, True, False), ("pprR", "hw", 1, 10000, True, False),
    ("kp", "pid", 0, 20, False, True), ("ki", "pid", 0, 50, False, True),
    ("kd", "pid", 0, 2, False, True), ("dlp", "pid", 0.05, 1, False, True),
    ("mode", "vel", 0, 1, False, False), ("kpv", "vel", 0, 0.05, False, True),
    ("kiv", "vel", 0, 1, False, True), ("kdv", "vel", 0, 0.01, False, True),
    ("rpmMax", "vel", 100, 2000, False, True),
    ("vmax", "spd", 0, 1, False, True), ("vmin", "spd", 0, 1, False, True),
    ("thr", "spd", 0.05, 1, False, True), ("acc", "spd", 0.1, 50, False, True),
    ("vref", "misc", 5.0, 8.4, False, True), ("vcomp", "misc", 0, 1, False, True),
    ("lostMs", "misc", 50, 5000, False, True), ("startMs", "misc", 0, 5000, False, True),
]
INT_KEYS = {"invL", "invR", "swap", "einvL", "einvR", "pol", "sinv", "chem", "lock", "mode",
            "vcomp", "lostMs", "startMs"}
GROUPS = ["hw", "pid", "vel", "spd", "misc"]
FACTORY_HW = {"invL": 0, "invR": 0, "swap": 0, "einvL": 0, "einvR": 0, "pol": 1, "sinv": 0,
              "chem": 1, "lock": 0, "vcut": 6.4, "vbk": 1.0, "pprL": 600.0, "pprR": 600.0}
# Chaves que cada comando legado vira (para o ack "locked", um por chave)
LEGACY_KEYS = {"pid": {"kp": "kp", "ki": "ki", "kd": "kd"},
               "pidv": {"kp": "kpv", "ki": "kiv", "kd": "kdv"},
               "spd": {"max": "vmax", "min": "vmin", "thrs": "thr"},
               "rpm": {"max": "rpmMax"}}
STATE_KINDS = ("get", "save", "load", "defaults", "prof", "scal", "start", "stop", "reset", "cal")
FACTORY_TUNE = {"mode": 0, "kp": 1.0, "ki": 0.0, "kd": 0.03, "dlp": 0.5, "kpv": 0.002,
                "kiv": 0.005, "kdv": 0.0, "vmax": 0.35, "vmin": 0.25, "thr": 0.6, "acc": 2.0,
                "rpmMax": 700.0, "vref": 7.4, "vcomp": 1, "lostMs": 300, "startMs": 1000}
FACTORY_VMAX = [0.35, 0.50, 0.70]


def factory_profile(n):
    p = dict(FACTORY_TUNE)
    p["vmax"] = FACTORY_VMAX[n if 0 <= n < 3 else 0]
    return p


def _compact(obj):
    return json.dumps(obj, separators=(",", ":"))


class ConfigModel:
    """Mesmas regras do CommandHandler do firmware (src/comm/CommandHandler.cpp)."""

    def __init__(self):
        self.hw = dict(FACTORY_HW)
        self.tune = factory_profile(0)
        self.profile = 0
        self.dirty = False
        self.saved_hw = None
        self.saved = {}

    @staticmethod
    def _ack(k, ok=True, err=None, v=None):
        msg = {"t": "ack", "k": k, "ok": 1 if ok else 0}
        if ok and v is not None:
            msg["v"] = v
        if not ok:
            msg["err"] = err
        return _compact(msg)

    def _group(self, g):
        msg = {"t": "cfg", "g": g}
        for k, grp, *_ in KEYS:
            if grp == g:
                msg[k] = self.hw[k] if k in self.hw else self.tune[k]
        return _compact(msg)

    @staticmethod
    def info():
        return _compact({"t": "info", "name": "LFR-RACER-01", "fw": FW, "mode": "SIM",
                         "cfgerr": 0, "stk": [0, 0]})

    def config_dump(self):
        out = [self._group(g) for g in GROUPS]
        out.append(_compact({"t": "cfg", "g": "meta", "prof": self.profile,
                             "dirty": int(self.dirty), "fw": FW}))
        return out

    def _check(self, k, v, running):
        """Valida sem aplicar. Devolve (erro ou None, valor normalizado)."""
        spec = next((s for s in KEYS if s[0] == k), None)
        if spec is None:
            return "key", v
        _, _, lo, hi, _, live = spec
        if running and not live:
            return "running", v
        if isinstance(v, bool) or not isinstance(v, (int, float)) or not math.isfinite(v):
            return "range", v
        if k in INT_KEYS:
            v = round(v)
        if v < lo or v > hi:
            return "range", v
        if k == "vmin" and v > self.tune["vmax"]:
            return "range", v
        if k == "vmax" and v < self.tune["vmin"]:
            return "range", v
        return None, v

    def _apply(self, k, v):
        is_hw = next(s for s in KEYS if s[0] == k)[4]
        (self.hw if is_hw else self.tune)[k] = v
        self.dirty = True

    def _set(self, k, v, running):
        err, v = self._check(k, v, running)
        if err:
            return self._ack(k, False, err)
        self._apply(k, v)
        return self._ack(k, True, v=v)

    def _set_many(self, items, running):
        """Vários itens num comando (legados): 2ª passada para vmin/vmax, como o firmware."""
        results = {}
        for k, v in items:
            err, nv = self._check(k, v, running)
            results[k] = err
            if err is None:
                self._apply(k, nv)
        for k, v in items:
            if results[k] == "range":
                err, nv = self._check(k, v, running)
                results[k] = err
                if err is None:
                    self._apply(k, nv)
        out = []
        for k, _ in items:
            if results[k]:
                out.append(self._ack(k, False, results[k]))
            else:
                out.append(self._ack(k, True, v=self.hw.get(k, self.tune.get(k))))
        return out

    def _refuse_locked(self, data):
        """Modo competição (regra RoboCore 5.9): em ARMED/RUNNING nada do BLE tem efeito."""
        kind = data.get("t")
        if kind == "set":
            return [self._ack(data.get("k", "?"), False, "locked")]
        if kind in LEGACY_KEYS:
            keys = [cfg for js, cfg in LEGACY_KEYS[kind].items() if js in data]
            return [self._ack(k, False, "locked") for k in keys]
        return [self._ack(kind, False, "locked")]

    def _reload(self, n):
        self.hw = dict(self.saved_hw) if self.saved_hw else dict(FACTORY_HW)
        self.tune = dict(self.saved[n]) if n in self.saved else factory_profile(n)
        self.profile = n
        self.dirty = False

    def command(self, raw, running=False):
        """Processa um comando e devolve a lista de respostas JSON (strings)."""
        try:
            data = json.loads(raw)
        except ValueError:
            return [self._ack("?", False, "parse")]
        if not isinstance(data, dict):
            return [self._ack("?", False, "parse")]
        kind = data.get("t")
        if running and self.hw.get("lock") and (kind == "set" or kind in LEGACY_KEYS
                                                 or kind in STATE_KINDS):
            return self._refuse_locked(data)
        if kind == "set":
            if "v" not in data:
                return [self._ack("?", False, "parse")]
            return [self._set(data.get("k", "?"), data.get("v"), running)]
        if kind == "pid":
            items = [(k, data[k]) for k in ("kp", "ki", "kd") if k in data]
            return self._set_many(items, running) if items else [self._ack("?", False, "parse")]
        if kind == "pidv":
            mapping = {"kp": "kpv", "ki": "kiv", "kd": "kdv"}
            items = [(mapping[k], data[k]) for k in mapping if k in data]
            return self._set_many(items, running) if items else [self._ack("?", False, "parse")]
        if kind == "spd":  # legado em 0–255; "base" não existe mais (F-15)
            items = []
            if isinstance(data.get("max"), (int, float)):
                items.append(("vmax", data["max"] / 255.0))
            if isinstance(data.get("min"), (int, float)):
                items.append(("vmin", data["min"] / 255.0))
            if "thrs" in data:
                items.append(("thr", data["thrs"]))
            return self._set_many(items, running) if items else [self._ack("?", False, "parse")]
        if kind == "rpm":
            if "max" not in data:
                return [self._ack("?", False, "parse")]
            return self._set_many([("rpmMax", data["max"])], running)
        if kind == "get":
            return [self.info()] + self.config_dump()
        if kind in ("save", "load", "defaults", "prof") and running:
            return [self._ack(kind, False, "running")]
        if kind == "save":
            self.saved_hw = dict(self.hw)
            self.saved[self.profile] = dict(self.tune)
            self.dirty = False
            return [self._ack("save")]
        if kind == "load":
            self._reload(self.profile)
            return [self._ack("load")] + self.config_dump()
        if kind == "defaults":
            self.hw = dict(FACTORY_HW)
            self.tune = factory_profile(self.profile)
            self.dirty = True
            return [self._ack("defaults")] + self.config_dump()
        if kind == "prof":
            n = data.get("n")
            if isinstance(n, bool) or not isinstance(n, int) or not 0 <= n < 3:
                return [self._ack("prof", False, "range")]
            if self.dirty and not data.get("f"):
                return [self._ack("prof", False, "dirty")]
            self._reload(n)
            return [self._ack("prof")] + self.config_dump()
        if kind == "cal" and running:
            return [self._ack("cal", False, "running")]
        if kind in ("start", "stop", "reset", "scal", "cal"):
            return [self._ack(kind)]
        return [self._ack(kind or "?", False, "parse")]


# Estados publicados no tel (src/strategy/RobotStateMachine.h)
ST_READY = 2
ST_RUNNING = 4


class LFRPhysics:
    """Mesma física do genFrame() do JavaScript, nas unidades do firmware 4.0.0."""

    def __init__(self, config: ConfigModel):
        self.cfg = config
        self.t = 0.0
        self.running = True
        self.battery = 72.0
        self._bat_tick = 0
        self._frames = 0
        self._ovr = 0
        self.lap_times: list[float] = []
        self.best_lap: float | None = None
        self._last_lap_t = time.monotonic()
        self._next_lap_in = 8.0 + random.uniform(0, 4)

    def step(self) -> list[dict]:
        """Avança um frame. Devolve [tel] ou, a cada STATUS_EVERY frames, [tel, sta]."""
        dt_s = 1.0 / RATE
        if self.running:
            self.t += dt_s

        tune = self.cfg.tune
        t = self.t
        track = math.sin(t * 0.8) * 2800 + math.sin(t * 2.1) * 600
        robot = track * 0.92 + math.sin(t * 5) * 80
        err = track - robot

        pos = max(-1.0, min(1.0, err / 3500.0))
        corr = max(-1.0, min(1.0, tune["kp"] * pos))
        base = tune["vmax"] if self.running else 0.0
        if self.running:
            v_l = max(-1023, min(1023, round((base + corr) * 1023)))
            v_r = max(-1023, min(1023, round((base - corr) * 1023)))
        else:
            v_l = v_r = 0
        dt_us = 1400 + random.randint(0, 400)
        self._ovr = max(self._ovr, dt_us)

        if tune["mode"] == 1 and self.running:
            sp_l = round((base + corr) * tune["rpmMax"])
            sp_r = round((base - corr) * tune["rpmMax"])
            rpm_l = round(sp_l * 0.97 + random.uniform(-5, 5))
            rpm_r = round(sp_r * 0.97 + random.uniform(-5, 5))
        else:
            sp_l = sp_r = rpm_l = rpm_r = 0

        sensors = []
        for i in range(8):
            d = (robot / 350.0 * 10.0) - (i - 3.5) * 10.0
            sensors.append(round(max(0.0, min(100.0, 100.0 * math.exp(-(d * d) / 80.0)))))

        # Drena bateria a cada 30 frames quando rodando
        if self.running:
            self._bat_tick += 1
            if self._bat_tick % 30 == 0:
                self.battery = max(0.0, min(100.0, self.battery - 0.09))

        # Lap timer simulado
        now = time.monotonic()
        if self.running and now - self._last_lap_t >= self._next_lap_in:
            lap_t = round(7.8 + random.uniform(0, 1.8), 2)
            self.lap_times.insert(0, lap_t)
            if len(self.lap_times) > 5:
                self.lap_times.pop()
            self.best_lap = min(self.lap_times)
            self._last_lap_t = now
            self._next_lap_in = 8.0 + random.uniform(0, 4)

        frames = [{
            "t": "tel",
            "pos": round(pos, 3),
            "corr": round(corr, 3),
            "vL": v_l,
            "vR": v_r,
            "dt": dt_us,
            "rpmL": rpm_l,
            "rpmR": rpm_r,
            "spL": sp_l,
            "spR": sp_r,
            "s": sensors,
            "st": ST_RUNNING if self.running else ST_READY,
            "lost": 0,
        }]

        self._frames += 1
        if self._frames % STATUS_EVERY == 0:
            frames.append({
                "t": "sta",
                "bat": round(self.battery),
                "vb": round(6.0 + self.battery / 100.0 * 2.4, 2),
                "lap": round(self.best_lap, 2) if self.best_lap is not None else 0.0,
                "laps": list(self.lap_times[:5]),
                "err": 0,
                "prof": self.cfg.profile,
                "dirty": int(self.cfg.dirty),
                "ovr": self._ovr,
            })
            self._ovr = 0
        return frames

    def command(self, raw: str):
        """Comandos de estado (start/stop/reset); a configuração é do ConfigModel."""
        data = json.loads(raw)
        kind = data.get("t")
        if kind == "start":
            self.running = True
            print("  [CMD] START")
        elif kind == "stop":
            self.running = False
            print("  [CMD] STOP")
        elif kind == "reset":
            self.running = False
            self.t = 0.0
            self.battery = 72.0
            self._bat_tick = 0
            self.lap_times.clear()
            self.best_lap = None
            print("  [CMD] RESET")


# Instâncias globais — todos os clientes compartilham o mesmo estado
config = ConfigModel()
physics = LFRPhysics(config)


def _first_ok(replies: list[str]) -> bool:
    try:
        return bool(replies) and json.loads(replies[0]).get("ok") == 1
    except ValueError:
        return False


async def handler(ws: websockets.ServerConnection):
    addr = ws.remote_address
    print(f"[LFR] Cliente conectado: {addr[0]}:{addr[1]}")

    # info + configuração inteira, como o firmware faz quando o cliente se inscreve
    await ws.send(config.info())
    for r in config.config_dump():
        await ws.send(r)

    async def delayed(msg: str, seconds: float):
        await asyncio.sleep(seconds)
        await ws.send(msg)

    async def recv_loop():
        async for msg in ws:
            try:
                replies = config.command(msg, physics.running)
                for reply in replies:
                    await ws.send(reply)
                data = json.loads(msg) if msg.startswith("{") else {}
                kind = data.get("t")
                if kind in ("start", "stop", "reset") and _first_ok(replies):
                    physics.command(msg)
                elif kind == "scal" and _first_ok(replies):
                    # o firmware responde calres ao fim da janela de 5 s
                    asyncio.ensure_future(delayed(_compact({"t": "calres", "ok": 1}), 5.0))
                elif kind == "cal" and data.get("op") == "finish" and _first_ok(replies):
                    side = "R" if data.get("side") == "R" else "L"
                    await ws.send(_compact({"t": "ecal", "side": side, "ok": 1, "ppr": 600.0}))
            except Exception as exc:  # noqa: BLE001 — o simulador não pode cair por um comando
                print(f"  [ERR] comando inválido: {exc}")

    recv_task = asyncio.ensure_future(recv_loop())
    interval = 1.0 / RATE

    try:
        while True:
            t0 = asyncio.get_event_loop().time()
            for frame in physics.step():
                await ws.send(_compact(frame))
            elapsed = asyncio.get_event_loop().time() - t0
            await asyncio.sleep(max(0.0, interval - elapsed))
    except websockets.exceptions.ConnectionClosed:
        print(f"[LFR] Cliente desconectado: {addr[0]}:{addr[1]}")
    finally:
        recv_task.cancel()


async def main():
    print("─" * 50)
    print(f"  LFR Simulator {FW} (protocolo BLE v4)")
    print(f"  WebSocket:  ws://{HOST}:{PORT}")
    print("  Dashboard:  http://localhost:8765/lfr-cockpit-mock.html")
    print("─" * 50, flush=True)
    async with websockets.serve(handler, HOST, PORT):
        await asyncio.Future()  # run forever


if __name__ == "__main__":
    if websockets is None:
        raise SystemExit("O servidor precisa do pacote websockets: pip install websockets")
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("\n[LFR] Simulador encerrado.")
