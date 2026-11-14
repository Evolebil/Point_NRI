#!/usr/bin/env python3
"""Интеграционные тесты: сервер игры (web/play_server.py) + настоящий движок build/arena_play + HTTP API.
Нужна собранная программа (make). Игроки пишутся во временный файл, а не в state/players.json.
Запуск из корня проекта:  python3 -m unittest -v tests.test_server_integration"""
import json
import os
import sys
import tempfile
import threading
import unittest
import urllib.error
import urllib.request
import warnings

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "web"))
sys.path.insert(0, os.path.join(ROOT, "tools"))
import play_server as ps  # noqa: E402
import sheet  # noqa: E402

KNIGHT = {"name": "Рыцарь", "budget": 800, "stats": [16, 10, 14, 8, 10, 8], "acV": 8,
          "weapon": {"n": 1, "s": 8, "range": 5, "att": 2, "reach": 5, "type": 11},
          "abilities": [{"delivery": 1, "saveStat": 1, "range": 5, "radius": 10, "dmgN": 2, "dmgS": 6, "dtype": 1, "cond": 1, "recharge": 5}],
          "passives": {"saveProf": [2]}}
MAX_TURNS = 300



def setUpModule():
    # Engine.kill() не ждёт завершения процесса — unittest сообщал бы об этом ResourceWarning после каждого боя
    warnings.simplefilter("ignore", ResourceWarning)


need_engine = unittest.skipUnless(os.path.exists(ps.ENGINE), "нет build/arena_play — соберите командой make")


def stat_rule(v):
    """Цена характеристики по правилам (docs, tools/sheet.py): V·(V+1)/2 − 55."""
    return v * (v + 1) // 2 - 55


@need_engine
class TestEngineProtocol(unittest.TestCase):
    """Связь сервер → процесс движка: класс Engine и команды price / catalog."""

    @classmethod
    def setUpClass(cls):
        cls.eng = ps.Engine()

    @classmethod
    def tearDownClass(cls):
        cls.eng.kill()

    def test_price_knight(self):
        d = self.eng.talk({"cmd": "price", "char": KNIGHT})["data"]
        self.assertTrue(d["ok"], d.get("err"))
        self.assertEqual(d["spent"] + d["left"], KNIGHT["budget"])
        self.assertGreater(d["hp"], 0)

    def test_price_items_follow_rules(self):
        items = self.eng.talk({"cmd": "price", "char": KNIGHT})["data"]["boss"]["items"]
        stats = [it["cost"] for it in items if it["cat"] == "stat"]
        self.assertEqual(stats, [stat_rule(v) for v in KNIGHT["stats"]])   # 81, 0, 50, −19, 0, −19

    def test_price_bad_budget(self):
        d = self.eng.talk({"cmd": "price", "char": dict(KNIGHT, budget=10)})["data"]
        self.assertFalse(d["ok"])
        self.assertIn("бюджет", d["err"])

    def test_more_budget_more_hp(self):
        hp = [self.eng.talk({"cmd": "price", "char": dict(KNIGHT, budget=b)})["data"]["hp"] for b in (800, 1600)]
        self.assertLess(hp[0], hp[1])

    def test_engine_restarts_after_kill(self):
        e = ps.Engine()
        e.talk({"cmd": "price", "char": KNIGHT})
        e.kill()
        self.assertTrue(e.talk({"cmd": "price", "char": KNIGHT})["data"]["ok"])   # новый процесс поднимается сам
        e.kill()

    def test_selected_world_scale_is_preserved(self):
        e = ps.Engine()
        try:
            out = e.talk({"cmd": "start", "char": KNIGHT, "seed": 11, "side": 0,
                          "opp": {"mode": "generator"},
                          "world": {"seed": "47361", "size": 1000, "x": 340, "y": 400,
                                    "w": 32, "h": 20, "step": 10}})
            world = out.get("init", {}).get("map", {}).get("world", {})
            self.assertIn(out["type"], ("turn", "end"), out)
            self.assertEqual((world.get("x"), world.get("y"), world.get("step")), (340, 400, 10))
        finally:
            e.kill()

    def test_closed_engine_is_error(self):
        # Найденный дефект: если процесс успел закрыться до записи, talk() выбрасывает BrokenPipeError,
        # а не своё RuntimeError("движок закрылся"). HTTP-обработчик ловит оба (см. test_dead_engine_over_http).
        old = ps.ENGINE
        ps.ENGINE = "/bin/true"                                  # «движок», который сразу закрывается
        try:
            with self.assertRaises((RuntimeError, BrokenPipeError)):
                ps.Engine().talk({"cmd": "price"})
        finally:
            ps.ENGINE = old


@need_engine
class TestSheetWithEngine(unittest.TestCase):
    """tools/sheet.py берёт каталог цен у движка и строит шпаргалку."""

    def test_catalog(self):
        c = sheet.catalog(800)
        for key in ("ladders", "coef", "mods", "unit", "dtypes", "passives"):
            self.assertIn(key, c)

    def test_sheet_text(self):
        text = sheet.sheet(800)
        self.assertTrue(text.startswith("# Шпаргалка цен (бюджет 800)"))
        for part in ("## 1. Тело", "## 2. Оружие", "## 3. Способность", "## 4. Пассивы"):
            self.assertIn(part, text)


@need_engine
class TestHttpApi(unittest.TestCase):
    """Сервер целиком: HTTP-запросы к ThreadingHTTPServer, за ним — движок."""

    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        cls.saved = (ps.PLAYERS_FILE, ps.PLAYERS)
        ps.PLAYERS_FILE, ps.PLAYERS = os.path.join(cls.tmp.name, "players.json"), {}
        cls.srv = ps.ThreadingHTTPServer(("127.0.0.1", 0), ps.Handler)
        cls.url = "http://127.0.0.1:%d" % cls.srv.server_address[1]
        threading.Thread(target=cls.srv.serve_forever, daemon=True).start()

    @classmethod
    def tearDownClass(cls):
        cls.srv.shutdown()
        for s in ps.SOLO.values():
            s["eng"].kill()
        ps.SOLO.clear()
        ps.QUERY.kill()
        ps.PLAYERS_FILE, ps.PLAYERS = cls.saved
        cls.tmp.cleanup()

    def api(self, msg, raw=None):
        data = raw if raw is not None else json.dumps(msg, ensure_ascii=False).encode("utf-8")
        req = urllib.request.Request(self.url + "/api", data=data, headers={"Content-Type": "application/json"})
        try:
            with urllib.request.urlopen(req, timeout=60) as r:
                return r.status, json.loads(r.read())
        except urllib.error.HTTPError as e:
            return e.code, json.loads(e.read())

    def get(self, path):
        try:
            with urllib.request.urlopen(self.url + path, timeout=60) as r:
                return r.status, r.read().decode("utf-8")
        except urllib.error.HTTPError as e:
            return e.code, e.read().decode("utf-8")

    def test_price(self):
        code, out = self.api({"cmd": "price", "char": KNIGHT})
        self.assertEqual(code, 200)
        self.assertTrue(out["data"]["ok"])

    def test_not_json(self):
        code, out = self.api(None, raw=b"{oops")
        self.assertEqual((code, out["err"]), (400, "не JSON"))

    def test_no_cmd_object(self):
        self.assertEqual(self.api([1, 2, 3])[0], 400)

    def test_dead_engine_over_http(self):
        old_engine, old_query = ps.ENGINE, ps.QUERY
        ps.ENGINE, ps.QUERY = "/bin/true", ps.Engine()
        try:
            code, out = self.api({"cmd": "price", "char": KNIGHT})
        finally:
            ps.QUERY.kill()
            ps.ENGINE, ps.QUERY = old_engine, old_query
        self.assertEqual((code, out["type"]), (200, "error"))
        self.assertIn("ошибка движка", out["err"])            # игрок видит понятный текст, сервер не падает

    def test_unknown_command(self):
        self.assertEqual(self.api({"cmd": "fly_to_moon"})[1]["err"], "неизвестная команда")

    def test_quick_match_commands(self):
        sid = "it-quick-match"
        code, waiting = self.api({"cmd": "match_find", "sid": sid, "name": "Тест", "budget": 800, "char": KNIGHT})
        self.assertEqual((code, waiting["type"]), (200, "match_wait"))
        code, cancelled = self.api({"cmd": "match_cancel", "sid": sid})
        self.assertEqual((code, cancelled["type"]), (200, "match_cancelled"))

        code, room = self.api({"cmd": "quick_match_create", "sid": "it-quick-room", "players": [
            {"sid": "it-player-a", "name": "Аня", "char": KNIGHT, "budget": 800},
            {"sid": "it-player-b", "name": "Боря", "char": KNIGHT, "budget": 800},
        ]})
        self.assertEqual((code, room["type"]), (200, "quick_match_room"))
        self.assertIn(room["code"], ps.ROOMS)
        self.assertEqual([m["team"] for m in ps.ROOMS[room["code"]].members], [0, 1])
        ps.ROOMS.pop(room["code"], None)

    def test_step_without_game(self):
        self.assertEqual(self.api({"cmd": "step", "sid": "nobody", "c": "end"})[1]["err"], "игра не идёт")

    def test_player_save_and_load(self):
        self.api({"cmd": "player_save", "name": "Тестовый Игрок", "s": {"budget": 900}})
        out = self.api({"cmd": "player_load", "name": "тестовый   игрок"})[1]
        self.assertEqual(out, {"type": "player", "s": {"budget": 900}, "room": ""})

    def test_pages(self):
        code, html = self.get("/")
        self.assertEqual(code, 200)
        self.assertNotIn("/*PLAY_JS*/", html)                 # скрипты подставлены в страницу
        code, md = self.get("/sheet?budget=1000")
        self.assertEqual(code, 200)
        self.assertIn("бюджет 1000", md)
        self.assertEqual(self.get("/nothing")[0], 404)

    def test_solo_fight_to_the_end(self):
        """Одиночный бой: старт против генератора, игрок каждый раз завершает ход — бой должен закончиться."""
        sid = "it-solo"
        out = self.api({"cmd": "start", "sid": sid, "char": KNIGHT, "seed": 7, "opp": {"mode": "generator"}})[1]
        self.assertIn(out["type"], ("turn", "end"), out)
        self.assertIsNotNone(out.get("init"))
        turns = 0
        while out["type"] == "turn" and turns < MAX_TURNS:
            out = self.api({"cmd": "step", "sid": sid, "c": "end"})[1]
            turns += 1
        self.assertEqual(out["type"], "end", "бой не закончился за %d ходов" % MAX_TURNS)
        self.assertIn("result", out)
        self.assertEqual(self.api({"cmd": "step", "sid": sid, "c": "end"})[1]["err"], "игра не идёт")


@need_engine
class TestRoomWithEngine(unittest.TestCase):
    """Комната: игрок против бота, бой идёт через настоящий движок до итога."""

    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.saved = (ps.PLAYERS_FILE, ps.PLAYERS)
        ps.PLAYERS_FILE, ps.PLAYERS = os.path.join(self.tmp.name, "players.json"), {}
        ps.ROOMS.clear()

    def tearDown(self):
        for r in ps.ROOMS.values():
            if r.eng:
                r.eng.kill()
        ps.ROOMS.clear()
        ps.PLAYERS_FILE, ps.PLAYERS = self.saved
        self.tmp.cleanup()

    def test_player_vs_bot(self):
        code = ps.room_cmd1({"cmd": "room_create", "name": "Аня"}, "s1")["code"]
        ps.room_cmd1({"cmd": "room_set", "code": code, "char": KNIGHT}, "s1")
        ps.room_cmd1({"cmd": "room_bot", "code": code, "team": 1, "budget": 800}, "s1")
        v = ps.room_cmd1({"cmd": "room_start", "code": code, "seed": 5}, "s1")
        self.assertEqual(v["err"], "")
        self.assertIn(v["status"], ("fight", "end"))
        self.assertGreater(v["nframes"], 0)
        turns = 0
        while v["status"] == "fight" and turns < MAX_TURNS:
            self.assertIsNotNone(v["req"])                   # в бою с ботом ход всегда за единственным игроком
            v = ps.room_cmd1({"cmd": "room_step", "code": code, "c": "end"}, "s1")
            turns += 1
        self.assertEqual(v["status"], "end")
        self.assertEqual(v["end"]["who"], ["Аня", "Бот 1"])
        self.assertTrue(all(m["sid"] != "s1" or m["name"] == "Аня" for m in ps.ROOMS[code].members))


if __name__ == "__main__":
    unittest.main()
