"""Testes do modelo de configuração do simulador (rodar: python3 -m unittest test_lfr_sim)."""
import json
import math
import unittest

from lfr_sim import ConfigModel


class ConfigModelTest(unittest.TestCase):
    def setUp(self):
        self.m = ConfigModel()

    def replies(self, cmd, running=False):
        return [json.loads(r) for r in self.m.command(json.dumps(cmd), running)]

    def test_set_valid_marks_dirty(self):
        r = self.replies({"t": "set", "k": "kp", "v": 1.5})
        self.assertEqual(r[0], {"t": "ack", "k": "kp", "ok": 1, "v": 1.5})
        self.assertTrue(self.m.dirty)

    def test_set_out_of_range(self):
        r = self.replies({"t": "set", "k": "kp", "v": 99})
        self.assertEqual(r[0]["err"], "range")

    def test_set_nan(self):
        r = [json.loads(x) for x in self.m.command('{"t":"set","k":"kp","v":NaN}', False)]
        self.assertEqual(r[0]["err"], "range")

    def test_hardware_locked_while_running(self):
        r = self.replies({"t": "set", "k": "invL", "v": 1}, running=True)
        self.assertEqual(r[0]["err"], "running")

    def test_get_sends_groups_and_meta(self):
        r = self.replies({"t": "get"})
        self.assertEqual([x["g"] for x in r], ["hw", "pid", "vel", "spd", "misc", "meta"])
        for x in r:
            self.assertLessEqual(len(json.dumps(x, separators=(",", ":"))), 180)

    def test_profile_refused_when_dirty(self):
        self.replies({"t": "set", "k": "kp", "v": 2})
        r = self.replies({"t": "prof", "n": 2})
        self.assertEqual(r[0]["err"], "dirty")
        r = self.replies({"t": "prof", "n": 2, "f": 1})
        self.assertEqual(r[0]["ok"], 1)
        self.assertTrue(math.isclose(self.m.tune["vmax"], 0.70))

    def test_save_then_load(self):
        self.replies({"t": "set", "k": "kp", "v": 2})
        self.replies({"t": "save"})
        self.replies({"t": "set", "k": "kp", "v": 3})
        self.replies({"t": "load"})
        self.assertEqual(self.m.tune["kp"], 2)
        self.assertFalse(self.m.dirty)

    # E-1 (F-33): modo competição — em ARMED/RUNNING nada do BLE tem efeito
    def test_lock_refuses_commands_while_running(self):
        r = self.replies({"t": "set", "k": "lock", "v": 1})
        self.assertEqual(r[0]["ok"], 1)
        r = self.replies({"t": "stop"}, running=True)
        self.assertEqual(r, [{"t": "ack", "k": "stop", "ok": 0, "err": "locked"}])
        r = self.replies({"t": "set", "k": "kp", "v": 2}, running=True)
        self.assertEqual(r, [{"t": "ack", "k": "kp", "ok": 0, "err": "locked"}])
        self.assertEqual(self.m.tune["kp"], 1.0)
        r = self.replies({"t": "start"})
        self.assertEqual(r[0]["ok"], 1)


if __name__ == "__main__":
    unittest.main()
