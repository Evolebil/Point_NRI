#!/usr/bin/env python3
"""Сервер игры: страница play.html и движок (build/arena_play по JSON-строкам).

Запуск:   python3 web/play_server.py [порт] [--public]   (по умолчанию 8770; --public — слушать все адреса, для туннеля)
Страница: web/play.html + web/css/play.css + web/js/*.js; общий код рисования — из web/viewer_template.html.
API:      POST /api, JSON. У каждого браузера свой id (sid): свой бой (start/step), свои запросы цен.
          Комнаты (v21, бои нескольких команд): room_create, room_join, room_set (персонаж, команда), room_bot (бот в команду),
          room_npc (противник из карточки персонажа — кампания),
          room_kick, room_start (хозяин), room_poll (состояние, кадры, мой ход), room_step (ход игрока своего места).
"""
import base64
import gzip
import json
import os
import queue
import random
import re
import smtplib
import string
import subprocess
import sys
import threading
import time
from email.message import EmailMessage
from urllib.parse import urlparse, parse_qs, unquote
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))   # корень проекта (сервер лежит в web/)
WEB = os.path.join(HERE, "web")
STATE = os.path.join(HERE, "state")   # игроки и журналы — не в git
os.makedirs(STATE, exist_ok=True)
ENGINE = os.path.join(HERE, "build", "arena_play")
TURN_TIMEOUT = 300   # сек: после пяти минут без ответа ход завершается за игрока
ROOM_IDLE = 3 * 3600  # сек: заброшенная комната удаляется
# предохранители сервера наружу (аудит кода, раздел Д, шаг 4)
MAX_BODY = 3 * 1024 * 1024   # байт: включает WebP-портрет в base64
ENGINE_TIMEOUT = 120         # сек: движок не ответил — процесс убит, игроку ошибка
MAX_SOLO = 32                # одиночных боёв (процессов движка) одновременно
MAX_ROOMS = 16               # комнат одновременно
MAX_PLAYERS = 5000           # игроков в players.json
MAX_PLAYER_BYTES = 256 * 1024   # персонаж одного игрока в players.json


class Refuse(Exception):
    """Отказ сервера по предохранителю: текст показывается игроку как есть."""


def log(*a):
    print(time.strftime("%H:%M:%S"), *a, file=sys.stderr, flush=True)


class Engine:
    """Процесс движка: строка JSON туда — строка JSON обратно."""

    def __init__(self):
        self.p = None
        self.q = None
        self.lock = threading.Lock()

    def proc(self):
        if self.p is None or self.p.poll() is not None:
            self.p = subprocess.Popen([ENGINE], cwd=HERE, stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True, bufsize=1, encoding="utf-8")
            # строки движка читает свой поток: ответ ждём с таймаутом, зависший движок не держит запрос вечно
            self.q = queue.Queue()
            threading.Thread(target=self._reader, args=(self.p, self.q), daemon=True).start()
        return self.p

    @staticmethod
    def _reader(p, q):
        for line in p.stdout:
            q.put(line)
        q.put("")

    def kill(self):
        if self.p is not None and self.p.poll() is None:
            try:
                self.p.kill()
            except Exception:  # noqa: BLE001
                pass
        self.p = None

    def talk(self, msg):
        p = self.proc()
        p.stdin.write(json.dumps(msg, ensure_ascii=False) + "\n")
        p.stdin.flush()
        try:
            line = self.q.get(timeout=ENGINE_TIMEOUT)
        except queue.Empty:
            self.kill()
            raise RuntimeError("движок не ответил за %d с" % ENGINE_TIMEOUT)
        if not line:
            self.kill()
            raise RuntimeError("движок закрылся")
        return json.loads(line)


QUERY = Engine()          # общий для запросов цен, каталога, карт (быстрые)
SOLO = {}                 # sid -> {"eng": Engine, "in_game": bool, "t": время}
SOLO_LOCK = threading.Lock()
ROOMS = {}                # код -> Room
ROOMS_LOCK = threading.Lock()
MATCH_TICKETS = {}        # sid -> заявка/результат быстрого боя; очередь общая для всех игроков
MATCH_LOCK = threading.Lock()
REPORT_TIMES = {}         # имя игрока -> отметки успешных отправок за последний час
REPORT_LOCK = threading.Lock()
# игроки по имени (v21): персонаж (состояние конструктора) и текущая комната — переживают смену адреса туннеля
PLAYERS_FILE = os.path.join(STATE, "players.json")
PLAYERS_LOCK = threading.Lock()
try:
    with open(PLAYERS_FILE, encoding="utf-8") as _f:
        PLAYERS = json.load(_f)
except Exception:  # noqa: BLE001
    PLAYERS = {}


def pkey(name):
    return " ".join(str(name or "").lower().split())[:40]


def player_set(name, **kw):
    k = pkey(name)
    if not k:
        return
    if len(json.dumps(kw, ensure_ascii=False)) > MAX_PLAYER_BYTES:
        raise Refuse("персонаж слишком большой для сохранения")
    with PLAYERS_LOCK:
        if k not in PLAYERS and len(PLAYERS) >= MAX_PLAYERS:
            raise Refuse("на сервере сохранено слишком много игроков")
        PLAYERS.setdefault(k, {}).update(kw)
        tmp = PLAYERS_FILE + ".tmp"
        with open(tmp, "w", encoding="utf-8") as f:
            json.dump(PLAYERS, f, ensure_ascii=False)
        os.replace(tmp, PLAYERS_FILE)


def player_chars_set(name, chars, active):
    k = pkey(name)
    if not k:
        return {"type": "error", "err": "Сначала укажите имя игрока."}
    if not isinstance(chars, list) or len(chars) > 100:
        return {"type": "error", "err": "На сервере можно хранить до 100 персонажей."}
    clean = []
    for item in chars:
        if not isinstance(item, dict) or not isinstance(item.get("character"), dict):
            return {"type": "error", "err": "Не удалось сохранить список персонажей."}
        c = item["character"]
        clean.append({"id": str(item.get("id", ""))[:80], "name": str(item.get("name") or c.get("name") or "Персонаж").strip()[:40],
                      "budget": max(0, min(100000, int(c.get("budget", item.get("budget", 0)) or 0))), "character": c,
                      "portrait": str(item.get("portrait", ""))[:300]})
    if len(json.dumps(clean, ensure_ascii=False)) > 4 * 1024 * 1024:
        return {"type": "error", "err": "Список персонажей слишком большой для хранения."}
    with PLAYERS_LOCK:
        if k not in PLAYERS and len(PLAYERS) >= MAX_PLAYERS:
            return {"type": "error", "err": "На сервере сохранено слишком много игроков."}
        PLAYERS.setdefault(k, {})["chars"] = clean
        PLAYERS[k]["activeChar"] = str(active or "")[:80]
        if not clean:
            PLAYERS[k].pop("s", None)
            PLAYERS[k]["activeChar"] = ""
        tmp = PLAYERS_FILE + ".tmp"
        with open(tmp, "w", encoding="utf-8") as f:
            json.dump(PLAYERS, f, ensure_ascii=False)
        os.replace(tmp, PLAYERS_FILE)
    return {"type": "ok"}


def solo(sid):
    with SOLO_LOCK:
        s = SOLO.get(sid)
        if s is None:
            if len(SOLO) >= MAX_SOLO:   # предел процессов: освободить самый давний бой, если он простаивает больше 10 минут
                k, old = min(SOLO.items(), key=lambda kv: kv[1]["t"])
                if time.time() - old["t"] < 600:
                    raise Refuse("сервер занят: идёт слишком много боёв, попробуйте позже")
                old["eng"].kill()
                del SOLO[k]
            s = SOLO[sid] = {"eng": Engine(), "in_game": False, "t": time.time()}
        s["t"] = time.time()
        # старые одиночные бои (больше часа без хода) — закрыть
        for k in [k for k, v in SOLO.items() if time.time() - v["t"] > 3600 and k != sid]:
            SOLO[k]["eng"].kill()
            del SOLO[k]
        return s


class Room:
    def __init__(self, code, host):
        self.code, self.host = code, host
        self.members = []      # {"sid", "name", "team", "char", "bot": бюджет или None}
        self.status = "lobby"  # lobby | fight | end
        self.eng = None
        self.init = None
        self.frames = []
        self.pending = None    # {"seat", "req", "t"}
        self.end = None
        self.err = ""
        self.opts = {}
        self.lock = threading.Lock()
        self.order = []        # имена участников боя по порядку (для итогового состояния)
        self.t = time.time()
        self.seats = {}        # sid -> место (номер человека в составе)
        self.ver = 0           # растёт при любом изменении (долгий опрос ждёт его)
        self.cv = threading.Condition()

    def bump(self):
        with self.cv:
            self.ver += 1
            self.cv.notify_all()

    def view(self, sid, since):
        mine = self.pending if self.pending and self.seats.get(sid, -2) == self.pending["seat"] else None
        who = ""
        if self.pending:
            for s, k in self.seats.items():
                if k == self.pending["seat"]:
                    who = next((m["name"] for m in self.members if m["sid"] == s), "")
        return {"type": "room", "code": self.code, "host": self.host == sid, "status": self.status, "err": self.err,
                "members": [{"name": m["name"], "team": m["team"], "bot": m["bot"], "brain": m.get("brain", "rules"), "npc": bool(m.get("npc")), "me": m["sid"] == sid, "host": m["sid"] == self.host, "selected": bool(m.get("char")), "ready": bool(m.get("ready")) or m["bot"] is not None,
                             "i": i} for i, m in enumerate(self.members)],
                "seat": self.seats.get(sid, -1), "init": self.init if self.init and since == 0 else None,
                "frames": self.frames[since:], "nframes": len(self.frames), "req": mine["req"] if mine else None,
                "waiting": who, "end": self.end, "opts": self.opts, "ver": self.ver}

    def feed(self, out):
        """Ответ движка: кадры, чей ход или итог."""
        if out.get("type") == "error":
            self.status, self.err = "lobby", out.get("err", "ошибка")
            self.eng.kill()
            return
        if out.get("init") is not None:
            self.init = out["init"]
            bosses = self.init.get("bosses", []) if isinstance(self.init, dict) else []
            for i, boss in enumerate(bosses):
                member = self.members[i] if i < len(self.members) else {}
                char = member.get("char") if isinstance(member.get("char"), dict) else {}
                portrait = char.get("portrait")
                if portrait:
                    boss["portrait"] = portrait
        self.frames.extend(out.get("frames", []))
        if out.get("type") == "turn":
            self.pending = {"seat": out.get("seat", -1), "req": out.get("req"), "t": time.time()}
        else:
            self.pending = None
            self.status = "end"
            self.end = {k: out.get(k) for k in ("result", "reason", "rounds", "scores", "state")}
            self.end["who"] = self.order   # чьё состояние: имена участников по порядку (state[i] — у who[i])
            self.eng.kill()
            self.members = [m for m in self.members if not m.get("left")]

    def step(self, cmd):
        self.pending = None
        self.feed(self.eng.talk(cmd))
        self.bump()

    def tick(self):
        if self.status != "fight" or not self.pending:
            return
        gone = any(m.get("left") and self.seats.get(m["sid"]) == self.pending["seat"] for m in self.members)
        if gone or time.time() - self.pending["t"] > TURN_TIMEOUT:   # ушёл или не ходит — ход завершается
            self.step({"c": "end"})
            self.tick()


def new_code():
    while True:
        c = "".join(random.choice("ABCDEFGHJKLMNPQRSTUVWXYZ23456789") for _ in range(5))
        if c not in ROOMS:
            return c


def room_cmd(msg, sid):
    """Команда комнаты. room_poll с "ver": ждёт (до 20 с), пока состояние комнаты не изменится — меньше запросов через туннель."""
    out = room_cmd1(msg, sid)
    cmd = msg.get("cmd")
    r = ROOMS.get(str(msg.get("code", "")).upper().strip()) or (ROOMS.get(out.get("code", "")) if isinstance(out, dict) else None)
    if r is None or out.get("type") != "room":
        return out
    if cmd != "room_poll":
        r.bump()
        if cmd == "room_leave":
            return out
        with r.lock:
            return r.view(sid, int(msg.get("since", 0)))
    want = msg.get("ver")
    if want is None or int(want) != r.ver:
        return out
    deadline = time.time() + 20
    while time.time() < deadline:   # порядок замков всегда r.lock -> r.cv (bump берёт cv под r.lock)
        with r.lock:   # пока ждём — ходы ушедших и просроченные завершаются
            try:
                r.tick()
            except Exception:  # noqa: BLE001
                pass
        with r.cv:
            if r.ver != int(want):
                break
            r.cv.wait(min(5, max(0.1, deadline - time.time())))
            if r.ver != int(want):
                break
    with r.lock:
        return r.view(sid, int(msg.get("since", 0)))


def room_cmd1(msg, sid):
    cmd = msg.get("cmd")
    with ROOMS_LOCK:
        for k in [k for k, r in ROOMS.items() if time.time() - r.t > ROOM_IDLE]:
            if ROOMS[k].eng:
                ROOMS[k].eng.kill()
            del ROOMS[k]
        r = None if cmd == "room_create" else ROOMS.get(str(msg.get("code", "")).upper().strip())
        if cmd != "room_create" and r is None:
            return {"type": "error", "err": "Комната не найдена"}
        # имя занято живым игроком другой комнаты (замок ROOMS_LOCK уже взят — второй раз брать нельзя: вечное ожидание)
        name = str(msg.get("name") or "").strip()
        if cmd in ("room_create", "room_join") and name:
            for other in ROOMS.values():
                if other is not r and any(m.get("bot") is None and not m.get("left") and pkey(m.get("name")) == pkey(name) for m in other.members):
                    return {"type": "error", "err": "Имя занято: этот игрок уже в другой комнате"}
        if cmd == "room_create":
            if len(ROOMS) >= MAX_ROOMS:
                return {"type": "error", "err": "сервер занят: слишком много комнат, попробуйте позже"}
            r = Room(new_code(), sid)
            ROOMS[r.code] = r
    with r.lock:
        r.t = time.time()
        me = next((m for m in r.members if m["sid"] == sid), None)
        if cmd in ("room_create", "room_join"):
            if me is None and msg.get("name"):   # вернулся по новой ссылке: то же имя — то же место (и в идущем бою)
                old = next((m for m in r.members if m["bot"] is None and pkey(m["name"]) == pkey(msg["name"])), None)
                if old is not None:
                    if old["sid"] != sid and not old.get("left"):
                        return {"type": "error", "err": "Имя занято"}
                    if old["sid"] in r.seats:
                        r.seats[sid] = r.seats.pop(old["sid"])
                    if r.host == old["sid"]:
                        r.host = sid
                    old["sid"], old["left"] = sid, False
                    me = old
            if me is None:
                if r.status == "fight":
                    return {"type": "error", "err": "бой уже идёт — войдите после него (или под своим именем, если вы в нём)"}
                me = {"sid": sid, "name": (msg.get("name") or "Игрок")[:24], "team": len({m["team"] for m in r.members}) % 2, "char": None, "bot": None, "ready": False}
                r.members.append(me)
            if msg.get("name"):
                me["name"] = str(msg["name"]).strip()[:24]
            player_set(me["name"], room=r.code)
        elif cmd == "room_set":
            if me is None:
                return {"type": "error", "err": "вы не в этой комнате"}
            target = me
            if "i" in msg:
                if r.host != sid:
                    return {"type": "error", "err": "менять другого участника может только хозяин комнаты"}
                i = int(msg.get("i", -1))
                if not (0 <= i < len(r.members)):
                    return {"type": "error", "err": "участник не найден"}
                target = r.members[i]
            if "team" in msg:
                target["team"] = max(0, min(63, int(msg["team"])))
            if "char" in msg:
                target["char"] = msg["char"]
                target["ready"] = bool(msg["char"])
            if "brain" in msg:
                if target.get("bot") is None or target.get("npc") or r.host != sid or r.status != "lobby":
                    return {"type": "error", "err": "выбирать, кто ведёт бота, может хозяин комнаты до боя"}
                target["brain"] = "net" if msg["brain"] == "net" else "rules"
            if "ready" in msg:
                if msg["ready"] and not target.get("char"):
                    return {"type": "error", "err": "Сначала выберите персонажа."}
                target["ready"] = bool(msg["ready"])
            if msg.get("name"):
                me["name"] = str(msg["name"]).strip()[:24]
        elif cmd == "room_bot":
            if r.host != sid or r.status != "lobby":
                return {"type": "error", "err": "ботов добавляет хозяин комнаты до боя"}
            r.members.append({"sid": "bot%d" % random.randint(0, 1 << 30), "name": "Бот %d" % (1 + sum(m["bot"] is not None for m in r.members)),
                              "team": max(0, min(63, int(msg.get("team", 1)))), "char": None, "bot": max(20, min(100000, int(msg.get("budget", 500)))),
                              "brain": "net" if msg.get("brain") == "net" else "rules"})
        elif cmd == "room_npc":   # противник из карточки (кампания, задача Б-2): персонаж без игрока — им ходит бот или сеть по ИНТ
            if r.host != sid or r.status != "lobby":
                return {"type": "error", "err": "противников добавляет хозяин комнаты до боя"}
            if not isinstance(msg.get("char"), dict):
                return {"type": "error", "err": "нужна карточка персонажа (char)"}
            r.members.append({"sid": "npc%d" % random.randint(0, 1 << 30), "name": str(msg.get("name") or msg["char"].get("name") or "Противник")[:40],
                              "team": max(0, min(63, int(msg.get("team", 1)))), "char": msg["char"], "bot": 0, "npc": True})
        elif cmd == "room_kick":
            if r.host != sid or r.status != "lobby":
                return {"type": "error", "err": "убирает хозяин до боя"}
            i = int(msg.get("i", -1))
            if 0 <= i < len(r.members) and r.members[i]["sid"] != sid:
                r.members.pop(i)
        elif cmd == "room_host":
            if r.host != sid or r.status != "lobby":
                return {"type": "error", "err": "Передать комнату может только хозяин до боя."}
            i = int(msg.get("i", -1))
            if not (0 <= i < len(r.members)) or r.members[i].get("bot") is not None:
                return {"type": "error", "err": "Выберите игрока, которому передать комнату."}
            r.host = r.members[i]["sid"]
        elif cmd == "room_start":
            if r.host != sid:
                return {"type": "error", "err": "начинает хозяин комнаты"}
            if r.status == "fight":
                return {"type": "error", "err": "бой уже идёт"}
            humans = [m for m in r.members if m["bot"] is None]   # NPC из карточки — не игроки (bot = 0)
            if any(not m["char"] for m in humans):
                return {"type": "error", "err": "не все игроки выбрали персонажа: " + ", ".join(m["name"] for m in humans if not m["char"])}
            if any(not m.get("ready") for m in humans):
                return {"type": "error", "err": "не все игроки готовы: " + ", ".join(m["name"] for m in humans if not m.get("ready"))}
            if len({m["team"] for m in r.members}) < 2:
                return {"type": "error", "err": "для начала боя нужны как минимум две команды"}
            r.seats = {m["sid"]: k for k, m in enumerate(humans)}
            members = []
            for m in r.members:
                e = {"team": m["team"], "name": m["name"]}
                if m.get("npc"):
                    e["char"] = m["char"]   # без места: ходит бот или сеть
                elif m["bot"] is not None:
                    e["budget"] = m["bot"]
                    if m.get("brain"):
                        e["brain"] = m["brain"]
                else:
                    e["char"], e["seat"] = m["char"], r.seats[m["sid"]]
                members.append(e)
            r.order = [m["name"] for m in r.members]
            opts = {k: msg[k] for k in ("seed", "rounds", "map", "world") if k in msg}
            r.opts = {k: v for k, v in opts.items() if k in ("seed", "rounds")}
            r.status, r.err, r.frames, r.init, r.end, r.pending = "fight", "", [], None, None, None
            if r.eng:
                r.eng.kill()
            r.eng = Engine()
            try:
                r.feed(r.eng.talk(dict(cmd="teams", members=members, seed=int(opts.get("seed", random.randint(1, 99999))), **{k: v for k, v in opts.items() if k != "seed"})))
            except Exception as e:  # noqa: BLE001
                log("комната", r.code, "ошибка:", repr(e))
                r.status, r.err = "lobby", "ошибка движка (подробности — в журнале сервера)"
        elif cmd == "room_step":
            if r.status != "fight" or not r.pending or r.seats.get(sid, -2) != r.pending["seat"]:
                return {"type": "error", "err": "сейчас не ваш ход"}
            inner = {k: v for k, v in msg.items() if k not in ("cmd", "code", "sid", "since")}
            try:
                r.step(inner)
                r.tick()
            except Exception as e:  # noqa: BLE001
                log("комната", r.code, "ошибка:", repr(e))
                r.status, r.err = "lobby", "ошибка движка (подробности — в журнале сервера)"
        elif cmd == "room_leave":   # в лобби — ушёл; в бою — его ходы завершаются сами, после боя он уходит
            if me is not None:
                player_set(me["name"], room="")
                if r.status == "fight":
                    me["left"] = True
                    r.tick()
                else:
                    r.members.remove(me)
                    if r.host == sid:
                        r.host = next((m["sid"] for m in r.members if m["bot"] is None), "")
                    if not any(m["bot"] is None for m in r.members):
                        if r.eng:
                            r.eng.kill()
                        ROOMS.pop(r.code, None)
            return {"type": "left"}
        else:
            try:
                r.tick()
            except Exception as e:  # noqa: BLE001
                log("комната", r.code, "ошибка:", repr(e))
                r.status, r.err = "lobby", "ошибка движка (подробности — в журнале сервера)"
        return r.view(sid, int(msg.get("since", 0)))


_SHEET = {"mod": None, "mtime": 0.0}


def sheet_mod():
    """sheet.py: импорт один раз, повторно — только если файл поменялся (правка видна без перезапуска сервера)."""
    import importlib
    mt = os.path.getmtime(os.path.join(HERE, "tools", "sheet.py"))
    if _SHEET["mod"] is None:
        d = os.path.join(HERE, "tools")   # sheet.py — в tools/
        if d not in sys.path:
            sys.path.insert(0, d)
        import sheet
        _SHEET["mod"] = sheet
    elif mt != _SHEET["mtime"]:
        importlib.reload(_SHEET["mod"])
    _SHEET["mtime"] = mt
    return _SHEET["mod"]


def page():
    """Страница одним ответом (через туннель лишние запросы дороги): web/play.html + стили web/css/play.css + скрипты
    web/js/*.js по порядку имён; в скрипт боя подставляется общий код рисования из web/viewer_template.html."""
    def rd(*p):
        with open(os.path.join(WEB, *p), encoding="utf-8") as f:
            return f.read()
    v = rd("viewer_template.html")
    a, b = v.index("/*RENDER_BEGIN*/"), v.index("/*RENDER_END*/")
    js = "".join(rd("js", n) for n in sorted(os.listdir(os.path.join(WEB, "js"))) if n.endswith(".js"))
    return rd("play.html").replace("/*PLAY_CSS*/\n", rd("css", "play.css")).replace("/*PLAY_JS*/\n", js.replace("/*RENDER_JS*/", v[a:b]))


def quick_match_room(players):
    """Создать комнату с двумя реальными игроками после пары в очереди."""
    if not isinstance(players, list) or len(players) != 2:
        return {"type": "error", "err": "для боя нужны два игрока"}
    clean = []
    for p in players:
        if not isinstance(p, dict) or not isinstance(p.get("char"), dict) or not p.get("sid"):
            return {"type": "error", "err": "не удалось проверить заявку на бой"}
        budget = int(p.get("budget", 0))
        if budget <= 0 or int(p["char"].get("budget", 0)) != budget:
            return {"type": "error", "err": "бюджет персонажа не совпадает с заявкой"}
        clean.append({"sid": str(p["sid"])[:64], "name": str(p.get("name") or "Игрок").strip()[:24], "char": p["char"], "budget": budget})
    with ROOMS_LOCK:
        if len(ROOMS) >= MAX_ROOMS:
            return {"type": "error", "err": "сервер занят: слишком много комнат, попробуйте позже"}
        r = Room(new_code(), clean[0]["sid"])
        r.members = [
            {"sid": clean[0]["sid"], "name": clean[0]["name"], "team": 0, "char": clean[0]["char"], "bot": None, "ready": True},
            {"sid": clean[1]["sid"], "name": clean[1]["name"], "team": 1, "char": clean[1]["char"], "bot": None, "ready": True},
        ]
        ROOMS[r.code] = r
    return {"type": "quick_match_room", "code": r.code}


def match_cmd(msg, sid):
    cmd = msg.get("cmd")
    now = time.time()
    if cmd == "quick_match_create":
        return quick_match_room(msg.get("players"))
    if cmd == "match_cancel":
        with MATCH_LOCK:
            ticket = MATCH_TICKETS.get(sid)
            if ticket and ticket.get("status") == "waiting":
                ticket["status"] = "cancelled"
        return {"type": "match_cancelled"}
    if cmd == "match_find":
        char = msg.get("char")
        try:
            budget = int(msg.get("budget", 0))
            if not isinstance(char, dict) or budget <= 0 or int(char.get("budget", 0)) != budget:
                return {"type": "error", "err": "Бюджет персонажа должен совпадать с выбранной суммой."}
            name = str(msg.get("name") or char.get("name") or "Игрок").strip()[:24] or "Игрок"
        except (TypeError, ValueError):
            return {"type": "error", "err": "Не удалось проверить бюджет персонажа."}
        with MATCH_LOCK:
            old = MATCH_TICKETS.get(sid)
            if old and old.get("status") in ("match", "bot"):
                return dict(old["result"])
            if old and old.get("status") == "waiting" and old.get("budget") == budget:
                if now - old["created"] >= 60:
                    old["status"], old["result"] = "bot", {"type": "match_bot", "budget": budget}
                    return dict(old["result"])
                old["lastSeen"] = now
                return {"type": "match_wait", "budget": budget, "elapsed": int(now - old["created"])}
            if old and old.get("status") == "waiting":
                old["status"] = "cancelled"
            ticket = {"status": "waiting", "created": now, "lastSeen": now, "budget": budget, "sid": sid, "name": name, "char": char}
            others = sorted((t for k, t in MATCH_TICKETS.items() if k != sid and t.get("status") == "waiting" and t.get("budget") == budget and now - t.get("lastSeen", t["created"]) <= 15 and now - t["created"] <= 60), key=lambda t: t["created"])
            if any(pkey(t.get("name")) == pkey(name) for t in others):
                return {"type": "error", "err": "Поиск уже идёт"}
            if others:
                first = others[0]
                room = quick_match_room([first, ticket])
                if room.get("type") != "quick_match_room":
                    return room
                result = {"type": "match", "code": room["code"]}
                first["status"], first["result"] = "match", result
                ticket["status"], ticket["result"] = "match", result
                MATCH_TICKETS[sid] = ticket
                return dict(result)
            MATCH_TICKETS[sid] = ticket
            for k, t in list(MATCH_TICKETS.items()):
                if t.get("status") in ("cancelled", "bot", "match") and now - t.get("created", now) > 1800:
                    MATCH_TICKETS.pop(k, None)
        return {"type": "match_wait", "budget": budget, "elapsed": 0}
    if cmd == "match_poll":
        with MATCH_LOCK:
            ticket = MATCH_TICKETS.get(sid)
            if not ticket:
                return {"type": "error", "err": "Поиск боя завершён. Нажмите «Найти бой» ещё раз."}
            if ticket.get("status") in ("match", "bot"):
                return dict(ticket["result"])
            if ticket.get("status") == "cancelled":
                return {"type": "error", "err": "Поиск отменён."}
            if now - ticket.get("lastSeen", ticket["created"]) > 15:
                ticket["status"] = "cancelled"
                return {"type": "error", "err": "Поиск завершён. Нажмите «Найти бой» ещё раз."}
            ticket["lastSeen"] = now
            elapsed = int(now - ticket["created"])
            if elapsed >= 60:
                ticket["status"], ticket["result"] = "bot", {"type": "match_bot", "budget": ticket["budget"]}
                return dict(ticket["result"])
        return {"type": "match_wait", "budget": ticket["budget"], "elapsed": elapsed}
    return {"type": "error", "err": "Неизвестная команда поиска боя."}


def site_config():
    path = os.path.join(HERE, "config", "site.json")
    try:
        with open(path, encoding="utf-8") as f:
            value = json.load(f)
        if not isinstance(value, dict):
            raise ValueError("ожидался объект")
        return {"type": "site_config", "version": str(value.get("version", "0.1.0")),
                "developers": value.get("developers", []), "templates": value.get("templates", [])}
    except Exception:  # noqa: BLE001
        return {"type": "site_config", "version": "0.1.0", "developers": [], "templates": []}


def report_env():
    """Настройки почты для жалоб: переменные окружения или файл state/smtp.env (строки КЛЮЧ=значение, не в git).
    Файл читается при каждой отправке — после правки перезапуск сервера не нужен."""
    env = {k: os.environ.get(k, "") for k in ("SMTP_HOST", "SMTP_PORT", "SMTP_USER", "SMTP_PASSWORD", "SMTP_FROM", "SMTP_TLS", "REPORT_TO")}
    try:
        with open(os.path.join(STATE, "smtp.env"), encoding="utf-8") as f:
            for line in f:
                line = line.strip()
                if line and not line.startswith("#") and "=" in line:
                    k, v = line.split("=", 1)
                    if k.strip() in env and not env[k.strip()]:
                        env[k.strip()] = v.strip().strip('"').strip("'")
    except OSError:
        pass
    return env


def send_report(msg):
    category = str(msg.get("category", "Ошибка"))
    text = str(msg.get("text", "")).strip()
    contact = str(msg.get("contact", "")).strip()[:200]
    metadata = msg.get("metadata") if isinstance(msg.get("metadata"), dict) else {}
    name = str(metadata.get("name", "")).strip()[:24] or "Не указано"
    if category not in ("Ошибка", "Идея", "Другое") or not 10 <= len(text) <= 2000:
        return {"type": "error", "err": "Проверьте категорию и текст сообщения (10–2000 символов)."}
    now = time.time()
    key = pkey(name)
    with REPORT_LOCK:
        times = [t for t in REPORT_TIMES.get(key, []) if now - t < 3600]
        if times and now - times[-1] < 60 or len(times) >= 5:
            return {"type": "error", "err": "Слишком часто, попробуйте позже."}
        times.append(now)
        REPORT_TIMES[key] = times
    E = report_env()
    host = E.get("SMTP_HOST", "").strip()
    recipient = E.get("REPORT_TO", "").strip()
    if not host or not recipient:
        return {"type": "error", "err": "Не удалось отправить: отправка писем ещё не настроена."}
    try:
        port = int(E.get("SMTP_PORT", "465"))
        sender = E.get("SMTP_FROM", E.get("SMTP_USER", recipient)).strip()
        payload = (f"Категория: {category}\nИгрок: {name}\nКонтакт: {contact or 'не указан'}\n"
                   f"Версия: {str(metadata.get('version', '0.1.0'))[:40]}\nЭкран: {str(metadata.get('screen', ''))[:40]}\n"
                   f"Тема: {str(metadata.get('theme', ''))[:40]}\nБраузер: {str(metadata.get('browser', ''))[:300]}\n\n{text}")
        mail = EmailMessage()
        mail["Subject"] = f"Горнило Миров — {category}"
        mail["From"] = sender
        mail["To"] = recipient
        mail.set_content(payload)
        if port == 465:
            with smtplib.SMTP_SSL(host, port, timeout=15) as smtp:
                user = E.get("SMTP_USER", "").strip()
                if user:
                    smtp.login(user, E.get("SMTP_PASSWORD", ""))
                smtp.send_message(mail)
        else:
            with smtplib.SMTP(host, port, timeout=15) as smtp:
                smtp.ehlo()
                if E.get("SMTP_TLS", "1") != "0":
                    smtp.starttls()
                    smtp.ehlo()
                user = E.get("SMTP_USER", "").strip()
                if user:
                    smtp.login(user, E.get("SMTP_PASSWORD", ""))
                smtp.send_message(mail)
        return {"type": "ok"}
    except Exception:  # noqa: BLE001
        with REPORT_LOCK:
            REPORT_TIMES[key] = [t for t in REPORT_TIMES.get(key, []) if t != now]
        return {"type": "error", "err": "Не удалось отправить, повторите."}


class Handler(BaseHTTPRequestHandler):
    def log_message(self, *a):
        pass

    def _send(self, code, body, ctype="application/json; charset=utf-8"):
        data = body if isinstance(body, bytes) else body.encode("utf-8")
        gz = len(data) > 1024 and "gzip" in self.headers.get("Accept-Encoding", "")   # сжатие: через туннель канал узкий
        if gz:
            data = gzip.compress(data, 6)
        self.send_response(code)
        if gz:
            self.send_header("Content-Encoding", "gzip")
        self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(len(data)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(data)

    def do_GET(self):
        u = urlparse(self.path)
        path = u.path
        if path.startswith("/portrait/"):
            portrait_id = unquote(path[len("/portrait/"):])
            if not re.fullmatch(r"[A-Za-z0-9_-]{16,80}", portrait_id):
                self._send(404, "не найдено", "text/plain; charset=utf-8")
                return
            try:
                with open(os.path.join(STATE, "portraits", portrait_id + ".webp"), "rb") as f:
                    self._send(200, f.read(), "image/webp")
            except OSError:
                self._send(404, "не найдено", "text/plain; charset=utf-8")
        elif path == "/sheet":
            try:
                b = int(parse_qs(u.query).get("budget", ["800"])[0])
                self._send(200, sheet_mod().sheet(b), "text/markdown; charset=utf-8")
            except Exception as e:  # noqa: BLE001
                log("шпаргалка:", repr(e))
                self._send(500, "ошибка шпаргалки (подробности — в журнале сервера)", "text/plain; charset=utf-8")
        elif path in ("/", "/play.html"):
            try:
                self._send(200, page(), "text/html; charset=utf-8")
            except Exception as e:  # noqa: BLE001
                log("страница:", repr(e))
                self._send(500, "ошибка сборки страницы (подробности — в журнале сервера)", "text/plain; charset=utf-8")
        else:
            self._send(404, "не найдено", "text/plain; charset=utf-8")

    def do_POST(self):
        if self.path != "/api":
            self._send(404, "{}")
            return
        try:
            n = int(self.headers.get("Content-Length", "0"))
        except ValueError:
            n = -1
        if n < 0 or n > MAX_BODY:
            self._send(413, json.dumps({"type": "error", "err": "запрос слишком большой"}, ensure_ascii=False))
            return
        try:
            msg = json.loads(self.rfile.read(n).decode("utf-8"))
        except Exception:  # noqa: BLE001
            self._send(400, json.dumps({"type": "error", "err": "не JSON"}, ensure_ascii=False))
            return
        if not isinstance(msg, dict) or not isinstance(msg.get("cmd", ""), str):
            self._send(400, json.dumps({"type": "error", "err": "ожидается объект JSON с полем cmd"}, ensure_ascii=False))
            return
        sid = str(msg.pop("sid", "") or "anon")[:64]
        for k in ("opp", "opts"):   # путь к весам Сборщика выбирает сервер, не браузер (иначе движок открыл бы любой файл)
            if isinstance(msg.get(k), dict):
                msg[k].pop("builder", None)
        cmd = msg.get("cmd")
        try:
            if cmd and cmd.startswith("room_"):
                out = room_cmd(msg, sid)
            elif cmd in ("match_find", "match_poll", "match_cancel", "quick_match_create"):
                out = match_cmd(msg, sid)
            elif cmd == "site_config":
                out = site_config()
            elif cmd == "report":
                out = send_report(msg)
            elif cmd == "portrait_put":
                portrait_id = str(msg.get("portraitId") or "")
                data = str(msg.get("data") or "")
                match = re.fullmatch(r"data:image/webp;base64,([A-Za-z0-9+/=]+)", data)
                if not re.fullmatch(r"[A-Za-z0-9_-]{16,80}", portrait_id) or not match or len(match[1]) > 2800000:
                    out = {"type": "error", "err": "Портрет должен быть WebP до 2 МБ."}
                else:
                    try:
                        raw = base64.b64decode(match[1], validate=True)
                        if len(raw) > 2 * 1024 * 1024 or len(raw) < 12 or raw[:4] != b"RIFF" or raw[8:12] != b"WEBP":
                            raise ValueError("invalid WebP")
                        portrait_dir = os.path.join(STATE, "portraits")
                        os.makedirs(portrait_dir, exist_ok=True)
                        tmp = os.path.join(portrait_dir, portrait_id + ".tmp")
                        with open(tmp, "wb") as f:
                            f.write(raw)
                        os.replace(tmp, os.path.join(portrait_dir, portrait_id + ".webp"))
                        out = {"type": "portrait", "url": "/portrait/" + portrait_id}
                    except Exception:  # noqa: BLE001
                        out = {"type": "error", "err": "Файл портрета повреждён или не удалось сохранить."}
            elif cmd == "portrait_delete":
                portrait_id = str(msg.get("portrait") or "").split("/")[-1]
                if not re.fullmatch(r"[A-Za-z0-9_-]{16,80}", portrait_id):
                    out = {"type": "error", "err": "Некорректный идентификатор портрета."}
                else:
                    try:
                        os.remove(os.path.join(STATE, "portraits", portrait_id + ".webp"))
                    except FileNotFoundError:
                        pass
                    out = {"type": "ok"}
            elif cmd and cmd.startswith("kmp_"):   # кампания: исследование и социалка (web/kampaniya.py)
                import kampaniya
                out = kampaniya.kmp_cmd(msg, sid)
            elif cmd == "player_save":   # персонаж под именем игрока
                player_set(msg.get("name"), s=msg.get("s"))
                out = {"type": "ok"}
            elif cmd == "player_load":
                with PLAYERS_LOCK:
                    p = dict(PLAYERS.get(pkey(msg.get("name")), {}))
                if p.get("room") and p["room"] not in ROOMS:
                    p["room"] = ""
                out = {"type": "player", "s": p.get("s"), "room": p.get("room", "")}
            elif cmd == "player_chars_load":
                with PLAYERS_LOCK:
                    p = dict(PLAYERS.get(pkey(msg.get("name")), {}))
                out = {"type": "player_chars", "chars": p.get("chars", []), "active": p.get("activeChar", "")}
            elif cmd == "player_chars_save":
                out = player_chars_set(msg.get("name"), msg.get("chars"), msg.get("active"))
            elif cmd in ("start", "step"):
                s = solo(sid)
                with s["eng"].lock:
                    if cmd == "start":
                        s["eng"].kill()
                        out = s["eng"].talk(msg)
                        char = msg.get("char") if isinstance(msg.get("char"), dict) else {}
                        if char.get("portrait") and isinstance(out.get("init"), dict):
                            bosses = out["init"].get("bosses", [])
                            own = int(out["init"].get("me", 0))
                            if 0 <= own < len(bosses):
                                bosses[own]["portrait"] = str(char["portrait"])[:300]
                        s["in_game"] = out.get("type") == "turn"
                    elif not s["in_game"]:
                        out = {"type": "error", "err": "игра не идёт"}
                    else:
                        try:
                            out = s["eng"].talk({k: v for k, v in msg.items() if k != "cmd"})
                        except Exception as e:  # noqa: BLE001 — движок упал или завис: бой не продолжить
                            s["in_game"] = False
                            log("api", "step", "ошибка:", repr(e))
                            raise Refuse("Бой прерван: движок перезапущен, начните новый бой.") from e
                        # ответ-ошибка (не тот ход) не обрывает бой — игрок просто ходит заново
                        s["in_game"] = out.get("type") in ("turn", "error")
            elif cmd in ("catalog", "price", "preview", "genmap", "checkmap", "worldmap", "regionmap"):
                with QUERY.lock:
                    out = QUERY.talk(msg)
            else:
                out = {"type": "error", "err": "неизвестная команда"}
        except Refuse as e:   # ожидаемые отказы сервера (предохранители): текст для игрока
            out = {"type": "error", "err": str(e)}
        except Exception as e:  # noqa: BLE001
            log("api", cmd, "ошибка:", repr(e))
            out = {"type": "error", "err": "ошибка движка (подробности — в журнале сервера)"}
        status = 403 if cmd == "kmp_delete" and out.get("err") == "Удалить кампанию может только мастер." else 200
        self._send(status, json.dumps(out, ensure_ascii=False))


if __name__ == "__main__":
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    port = int(args[0]) if args else 8770
    host = "0.0.0.0" if "--public" in sys.argv else "127.0.0.1"
    if not os.path.exists(ENGINE):
        sys.exit("нет %s: соберите командой make" % ENGINE)
    srv = ThreadingHTTPServer((host, port), Handler)
    print("Игра: http://127.0.0.1:%d/   (Ctrl+C — выход)" % port)
    try:
        srv.serve_forever()
    except KeyboardInterrupt:
        pass
