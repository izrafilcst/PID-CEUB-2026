#!/usr/bin/env python3
"""
LFR Simulator — WebSocket bridge para o dashboard.

Replica o protocolo exato que o ESP32 usará via BLE:
  • BLE Notify  (ESP32 → App)  →  WS Server → Client
  • BLE Write   (App → ESP32)  →  WS Client → Server

Protocolo (JSON, mirrors BLE characteristics):
──────────────────────────────────────────────
  Servidor → Cliente  (BLE UUID: 0xABCD — Telemetry Char, Notify)
    {"t":"info", "name":"LFR-RACER-01", "fw":"v3.1.0", "mode":"SIM"}
    {"t":"tel",
      "pos":  float,       # posição normalizada do erro [-3500, +3500]
      "corr": float,       # correção PID
      "vL":   int,         # velocidade motor esquerdo [-255, 255]
      "vR":   int,         # velocidade motor direito  [-255, 255]
      "dt":   int,         # tempo de loop em µs
      "s":    [8× float],  # sensores TCRT5000 [0.0–100.0]
      "bat":  float,       # bateria percentual [0–100]
      "lap":  float|null,  # melhor volta em segundos
      "laps": [float]      # histórico últimas 5 voltas
    }

  Cliente → Servidor  (BLE UUID: 0xABCE — Command Char, Write)
    {"t":"pid",   "kp":float, "ki":float, "kd":float}
    {"t":"spd",   "base":int, "min":int,  "max":int, "thrs":float}
    {"t":"start"}
    {"t":"stop"}
    {"t":"reset"}
──────────────────────────────────────────────
Uso:
    python3 lfr_sim.py
    # Em outra aba: python3 -m http.server 8765
    # Abrir: http://localhost:8765/lfr-cockpit-mock.html
"""

import asyncio
import json
import math
import random
import time

import websockets

HOST = "localhost"
PORT = 8766
RATE = 30  # Hz — mesma taxa do loop PID do ESP32


class LFRPhysics:
    """Mesma física do genFrame() do JavaScript — resultados idênticos."""

    def __init__(self):
        self.t = 0.0
        self.running = True
        self.pid = {"kp": 3.0, "ki": 0.0, "kd": 12.0}
        self.spd = {"base": 160, "min": 60, "max": 230, "thrs": 0.6}
        self.battery = 72.0
        self._bat_tick = 0
        self.lap_times: list[float] = []
        self.best_lap: float | None = None
        self._last_lap_t = time.monotonic()
        self._next_lap_in = 8.0 + random.uniform(0, 4)

    def step(self) -> dict:
        """Avança um frame e retorna o frame de telemetria."""
        dt_s = 1.0 / RATE
        if self.running:
            self.t += dt_s

        kp = self.pid["kp"]
        kd = self.pid["kd"]
        t = self.t

        track = math.sin(t * 0.8) * 2800 + math.sin(t * 2.1) * 600
        robot = track * 0.92 + math.sin(t * 5) * 80
        pos = robot
        err = track - robot
        corr = err * kp - math.cos(t * 0.8) * kd * 200
        base = self.spd["base"]
        vL = min(255, max(-255, base + corr * 0.08))
        vR = min(255, max(-255, base - corr * 0.08))
        dt_us = 1400 + random.randint(0, 400)

        sensors = []
        for i in range(8):
            d = (pos / 350.0 * 10.0) - (i - 3.5) * 10.0
            v = max(0.0, min(100.0, 100.0 * math.exp(-(d * d) / 80.0)))
            sensors.append(round(v, 1))

        # Drena bateria a cada 30 frames quando rodando
        if self.running:
            self._bat_tick += 1
            if self._bat_tick % 30 == 0:
                self.battery = max(0.0, min(100.0, self.battery - 0.09))

        # Lap timer simulado
        now = time.monotonic()
        if now - self._last_lap_t >= self._next_lap_in:
            lap_t = round(7.8 + random.uniform(0, 1.8), 2)
            self.lap_times.insert(0, lap_t)
            if len(self.lap_times) > 5:
                self.lap_times.pop()
            self.best_lap = min(self.lap_times)
            self._last_lap_t = now
            self._next_lap_in = 8.0 + random.uniform(0, 4)

        return {
            "t":   "tel",
            "pos":  round(pos, 1),
            "corr": round(corr, 1),
            "vL":   round(vL),
            "vR":   round(vR),
            "dt":   dt_us,
            "s":    sensors,
            "bat":  round(self.battery, 1),
            "lap":  round(self.best_lap, 2) if self.best_lap is not None else None,
            "laps": list(self.lap_times[:5]),
        }

    def command(self, raw: str):
        """Processa um comando recebido do dashboard (mirrors BLE Write handler)."""
        data = json.loads(raw)
        kind = data.get("t")
        if kind == "pid":
            for k in ("kp", "ki", "kd"):
                if k in data:
                    self.pid[k] = float(data[k])
            print(f"  [CMD] PID → kp={self.pid['kp']} ki={self.pid['ki']} kd={self.pid['kd']}")
        elif kind == "spd":
            for k in ("base", "min", "max", "thrs"):
                if k in data:
                    self.spd[k] = float(data[k])
            print(f"  [CMD] SPD → {self.spd}")
        elif kind == "start":
            self.running = True
            print("  [CMD] START")
        elif kind == "stop":
            self.running = False
            print("  [CMD] STOP")
        elif kind == "reset":
            self.t = 0.0
            self.battery = 72.0
            self._bat_tick = 0
            self.lap_times.clear()
            self.best_lap = None
            print("  [CMD] RESET")
        else:
            print(f"  [CMD] desconhecido: {kind}")


# Instância global — todos os clientes compartilham o mesmo estado
physics = LFRPhysics()


async def handler(ws: websockets.ServerConnection):
    addr = ws.remote_address
    print(f"[LFR] ✓ Cliente conectado: {addr[0]}:{addr[1]}")

    # Frame de identificação (mirrors BLE Device Information Service)
    await ws.send(json.dumps({
        "t":    "info",
        "name": "LFR-RACER-01",
        "fw":   "v3.1.0-sim",
        "mode": "SIM",
    }))

    # Task de recepção de comandos (não-bloqueante)
    async def recv_loop():
        async for msg in ws:
            try:
                physics.command(msg)
            except Exception as exc:
                print(f"  [ERR] comando inválido: {exc}")

    recv_task = asyncio.ensure_future(recv_loop())
    interval = 1.0 / RATE

    try:
        while True:
            t0 = asyncio.get_event_loop().time()
            frame = physics.step()
            await ws.send(json.dumps(frame))
            elapsed = asyncio.get_event_loop().time() - t0
            await asyncio.sleep(max(0.0, interval - elapsed))
    except websockets.exceptions.ConnectionClosed:
        print(f"[LFR] ✗ Cliente desconectado: {addr[0]}:{addr[1]}")
    finally:
        recv_task.cancel()


async def main():
    print("─" * 50)
    print("  LFR Simulator v1.0")
    print(f"  WebSocket:  ws://{HOST}:{PORT}")
    print(f"  Dashboard:  http://localhost:8765/lfr-cockpit-mock.html")
    print("─" * 50)
    async with websockets.serve(handler, HOST, PORT):
        await asyncio.Future()  # run forever


if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("\n[LFR] Simulador encerrado.")
