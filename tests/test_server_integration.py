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

    def test_room_vox(self):
        # хозяин с видом 3D: бой комнаты в режиме кубов (у бойцов в кадре z, карта с vox) и доходит до конца; без vox — по слоям
        code = ps.room_cmd1({"cmd": "room_create", "name": "Аня"}, "s1")["code"]
        ps.room_cmd1({"cmd": "room_set", "code": code, "char": KNIGHT}, "s1")
        ps.room_cmd1({"cmd": "room_bot", "code": code, "team": 1, "budget": 800}, "s1")
        v = ps.room_cmd1({"cmd": "room_start", "code": code, "seed": 5, "vox": 1}, "s1")
        self.assertEqual(v["err"], "")
        self.assertEqual(v["init"]["map"].get("vox"), 1)
        self.assertTrue(all("z" in f for f in ps.ROOMS[code].frames[0]["f"]))
        turns = 0
        while v["status"] == "fight" and turns < MAX_TURNS:
            v = ps.room_cmd1({"cmd": "room_step", "code": code, "c": "end"}, "s1")
            turns += 1
        self.assertEqual(v["status"], "end")
        code2 = ps.room_cmd1({"cmd": "room_create", "name": "Боря"}, "s2")["code"]
        ps.room_cmd1({"cmd": "room_set", "code": code2, "char": KNIGHT}, "s2")
        ps.room_cmd1({"cmd": "room_bot", "code": code2, "team": 1, "budget": 800}, "s2")
        v = ps.room_cmd1({"cmd": "room_start", "code": code2, "seed": 5}, "s2")
        self.assertNotIn("vox", v["init"]["map"])
        self.assertTrue(all("z" not in f for f in ps.ROOMS[code2].frames[0]["f"]))

    def test_pointer(self):
        """Указка (Q): точку игрока видят другие (не он сам); отпустил — гаснет; без обновления — гаснет сама."""
        code = ps.room_cmd1({"cmd": "room_create", "name": "Аня"}, "s1")["code"]
        ps.room_cmd1({"cmd": "room_join", "code": code, "name": "Боря"}, "s2")
        ps.room_cmd1({"cmd": "room_ptr", "code": code, "on": True, "x": 3, "y": 4}, "s1")
        self.assertEqual(ps.room_cmd1({"cmd": "room_poll", "code": code}, "s2")["ptrs"], [{"name": "Аня", "team": 0, "x": 3.0, "y": 4.0}])
        self.assertEqual(ps.room_cmd1({"cmd": "room_poll", "code": code}, "s1")["ptrs"], [])
        ps.ROOMS[code].members[0]["ptr"][2] -= ps.PTR_TTL + 1
        self.assertEqual(ps.room_cmd1({"cmd": "room_poll", "code": code}, "s2")["ptrs"], [])
        ps.room_cmd1({"cmd": "room_ptr", "code": code, "on": True, "x": "мусор"}, "s1")
        ps.room_cmd1({"cmd": "room_ptr", "code": code, "on": False}, "s1")
        self.assertEqual(ps.room_cmd1({"cmd": "room_poll", "code": code}, "s2")["ptrs"], [])

    def test_rematch_and_leave(self):
        """QA 2026-11-16: после боя комната — снова лобби (бот, реванш, новый номер боя); выход будит опрос остальных."""
        code = ps.room_cmd1({"cmd": "room_create", "name": "Хозяин"}, "h1")["code"]
        ps.room_cmd1({"cmd": "room_join", "code": code, "name": "Гость"}, "g1")
        for sid in ("h1", "g1"):
            ps.room_cmd1({"cmd": "room_set", "code": code, "char": KNIGHT}, sid)
        ps.room_cmd1({"cmd": "room_set", "code": code, "team": 1}, "g1")
        r = ps.ROOMS[code]
        for n in (1, 2):
            v = ps.room_cmd1({"cmd": "room_start", "code": code, "seed": 5, "rounds": 1, "map": {"aw": 12, "ah": 10}}, "h1")
            self.assertEqual((v["err"], v["fid"]), ("", n))
            for _ in range(20):
                if r.status != "fight" or not r.pending:
                    break
                sid = next(s for s, k in r.seats.items() if k == r.pending["seat"])
                ps.room_cmd1({"cmd": "room_step", "code": code, "c": "end"}, sid)
            self.assertEqual(r.status, "end")
            late = ps.room_cmd1({"cmd": "room_poll", "code": code, "since": 9999}, "g1")
            self.assertEqual(late["fid"], n)   # по номеру боя гость понимает, что бой новый, и опрашивает с нуля
            self.assertIsNotNone(ps.room_cmd1({"cmd": "room_poll", "code": code, "since": 0}, "g1")["init"])
        v = ps.room_cmd1({"cmd": "room_bot", "code": code, "budget": 300}, "h1")   # после боя хозяин меняет состав
        self.assertEqual(v["type"], "room", v)
        self.assertEqual(len(r.members), 3)
        ver = r.ver
        ps.room_cmd({"cmd": "room_leave", "code": code}, "h1")
        self.assertGreater(r.ver, ver)   # опрос гостя проснётся сразу
        self.assertEqual(r.host, "g1")   # хозяином стал оставшийся игрок
        r.eng and r.eng.kill()

    def test_fog_two_players(self):
        """Туман войны в бою двух людей: кадр общий, но каждому — своя копия; скрытый от его команды боец помечен hid."""
        frames = [{"f": [{"x": 1, "team": 0}, {"x": 9, "team": 1, "hT": [0]}]}, {"f": [{"x": 2, "team": 0}]}]
        a, b, spect = ps.fog_frames(frames, 0), ps.fog_frames(frames, 1), ps.fog_frames(frames, None)
        self.assertEqual(a[0]["f"][1], {"x": 9, "team": 1, "hid": 1})
        self.assertEqual(b[0]["f"][1], {"x": 9, "team": 1})
        self.assertEqual(spect[0]["f"][1], {"x": 9, "team": 1})
        self.assertIn("hT", frames[0]["f"][1])                 # общий кадр комнаты не испорчен
        self.assertIs(a[1], frames[1])                         # кадр без скрытых — без копии
        code = ps.room_cmd1({"cmd": "room_create", "name": "Аня"}, "s1")["code"]
        ps.room_cmd1({"cmd": "room_join", "code": code, "name": "Боря"}, "s2")
        ps.room_cmd1({"cmd": "room_set", "code": code, "char": KNIGHT}, "s1")
        ps.room_cmd1({"cmd": "room_set", "code": code, "char": KNIGHT}, "s2")
        mp = {"aw": 20, "ah": 14, "dark": ["1" * 20] * 14}   # вся арена во тьме: без тёмного зрения врага не видно
        v = ps.room_cmd1({"cmd": "room_start", "code": code, "seed": 3, "map": mp}, "s1")
        self.assertEqual(v["err"], "")
        r = ps.ROOMS[code]
        for _ in range(8):   # стоят на месте: где враг стоял в начале боя, через раунд уже не известно
            if r.status != "fight" or not r.pending:
                break
            sid = next(s for s, k in r.seats.items() if k == r.pending["seat"])
            ps.room_cmd1({"cmd": "room_step", "code": code, "c": "end"}, sid)
        team = {m["sid"]: m["team"] for m in r.members}
        for sid in ("s1", "s2"):
            last = ps.room_cmd1({"cmd": "room_poll", "code": code}, sid)["frames"][-1]["f"]
            for f in last:
                self.assertNotIn("hT", f)
                self.assertEqual(bool(f.get("hid")), f["team"] != team[sid], (sid, f))   # свой виден, чужой во тьме — нет


class TestFogInData(unittest.TestCase):
    """Туман войны в данных (2026-11-16): чего игрок не видит, того нет и в ответе сервера."""

    def test_hidden_fighter_is_blank(self):
        frames = [{"actor": 1, "act": "attack-melee", "target": 0, "abil": -1, "cell": [3, 4], "ev": [],
                   "f": [{"x": 1, "y": 1, "hp": 50, "team": 0, "kind": 0, "cond": []},
                         {"x": 9, "y": 7, "hp": 133, "maxHp": 133, "ac": 23, "team": 1, "kind": 1, "cond": ["невидим"], "ct": 4, "lt": 30, "z": 5, "hid": 1}]}]
        out = ps.mask_hidden(frames, {})[0]
        h = out["f"][1]
        self.assertEqual((h["hid"], h["team"], h["kind"], h["x"], h["y"], h["hp"], h["cond"]), (1, 1, 1, -1, -1, 1, []))
        for k in ("ct", "lt", "z", "ac"):
            self.assertTrue(k not in h or h[k] == 0, k)
        self.assertEqual((out["act"], out["target"], out["abil"]), ("none", -1, -1))   # ход скрытого — без действия и цели
        self.assertNotIn("cell", out)
        self.assertEqual(out["f"][0], frames[0]["f"][0])                              # видимый — как есть
        self.assertEqual(frames[0]["f"][1]["hp"], 133)                                # исходный кадр не испорчен

    def test_events_about_hidden_dropped(self):
        ev = [{"k": "save", "who": 1}, {"k": "fall", "who": 1, "dmg": 3}, {"k": "roll", "who": 1, "tgt": 0, "d20": 12},
              {"k": "roll", "who": 0, "tgt": 1, "d20": 5}, {"k": "order", "who": 2, "tgt": 1}, {"k": "door", "x": 1, "y": 2}]
        out = ps.mask_hidden([{"actor": 0, "ev": ev, "f": [{"team": 0}, {"team": 1, "hid": 1}, {"team": 1}]}], {})[0]
        kinds = [(e["k"], e.get("who")) for e in out["ev"]]
        self.assertEqual(kinds, [("roll", 1), ("door", None)])   # остался бросок по видимой цели и дверь

    def test_digging_held_until_seen(self):
        dig = {"k": "vox", "x": 4, "y": 4, "z": 2, "m": 0}
        pend = {}
        f_hid = [{"team": 0}, {"team": 1, "hid": 1}]
        a = ps.mask_hidden([{"actor": 1, "ev": [dig], "f": f_hid}], pend)[0]
        b = ps.mask_hidden([{"actor": 0, "ev": [], "f": f_hid}], pend)[0]
        c = ps.mask_hidden([{"actor": 0, "ev": [], "f": [{"team": 0}, {"team": 1}]}], pend)[0]
        self.assertEqual((a["ev"], b["ev"], c["ev"], pend), ([], [], [dig], {}))   # ход копателя — когда он показался

    def test_enemy_build_without_numbers(self):
        boss = {"title": "Враг", "hp": 133, "ac": 23, "stats": [10] * 6, "items": [{"cost": 5}], "text": "…", "abil": [{"title": "Удар"}], "auraRad": 0,
                "minion": {"title": "Приспешник", "hp": 20, "abil": []}}
        init = ps.mask_init({"me": 0, "bosses": [dict(boss, title="Я"), boss]}, {0})
        me, foe = init["bosses"]
        self.assertEqual(me["hp"], 133)
        for k in ("hp", "ac", "stats", "items", "text"):
            self.assertNotIn(k, foe)
        self.assertNotIn("hp", foe["minion"])
        self.assertEqual((foe["title"], foe["abil"]), ("Враг", [{"title": "Удар"}]))   # имя и способности — для журнала

    @need_engine
    def test_solo_dark_fight_sends_no_hidden_data(self):
        """Настоящий бой во тьме через сервер: у скрытых бойцов в ответах нет ни координат, ни ХП; сборка врага — без чисел."""
        ps.SOLO.pop("it-fog", None)
        mp = {"aw": 20, "ah": 14, "dark": ["1" * 20] * 14}
        srv = ps.ThreadingHTTPServer(("127.0.0.1", 0), ps.Handler)
        threading.Thread(target=srv.serve_forever, daemon=True).start()
        try:
            def api(msg):
                req = urllib.request.Request("http://127.0.0.1:%d/api" % srv.server_address[1], data=json.dumps(msg, ensure_ascii=False).encode(), headers={"Content-Type": "application/json"})
                with urllib.request.urlopen(req, timeout=60) as r:
                    return json.loads(r.read())
            out = api({"cmd": "start", "sid": "it-fog", "char": KNIGHT, "seed": 3, "map": mp, "opp": {"mode": "generator"}})
            foe = out["init"]["bosses"][1 - out["init"]["me"]]
            self.assertNotIn("hp", foe)
            self.assertNotIn("items", foe)
            hidden = 0
            for _ in range(6):
                for fr in out.get("frames", []):
                    for f in fr["f"]:
                        if f.get("hid"):
                            hidden += 1
                            self.assertEqual((f["x"], f["y"], f["hp"], f["cond"]), (-1, -1, 1, []), f)
                if out["type"] != "turn":
                    break
                out = api({"cmd": "step", "sid": "it-fog", "c": "end"})
            self.assertGreater(hidden, 0, "во тьме враг должен быть скрыт хоть в одном кадре")
        finally:
            srv.shutdown()
            if "it-fog" in ps.SOLO:
                ps.SOLO.pop("it-fog")["eng"].kill()


if __name__ == "__main__":
    unittest.main()
