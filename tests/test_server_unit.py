#!/usr/bin/env python3
"""Модульные тесты сервера игры (web/play_server.py) и шпаргалки (tools/sheet.py).
Движок arena_play заменён заглушкой FakeEngine; игроки пишутся во временный файл, а не в state/players.json.
Запуск из корня проекта:  python3 -m unittest -v tests.test_server_unit"""
import json
import os
import sys
import tempfile
import time
import unittest

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "web"))
sys.path.insert(0, os.path.join(ROOT, "tools"))
import play_server as ps  # noqa: E402
import sheet  # noqa: E402


class FakeEngine:
    """Заглушка движка: отдаёт заранее заданные ответы и запоминает, что ей прислали."""

    def __init__(self, answers=()):
        self.answers = list(answers)
        self.sent = []
        self.killed = False

    def talk(self, msg):
        self.sent.append(msg)
        return self.answers.pop(0)

    def kill(self):
        self.killed = True


class ServerTestCase(unittest.TestCase):
    """Общая среда: чистые комнаты, бои и временный файл игроков."""

    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.saved = (ps.PLAYERS_FILE, ps.PLAYERS)
        ps.PLAYERS_FILE = os.path.join(self.tmp.name, "players.json")
        ps.PLAYERS = {}
        ps.ROOMS.clear()
        ps.SOLO.clear()

    def tearDown(self):
        ps.PLAYERS_FILE, ps.PLAYERS = self.saved
        ps.ROOMS.clear()
        ps.SOLO.clear()
        self.tmp.cleanup()


class TestPkey(unittest.TestCase):
    def test_lower_and_spaces(self):
        self.assertEqual(ps.pkey("  Вася   ПУПКИН "), "вася пупкин")

    def test_empty(self):
        self.assertEqual(ps.pkey(None), "")
        self.assertEqual(ps.pkey(""), "")

    def test_cut_to_40(self):
        self.assertEqual(len(ps.pkey("а" * 100)), 40)

    def test_not_string(self):
        self.assertEqual(ps.pkey(123), "123")


class TestPlayerSet(ServerTestCase):
    def test_saves_to_file(self):
        ps.player_set("Рыцарь", room="ABCDE")
        with open(ps.PLAYERS_FILE, encoding="utf-8") as f:
            self.assertEqual(json.load(f), {"рыцарь": {"room": "ABCDE"}})

    def test_update_keeps_other_fields(self):
        ps.player_set("Рыцарь", s={"budget": 800})
        ps.player_set("РЫЦАРЬ", room="XYZ")         # то же имя в другом регистре — тот же игрок
        self.assertEqual(ps.PLAYERS["рыцарь"], {"s": {"budget": 800}, "room": "XYZ"})

    def test_empty_name_ignored(self):
        ps.player_set("   ", room="A")
        self.assertEqual(ps.PLAYERS, {})
        self.assertFalse(os.path.exists(ps.PLAYERS_FILE))

    def test_too_big_character(self):
        with self.assertRaises(ps.Refuse):
            ps.player_set("Великан", s="x" * (ps.MAX_PLAYER_BYTES + 1))

    def test_players_limit(self):
        old = ps.MAX_PLAYERS
        ps.MAX_PLAYERS = 2
        try:
            ps.player_set("a", room="")
            ps.player_set("b", room="")
            ps.player_set("a", room="1")              # старый игрок — можно
            with self.assertRaises(ps.Refuse):
                ps.player_set("c", room="")
        finally:
            ps.MAX_PLAYERS = old


class TestNewCode(ServerTestCase):
    ALPHABET = set("ABCDEFGHJKLMNPQRSTUVWXYZ23456789")

    def test_format(self):
        for _ in range(200):
            c = ps.new_code()
            self.assertEqual(len(c), 5)
            self.assertTrue(set(c) <= self.ALPHABET)   # без похожих символов 0, O, 1, I

    def test_unique(self):
        ps.random.seed(1)
        first = ps.new_code()
        ps.ROOMS[first] = object()
        ps.random.seed(1)                              # тот же генератор дал бы тот же код — должен выбрать другой
        self.assertNotEqual(ps.new_code(), first)


class TestRoomFeed(unittest.TestCase):
    def setUp(self):
        self.r = ps.Room("ABCDE", "host")
        self.r.eng = FakeEngine()
        self.r.order = ["Игрок", "Бот 1"]

    def test_turn(self):
        self.r.feed({"type": "turn", "init": {"map": 1}, "frames": [1, 2], "seat": 0, "req": {"me": 0}})
        self.assertEqual(self.r.init, {"map": 1})
        self.assertEqual(self.r.frames, [1, 2])
        self.assertEqual(self.r.pending["seat"], 0)

    def test_frames_accumulate(self):
        self.r.feed({"type": "turn", "frames": [1], "seat": 0, "req": {}})
        self.r.feed({"type": "turn", "frames": [2, 3], "seat": 0, "req": {}})
        self.assertEqual(self.r.frames, [1, 2, 3])

    def test_end(self):
        self.r.status = "fight"
        self.r.feed({"type": "end", "result": "win", "reason": "все враги пали", "rounds": 7, "frames": []})
        self.assertEqual(self.r.status, "end")
        self.assertIsNone(self.r.pending)
        self.assertEqual(self.r.end["result"], "win")
        self.assertEqual(self.r.end["who"], ["Игрок", "Бот 1"])
        self.assertTrue(self.r.eng.killed)

    def test_error_returns_to_lobby(self):
        self.r.status = "fight"
        self.r.feed({"type": "error", "err": "плохой персонаж"})
        self.assertEqual((self.r.status, self.r.err), ("lobby", "плохой персонаж"))

    def test_bump(self):
        v = self.r.ver
        self.r.bump()
        self.assertEqual(self.r.ver, v + 1)


class TestRoomView(unittest.TestCase):
    def setUp(self):
        self.r = ps.Room("ABCDE", "s1")
        self.r.members = [{"sid": "s1", "name": "Аня", "team": 0, "char": {"x": 1}, "bot": None},
                          {"sid": "s2", "name": "Боря", "team": 1, "char": None, "bot": None},
                          {"sid": "b", "name": "Бот 1", "team": 1, "char": None, "bot": 500}]
        self.r.seats = {"s1": 0, "s2": 1}
        self.r.frames = ["f0", "f1", "f2"]
        self.r.pending = {"seat": 1, "req": {"me": 1}, "t": time.time()}

    def test_host_and_me(self):
        v = self.r.view("s1", 0)
        self.assertTrue(v["host"])
        self.assertEqual([m["me"] for m in v["members"]], [True, False, False])

    def test_ready(self):
        self.assertEqual([m["ready"] for m in self.r.view("s1", 0)["members"]], [True, False, True])

    def test_request_only_for_owner_of_turn(self):
        self.assertIsNone(self.r.view("s1", 0)["req"])
        self.assertEqual(self.r.view("s2", 0)["req"], {"me": 1})
        self.assertEqual(self.r.view("s1", 0)["waiting"], "Боря")

    def test_frames_since(self):
        v = self.r.view("s1", 2)
        self.assertEqual((v["frames"], v["nframes"]), (["f2"], 3))


class TestRoomTick(unittest.TestCase):
    def setUp(self):
        self.r = ps.Room("ABCDE", "s1")
        self.r.status = "fight"
        self.r.members = [{"sid": "s1", "name": "Аня", "team": 0, "char": {}, "bot": None}]
        self.r.seats = {"s1": 0}
        self.r.eng = FakeEngine([{"type": "end", "result": "lose", "frames": []}])

    def test_no_timeout_no_step(self):
        self.r.pending = {"seat": 0, "req": {}, "t": time.time()}
        self.r.tick()
        self.assertEqual(self.r.eng.sent, [])

    def test_timeout_ends_turn(self):
        self.r.pending = {"seat": 0, "req": {}, "t": time.time() - ps.TURN_TIMEOUT - 1}
        self.r.tick()
        self.assertEqual(self.r.eng.sent, [{"c": "end"}])
        self.assertEqual(self.r.status, "end")

    def test_left_player_turn_ends(self):
        self.r.members[0]["left"] = True
        self.r.pending = {"seat": 0, "req": {}, "t": time.time()}
        self.r.tick()
        self.assertEqual(self.r.eng.sent, [{"c": "end"}])


class TestRoomCommands(ServerTestCase):
    """Команды лобби (без движка): room_create, room_join, room_set, room_bot, room_kick, room_start, room_leave."""

    def create(self):
        v = ps.room_cmd1({"cmd": "room_create", "name": "Аня"}, "s1")
        return v["code"]

    def test_create(self):
        v = ps.room_cmd1({"cmd": "room_create", "name": "Аня"}, "s1")
        self.assertEqual(v["status"], "lobby")
        self.assertTrue(v["host"])
        self.assertIn(v["code"], ps.ROOMS)
        self.assertEqual(ps.PLAYERS["аня"]["room"], v["code"])

    def test_rooms_limit(self):
        for k in range(ps.MAX_ROOMS):
            ps.ROOMS["R%d" % k] = ps.Room("R%d" % k, "x")
        self.assertEqual(ps.room_cmd1({"cmd": "room_create"}, "s1")["type"], "error")

    def test_join_unknown(self):
        self.assertEqual(ps.room_cmd1({"cmd": "room_join", "code": "NOPE1"}, "s2")["err"], "нет такой комнаты")

    def test_join_code_case_and_spaces(self):
        code = self.create()
        v = ps.room_cmd1({"cmd": "room_join", "code": " " + code.lower() + " ", "name": "Боря"}, "s2")
        self.assertEqual(len(v["members"]), 2)
        self.assertEqual(v["members"][1]["team"], 1)          # второй игрок — в другую команду

    def test_rejoin_same_name_keeps_place(self):
        code = self.create()
        v = ps.room_cmd1({"cmd": "room_join", "code": code, "name": "аня"}, "s1-new")
        self.assertEqual(len(v["members"]), 1)
        self.assertTrue(v["host"])                              # хозяин вернулся с новым sid

    def test_set_team_is_clamped(self):
        code = self.create()
        ps.room_cmd1({"cmd": "room_set", "code": code, "team": 999}, "s1")
        self.assertEqual(ps.ROOMS[code].members[0]["team"], 63)

    def test_set_not_member(self):
        code = self.create()
        self.assertEqual(ps.room_cmd1({"cmd": "room_set", "code": code, "team": 1}, "zz")["type"], "error")

    def test_bot_only_host(self):
        code = self.create()
        ps.room_cmd1({"cmd": "room_join", "code": code, "name": "Боря"}, "s2")
        self.assertEqual(ps.room_cmd1({"cmd": "room_bot", "code": code}, "s2")["type"], "error")
        v = ps.room_cmd1({"cmd": "room_bot", "code": code, "budget": 5}, "s1")
        self.assertEqual(v["members"][-1]["bot"], 20)           # бюджет бота не меньше 20

    def test_kick(self):
        code = self.create()
        ps.room_cmd1({"cmd": "room_bot", "code": code}, "s1")
        ps.room_cmd1({"cmd": "room_kick", "code": code, "i": 0}, "s1")   # себя убрать нельзя
        self.assertEqual(len(ps.ROOMS[code].members), 2)
        ps.room_cmd1({"cmd": "room_kick", "code": code, "i": 1}, "s1")
        self.assertEqual(len(ps.ROOMS[code].members), 1)

    def test_start_without_character(self):
        code = self.create()
        v = ps.room_cmd1({"cmd": "room_start", "code": code}, "s1")
        self.assertIn("не все игроки выбрали персонажа", v["err"])

    def test_start_not_host(self):
        code = self.create()
        ps.room_cmd1({"cmd": "room_join", "code": code, "name": "Боря"}, "s2")
        self.assertEqual(ps.room_cmd1({"cmd": "room_start", "code": code}, "s2")["err"], "начинает хозяин комнаты")

    def test_step_not_your_turn(self):
        code = self.create()
        self.assertEqual(ps.room_cmd1({"cmd": "room_step", "code": code, "c": "end"}, "s1")["err"], "сейчас не ваш ход")

    def test_leave_last_human_deletes_room(self):
        code = self.create()
        ps.room_cmd1({"cmd": "room_bot", "code": code}, "s1")
        self.assertEqual(ps.room_cmd1({"cmd": "room_leave", "code": code}, "s1"), {"type": "left"})
        self.assertNotIn(code, ps.ROOMS)
        self.assertEqual(ps.PLAYERS["аня"]["room"], "")

    def test_leave_passes_host(self):
        code = self.create()
        ps.room_cmd1({"cmd": "room_join", "code": code, "name": "Боря"}, "s2")
        ps.room_cmd1({"cmd": "room_leave", "code": code}, "s1")
        self.assertEqual(ps.ROOMS[code].host, "s2")

    def test_idle_room_removed(self):
        code = self.create()
        ps.ROOMS[code].t -= ps.ROOM_IDLE + 1
        self.assertEqual(ps.room_cmd1({"cmd": "room_poll", "code": code}, "s1")["err"], "нет такой комнаты")


class TestSoloLimit(ServerTestCase):
    def test_same_sid_same_fight(self):
        self.assertIs(ps.solo("a"), ps.solo("a"))

    def test_busy_server(self):
        old = ps.MAX_SOLO
        ps.MAX_SOLO = 2
        try:
            ps.solo("a")
            ps.solo("b")
            with self.assertRaises(ps.Refuse):
                ps.solo("c")
            ps.SOLO["a"]["t"] -= 601                            # давний простаивающий бой освобождает место
            ps.SOLO["a"]["eng"] = FakeEngine()
            ps.solo("c")
            self.assertNotIn("a", ps.SOLO)
        finally:
            ps.MAX_SOLO = old


class TestSheetHelpers(unittest.TestCase):
    def test_pct(self):
        self.assertEqual(sheet.pct(0.25), "+25%")
        self.assertEqual(sheet.pct(-0.1), "-10%")
        self.assertEqual(sheet.pct(0), "+0%")
        self.assertEqual(sheet.pct(0.126), "+13%")

    def test_step_price(self):
        lad = [0, 10, 25, 45]
        self.assertEqual([sheet.step_price(lad, n) for n in (1, 2, 3)], [10, 15, 20])


if __name__ == "__main__":
    unittest.main()
