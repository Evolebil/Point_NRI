#!/usr/bin/env python3
"""Кампания (исследование и социалка, ТЗ docs/TZ_issledovanie_socialka.md): хранилище журнала и загрузка Obsidian.

Правила считает build/kampaniya (C++); здесь — только:
  * журнал кампании state/kampaniya/<id>/journal.jsonl (одна строка — одно принятое событие) и meta.json;
  * черновики событий: propose → мастер принимает (с правкой итога или без), перебрасывает или отменяет;
  * чтение хранилища Obsidian (только чтение, ДАН-05): таблицы встреч акта как написаны (ДАН-03) и карточки с полем
    «тип» во frontmatter (npc, вещь, задание, диалог); файлы без «тип» не трогаются.

Путь к хранилищу — config/kampaniya.json (ДАН-01). Проверка: python3 web/kampaniya.py check
"""
import json
import os
import random
import re
import subprocess
import sys
import threading
import time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
STATE = os.path.join(ROOT, "state", "kampaniya")
CONFIG = os.path.join(ROOT, "config", "kampaniya.json")


def config():
    try:
        with open(CONFIG, encoding="utf-8") as f:
            c = json.load(f)
    except (OSError, ValueError):
        c = {}
    c["хранилище"] = os.path.expanduser(c.get("хранилище", ""))
    return c


# ---------------------------------------------------------------- Obsidian: таблицы встреч акта

TERRAINS = ["Пашни и сёла", "Лес", "Холмы и плато", "Горы", "Болото", "Река и переправа", "Город"]
# названия местности в таблице засад Акта I → местности путей
AMBUSH_ALIAS = {"Пашни": ["Пашни и сёла"], "Лес": ["Лес"], "Холмы и горы": ["Холмы и плато", "Горы"], "Болото": ["Болото"],
                "Река": ["Река и переправа"], "Город": ["Город"]}


def _terrain(name):
    """«Город (ночёвка)», «Горы (Пиренеи, Альпы)» → «Город», «Горы»."""
    n = re.sub(r"\s*\(.*\)\s*$", "", name.strip())
    return n if n in TERRAINS else None


def _rows(lines, i):
    """Строки таблицы markdown, начиная с заголовка в lines[i]: [[ячейки], …] без строки «|---|»."""
    out = []
    while i < len(lines) and lines[i].lstrip().startswith("|"):
        cells = [c.strip() for c in lines[i].strip().strip("|").split("|")]
        if not all(re.fullmatch(r":?-{2,}:?", c) for c in cells):
            out.append(cells)
        i += 1
    return out


def parse_encounters(text, errors, src="акт"):
    """Пороги, таблицы к12 и засады из текста акта (формат «Путь и встречи» Акта I)."""
    lines = text.split("\n")
    thr, table, ambush = {}, {}, {}
    section = None
    for i, ln in enumerate(lines):
        h = re.match(r"^#{2,4}\s+(.*)$", ln)
        if h:
            section = _terrain(h.group(1))
            continue
        if not ln.lstrip().startswith("|") or (i > 0 and lines[i - 1].lstrip().startswith("|")):
            continue   # таблица разбирается с её первой строки
        rows = _rows(lines, i)
        if not rows:
            continue
        head = [c.lower() for c in rows[0]]
        if head[:2] == ["местность", "порог встречи (к20)"]:
            for r in rows[1:]:
                t = _terrain(r[0])
                if t is None or not r[1].strip().isdigit():
                    errors.append(f"{src}, строка {i + 1}: порог «{r[0]}» не разобран")
                    continue
                thr[t] = int(r[1])
        elif head[:2] == ["к12", "встреча"] and section:
            rows12 = {}
            for r in rows[1:]:
                if r[0].isdigit():
                    rows12[int(r[0])] = r[1]
            if sorted(rows12) != list(range(1, 13)):
                errors.append(f"{src}, строка {i + 1}: в таблице «{section}» не все строки к12 1–12")
            table[section] = [rows12.get(k, "") for k in range(1, 13)]
        elif head[:3] == ["местность", "где засада", "кто"]:
            for r in rows[1:]:
                for t in AMBUSH_ALIAS.get(r[0], []):
                    ambush[t] = f"{r[1]} — {r[2]}"
                if r[0] not in AMBUSH_ALIAS:
                    errors.append(f"{src}, строка {i + 1}: местность засады «{r[0]}» не известна")
    return {"thr": thr, "table": table, "ambush": ambush}


# ---------------------------------------------------------------- Obsidian: карточки (frontmatter)

def _scalar(v):
    v = v.strip()
    if not v:
        return ""
    if v[0] in "\"'" and v[-1] == v[0] and len(v) >= 2:
        return v[1:-1]
    if v.startswith("[") and v.endswith("]"):
        return [_scalar(x) for x in _split(v[1:-1])] if v[1:-1].strip() else []
    if v.startswith("{") and v.endswith("}"):
        d = {}
        for part in _split(v[1:-1]):
            if ":" in part:
                k, x = part.split(":", 1)
                d[k.strip()] = _scalar(x)
        return d
    if re.fullmatch(r"-?\d+", v):
        return int(v)
    if re.fullmatch(r"-?\d+\.\d+", v):
        return float(v)
    if v in ("true", "false"):
        return v == "true"
    return v


def _split(s):
    """Разделить по запятым верхнего уровня (внутри [], {}, кавычек — не делить)."""
    out, depth, cur, quote = [], 0, "", None
    for ch in s:
        if quote:
            cur += ch
            quote = None if ch == quote else quote
            continue
        if ch in "\"'":
            quote = ch
        elif ch in "[{":
            depth += 1
        elif ch in "]}":
            depth -= 1
        elif ch == "," and depth == 0:
            out.append(cur)
            cur = ""
            continue
        cur += ch
    if cur.strip():
        out.append(cur)
    return out


def frontmatter(text):
    """Свойства Obsidian: простой YAML — «ключ: значение», списки [..] и «- пункт», словари {..}; комментарии #."""
    if not text.startswith("---"):
        return {}, text
    end = text.find("\n---", 3)
    if end < 0:
        return {}, text
    head, body = text[3:end], text[end + 4:].lstrip("\n")
    props, key = {}, None
    for ln in head.split("\n"):
        s = re.sub(r"\s+#.*$", "", ln).rstrip()
        if not s.strip():
            continue
        m = re.match(r"^\s*-\s+(.*)$", s)
        if m and key is not None:
            if not isinstance(props.get(key), list):
                props[key] = []
            props[key].append(_scalar(m.group(1)))
            continue
        m = re.match(r"^([^:\s][^:]*):\s*(.*)$", s)
        if m:
            key = m.group(1).strip()
            props[key] = _scalar(m.group(2))
    return props, body


def load_cards(vault, errors):
    """Все заметки хранилища с полем «тип»: {тип: {имя: свойства}}; имя — поле «имя» или имя файла."""
    cards = {}
    for dp, dn, fn in os.walk(vault):
        dn[:] = [d for d in dn if not d.startswith(".")]
        for f in fn:
            if not f.endswith(".md"):
                continue
            p = os.path.join(dp, f)
            try:
                with open(p, encoding="utf-8") as fh:
                    raw = fh.read()
                props, body = frontmatter(raw)
            except (OSError, UnicodeDecodeError) as e:
                errors.append(f"{os.path.relpath(p, vault)}: не читается ({e})")
                continue
            t = props.get("тип")
            if not t:
                continue
            name = str(props.get("имя") or f[:-3])
            props["_файл"] = os.path.relpath(p, vault)
            if t == "диалог":
                props["_сырой"] = raw
            cards.setdefault(t, {})[name] = props
    return cards


class Vault:
    """Содержимое кампании из Obsidian; перечитывается, когда меняются файлы (ДАН-04)."""

    def __init__(self, cfg=None):
        self.cfg = cfg or config()
        self.stamp, self.data, self.errors = None, {"thr": {}, "table": {}, "ambush": {}}, []
        self.cards = {}

    def _stamp(self):
        v = self.cfg.get("хранилище", "")
        if not v or not os.path.isdir(v):
            return None
        st = []
        for dp, dn, fn in os.walk(v):
            dn[:] = [d for d in dn if not d.startswith(".")]
            st.extend((f, os.path.getmtime(os.path.join(dp, f))) for f in fn if f.endswith(".md"))
        return hash(tuple(sorted(st)))

    def get(self):
        s = self._stamp()
        if s is not None and s == self.stamp:
            return self.data, self.cards, self.errors
        self.stamp, errors = s, []
        vault = self.cfg.get("хранилище", "")
        if s is None:
            errors.append(f"хранилище не найдено: {vault or '(не задано в config/kampaniya.json)'}")
            self.data, self.cards, self.errors = {"thr": {}, "table": {}, "ambush": {}}, {}, errors
            return self.data, self.cards, self.errors
        act = os.path.join(vault, self.cfg.get("акт", ""))
        try:
            with open(act, encoding="utf-8") as f:
                self.data = parse_encounters(f.read(), errors, os.path.relpath(act, vault))
        except OSError:
            errors.append(f"файл акта не найден: {self.cfg.get('акт')}")
            self.data = {"thr": {}, "table": {}, "ambush": {}}
        self.cards = load_cards(vault, errors)
        self.errors = errors
        return self.data, self.cards, self.errors


# ---------------------------------------------------------------- разговоры: деревья диалогов (ТЗ §3.3, Д3)

# таблица реакций DMG: СЛ просьбы по отношению NPC и риску (None — не сделает ни при каком броске)
REACTION_DC = {"дружелюбный": {"нет": 0, "малый": 10, "большой": 20},
               "безразличный": {"нет": 10, "малый": 20, "большой": None},
               "враждебный": {"нет": 20, "малый": None, "большой": None}}
ATT_ORDER = ["враждебный", "безразличный", "дружелюбный"]
SKILLS = ["Атлетика", "Акробатика", "Ловкость рук", "Скрытность", "Магия", "История", "Анализ", "Природа", "Религия",
          "Уход за животными", "Проницательность", "Медицина", "Внимательность", "Выживание", "Обман", "Запугивание",
          "Выступление", "Убеждение", "СИЛ", "ЛОВ", "ТЕЛ", "ИНТ", "МДР", "ХАР"]
# состязания в разговоре (РАЗ-05): навык говорящего → навык NPC
CONTEST = {"Обман": "Проницательность", "Проницательность": "Обман", "Ловкость рук": "Внимательность", "Скрытность": "Внимательность"}
_MONEY = r"(\d+\s*(?:зм|см|мм)(?:\s+\d+\s*(?:зм|см|мм))*)"


def link_name(v):
    """«[[Ролан]]», «[[Ролан|лорд]]», «Ролан» → «Ролан»."""
    m = re.match(r"^\s*\[\[([^\]|#]+)", str(v or ""))
    return (m.group(1) if m else str(v or "")).strip()


def money_cp(t):
    tot = 0
    for n, u in re.findall(r"(\d+)\s*(зм|см|мм)", t):
        tot += int(n) * {"зм": 100, "см": 10, "мм": 1}[u]
    return tot


def parse_dialog(text, errors, src="диалог"):
    """Узлы дерева: {id: {"lines": [реплики], "opts": [варианты], "do": [действия-события], "shop", "end", "goto"}}.
    Вариант: {"text", "cond": [условия], "check": {"what","dc","hidden","risk"} | None, "ok", "fail"}."""
    props, body = frontmatter(text)
    nodes, cur = {}, None
    for i, ln in enumerate(body.split("\n"), 1):
        s = ln.strip()
        if not s:
            continue
        m = re.match(r"^##\s+(.+)$", s)
        if m:
            cur = m.group(1).strip()
            nodes[cur] = {"lines": [], "opts": [], "do": [], "shop": False, "end": False, "goto": "", "fight": ""}
            continue
        if cur is None:
            continue
        n = nodes[cur]
        if s.startswith("- "):
            o = {"text": "", "cond": [], "check": None, "ok": "", "fail": ""}
            rest = s[2:].strip()
            while rest.startswith("["):
                j = rest.find("]")
                if j < 0:
                    break
                tag, rest = rest[1:j].strip(), rest[j + 1:].strip()
                if tag.startswith("если:"):
                    o["cond"].append(tag[5:].strip())
                    continue
                parts = [p.strip() for p in tag.split(",")]
                m2 = re.match(r"^(.+?)(?:\s+(\d+))?$", parts[0])
                what = m2.group(1).strip()
                if what not in SKILLS:
                    errors.append(f"{src}, строка {i}: неизвестный навык «{what}»")
                    continue
                ck = {"what": what, "dc": int(m2.group(2)) if m2.group(2) else None, "hidden": False, "risk": "нет"}
                for p in parts[1:]:
                    if p == "скрыто":
                        ck["hidden"] = True
                    elif p.startswith("риск"):
                        ck["risk"] = p.split()[-1]
                o["check"] = ck
            if "→" in rest:
                rest, tgt = rest.split("→", 1)
                tgt = [t.strip() for t in tgt.split("|")]
                o["ok"] = tgt[0]
                o["fail"] = tgt[1] if len(tgt) > 1 else cur
            o["text"] = rest.strip()
            n["opts"].append(o)
        elif s.startswith("!"):
            m2 = re.match(r"^(\S+?)(?::\s*|\s+|$)(.*)$", s[1:].strip())   # «!дать: 100 зм отряду», «!отношение +1», «!конец»
            key, arg = m2.group(1).strip(), m2.group(2).strip()
            if key in ("дать", "взять"):
                sg = 1 if key == "дать" else -1
                ev = {"type": "gm"}
                mm = re.search(_MONEY, arg)
                if mm:
                    ev["purse"] = sg * money_cp(mm.group(1))
                    arg = (arg[:mm.start()] + arg[mm.end():]).strip()
                else:
                    mq = re.search(r"[×x]\s*(\d+)\s*$", arg)
                    ev["item"] = re.sub(r"[×x]\s*\d+\s*$", "", arg).strip()
                    ev["qty"] = sg * (int(mq.group(1)) if mq else 1)
                    arg = ""
                hero = arg.replace("отряду", "").replace("у отряда", "").strip()
                if hero:
                    ev["hero"] = hero
                n["do"].append(ev)
            elif key == "сведения":
                n["do"].append({"type": "learn", "fact": arg})
            elif key == "отношение":
                n["do"].append({"type": "attitude", "npc": "", "delta": int(arg or 0)})   # npc подставляется при разговоре
            elif key == "репутация":
                f, _, d = arg.rpartition(" ")
                n["do"].append({"type": "rep", "faction": f.strip(), "delta": int(d or 0)})
            elif key == "лавка":
                n["shop"] = True
            elif key == "конец":
                n["end"] = True
            elif key == "бой":
                n["fight"] = link_name(arg)
                n["do"].append({"type": "note", "text": "бой: " + (link_name(arg) or "противник")})
            else:
                errors.append(f"{src}, строка {i}: неизвестное действие «!{key}»")
        elif s.startswith("→"):
            n["goto"] = s[1:].strip()
        else:
            n["lines"].append(s)
    start = str(props.get("начало") or (next(iter(nodes)) if nodes else ""))
    for nid, n in nodes.items():   # ссылки на несуществующие узлы
        for o in n["opts"]:
            for t in (o["ok"], o["fail"]):
                if t and t not in nodes:
                    errors.append(f"{src}: узел «{nid}» ведёт в несуществующий «{t}»")
        if n["goto"] and n["goto"] not in nodes:
            errors.append(f"{src}: узел «{nid}» ведёт в несуществующий «{n['goto']}»")
    if start and start not in nodes:
        errors.append(f"{src}: начального узла «{start}» нет")
    return {"npc": link_name(props.get("npc")), "start": start, "nodes": nodes}


def cond_ok(c, state, hero, npc_card):
    """Условие варианта: «язык латынь», «отношение дружелюбный» (не ниже), «сведения X», «деньги 10 зм» (не меньше)."""
    k, _, v = c.partition(" ")
    v = v.strip()
    if k == "язык":
        return hero is not None and v in hero.get("langs", [])
    if k == "отношение":
        att = state["npcs"].get(npc_card.get("имя", ""), {}).get("attitude", npc_card.get("отношение", "безразличный"))
        return v in ATT_ORDER and ATT_ORDER.index(att) >= ATT_ORDER.index(v)
    if k == "сведения":
        return v in state["facts"]
    if k == "деньги":
        return state["purse"] >= money_cp(v)
    return False


class Talk:
    """Разговоры кампании: карточки NPC и деревья из хранилища; ход по дереву превращается в события журнала."""

    def __init__(self, cards, errors):
        self.npcs = cards.get("npc", {})
        self.trees = {}
        for name, d in cards.get("диалог", {}).items():
            t = parse_dialog(d.get("_сырой", ""), errors, d.get("_файл", name))
            self.trees[name] = t

    def tree_of(self, npc):
        card = self.npcs.get(npc, {})
        name = link_name(card.get("диалог"))
        if name in self.trees:
            return self.trees[name]
        for t in self.trees.values():
            if t["npc"] == npc:
                return t
        return None

    def node_do(self, npc, node):
        """Действия узла как события («!отношение» — к этому NPC); переход «→» без вариантов — сразу в следующий узел."""
        t = self.tree_of(npc)
        seen, do, shop, end = set(), [], False, False
        while t and node in t["nodes"] and node not in seen:
            seen.add(node)
            n = t["nodes"][node]
            for e in n["do"]:
                e = dict(e)
                if e["type"] == "attitude":
                    e["npc"] = npc
                do.append(e)
            shop |= n["shop"]
            if n["end"]:
                end = True
                break
            if n["goto"] and not n["opts"]:
                node = n["goto"]
                continue
            break
        return ("" if end else node), do, shop

    def start(self, npc):
        t = self.tree_of(npc)
        if t is None:
            return None
        node, do, shop = self.node_do(npc, t["start"])
        card = self.npcs.get(npc, {})
        return {"type": "dialog", "npc": npc, "node": node, "do": do, "shop": shop,
                "attitude": card.get("отношение", "безразличный")}

    def view(self, state, hero=None):
        """Текущий узел для экрана: реплики, варианты с пометкой доступности и проверки."""
        d = state.get("dialog", {})
        npc = d.get("npc")
        if not npc:
            return None
        t, card = self.tree_of(npc), self.npcs.get(npc, {})
        n = (t or {}).get("nodes", {}).get(d.get("node"), {})
        att = state["npcs"].get(npc, {}).get("attitude", card.get("отношение", "безразличный"))
        opts = []
        for i, o in enumerate(n.get("opts", [])):
            ok = all(cond_ok(c, state, hero, card) for c in o["cond"])
            ck = o["check"]
            info = ""
            if ck:
                dc = self._dc(ck, att)
                info = ck["what"] + (f" против {CONTEST[ck['what']]} NPC" if ck["what"] in CONTEST else (f", СЛ {dc}" if dc is not None else ", невозможно"))
                info += ", скрыто" if ck["hidden"] else ""
            opts.append({"i": i, "text": o["text"], "ok": ok, "check": info, "cond": o["cond"]})
        langs = card.get("языки") or []
        return {"npc": npc, "node": d.get("node"), "lines": n.get("lines", []), "opts": opts, "attitude": att,
                "shop": d.get("shop") and card.get("лавка") or [], "langs": langs, "card": {k: v for k, v in card.items() if not k.startswith("_")}}

    @staticmethod
    def _dc(ck, att):
        if ck["dc"] is not None:
            return ck["dc"]
        return REACTION_DC.get(att, REACTION_DC["безразличный"]).get(ck["risk"], 10)

    def say(self, state, i, hero, hero_info=None):
        """Выбор варианта i говорящим героем → событие (dialog без броска или check/contest с переходом)."""
        d = state.get("dialog", {})
        npc = d.get("npc")
        t, card = self.tree_of(npc), self.npcs.get(npc, {})
        if not t or d.get("node") not in t["nodes"]:
            return None, "разговора нет"
        opts = t["nodes"][d["node"]]["opts"]
        if not 0 <= i < len(opts):
            return None, "нет такого варианта"
        o = opts[i]
        if not all(cond_ok(c, state, hero_info, card) for c in o["cond"]):
            return None, "этот вариант сейчас недоступен"
        ck = o["check"]
        if not ck:
            node, do, shop = self.node_do(npc, o["ok"] or d["node"])
            return {"type": "dialog", "node": node, "do": do, "shop": shop, "said": o["text"], "hero": hero}, None
        att = state["npcs"].get(npc, {}).get("attitude", card.get("отношение", "безразличный"))
        okN, okDo, okShop = self.node_do(npc, o["ok"] or d["node"])
        fN, fDo, fShop = self.node_do(npc, o["fail"] or d["node"])
        dlg = {"npc": npc, "ok": okN, "okDo": okDo, "okShop": okShop, "fail": fN, "failDo": fDo, "failShop": fShop}
        # нет общего языка — объясняются жестами: помеха (Д4); языки NPC не указаны — считаем, что понимают
        langs = card.get("языки") or []
        dis = bool(langs and hero_info is not None and not set(langs) & set(hero_info.get("langs", [])))
        shift = state["npcs"].get(npc, {}).get("shift", 0)
        if ck["what"] in CONTEST:
            vs = CONTEST[ck["what"]]
            return {"type": "contest", "hero": hero, "what": ck["what"], "vs": npc, "vsWhat": vs, "dis": dis,
                    "vsMod": int((card.get("навыки") or {}).get(vs, 0)), "hidden": ck["hidden"], "said": o["text"], "dlg": dlg}, None
        dc = self._dc(ck, att)
        if dc is None:   # просьба не по отношению — NPC отказывает без броска
            node, do, shop = self.node_do(npc, o["fail"] or d["node"])
            return {"type": "dialog", "node": node, "do": do, "shop": shop, "said": o["text"], "hero": hero,
                    "text": "не станет: отношение не позволяет"}, None
        return {"type": "check", "hero": hero, "what": ck["what"], "dc": dc, "mod": shift, "dis": dis, "hidden": ck["hidden"],
                "said": o["text"], "dlg": dlg}, None


# ---------------------------------------------------------------- журнал и программа правил

class Engine:
    """build/kampaniya: строка JSON туда — строка JSON обратно."""

    def __init__(self):
        exe = os.path.join(ROOT, "build", "kampaniya")
        self.p = subprocess.Popen([exe], stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True, encoding="utf-8", bufsize=1)
        self.lock = threading.Lock()

    def talk(self, msg):
        with self.lock:
            self.p.stdin.write(json.dumps(msg, ensure_ascii=False) + "\n")
            self.p.stdin.flush()
            return json.loads(self.p.stdout.readline())

    def close(self):
        try:
            self.talk({"cmd": "quit"})
        except (OSError, ValueError):
            pass
        self.p.wait(timeout=5)


WORLDS = os.path.join(STATE, "miry")
WORLD_KEYS = {"size": (10, 5000, 800), "seed": (1, 10 ** 9, 1), "sea": (0, 0.9, 0.3), "mount": (0, 1, 0.5), "rough": (0, 1, 0.5),
              "climate": (-1, 1, 0), "humid": (-1, 1, 0), "towns": (0.05, 10, 1), "rivers": (0.05, 3, 1)}


def world_params(raw):
    """Параметры генератора из запроса: только известные, в своих пределах."""
    out = {}
    for k, (lo, hi, d) in WORLD_KEYS.items():
        try:
            v = float((raw or {}).get(k, d))
        except (TypeError, ValueError):
            v = d
        out[k] = int(min(hi, max(lo, v))) if k == "seed" else min(hi, max(lo, v))
    hm = re.sub(r"[^0-9A-Za-z_-]", "", str((raw or {}).get("hmap") or ""))[:40]
    if hm:   # карта высот: метка загрузки (или «height.bin» у сохранённого мира), размер в пикселях, км на пиксель, порог моря
        try:
            out.update(hmap=hm, hw=int(raw["hw"]), hh=int(raw["hh"]), kmpp=min(5000.0, max(0.05, float(raw.get("kmpp", 1)))),
                       hmSea=min(1.0, max(0.0, float(raw.get("hmSea", 0.3)))))
        except (KeyError, TypeError, ValueError):
            out.pop("hmap", None)
    return out


HMAP_MAX = 1024   # сторона карты высот, пикселей: страница уменьшает картинку до неё (запрос — не больше 2 МБ)


def hmap_save(token_owner, data_b64, w, h):
    """Загруженная карта высот (серое w × h байт, base64) — во временный файл; возвращает метку."""
    import base64
    w, h = int(w), int(h)
    if not (2 <= w <= HMAP_MAX and 2 <= h <= HMAP_MAX):
        raise ValueError(f"карта высот — до {HMAP_MAX} × {HMAP_MAX}")
    raw = base64.b64decode(data_b64 or "")
    if len(raw) != w * h:
        raise ValueError("размер данных не совпадает с картинкой")
    d = os.path.join(WORLDS, "_upload")
    os.makedirs(d, exist_ok=True)
    tok = "u" + re.sub(r"[^0-9A-Za-z]", "", token_owner)[:12] + "".join(random.choice("abcdefghjkmnpqrstuvwxyz23456789") for _ in range(6))
    with open(os.path.join(d, tok + ".bin"), "wb") as f:
        f.write(raw)
    return tok


def hmap_path(params, wid=None):
    """Путь к файлу карты высот для движка: у сохранённого мира — его height.bin, иначе — загрузка по метке."""
    hm = params.get("hmap")
    if not hm:
        return None
    return os.path.join(WORLDS, wid, "height.bin") if wid and hm == "height.bin" else os.path.join(WORLDS, "_upload", hm + ".bin")


def load_world(wid):
    try:
        with open(os.path.join(WORLDS, re.sub(r"[^0-9A-Za-z_-]", "", str(wid)), "world.json"), encoding="utf-8") as f:
            return json.load(f)
    except (OSError, ValueError):
        return None


def save_world(name, params, author):
    os.makedirs(WORLDS, exist_ok=True)
    wid = ""
    while not wid or os.path.exists(os.path.join(WORLDS, wid)):
        wid = "w" + "".join(random.choice("abcdefghjkmnpqrstuvwxyz23456789") for _ in range(6))
    os.makedirs(os.path.join(WORLDS, wid))
    if params.get("hmap"):   # карта высот переезжает к миру
        import shutil
        shutil.copyfile(hmap_path(params), os.path.join(WORLDS, wid, "height.bin"))
        params = dict(params, hmap="height.bin")
    with open(os.path.join(WORLDS, wid, "world.json"), "w", encoding="utf-8") as f:
        json.dump({"имя": name, "тип": "генератор", "параметры": params, "автор": author}, f, ensure_ascii=False)
    return wid


class Campaign:
    """Кампания: meta.json (имя, зерно, мастер) + journal.jsonl. Состояние = свёртка журнала (НФ-01)."""

    def __init__(self, cid, vault=None, root=STATE):
        self.cid, self.dir = cid, os.path.join(root, cid)
        self.vault = vault or Vault()
        with open(os.path.join(self.dir, "meta.json"), encoding="utf-8") as f:
            self.meta = json.load(f)
        self.journal = []
        jp = os.path.join(self.dir, "journal.jsonl")
        if os.path.exists(jp):
            with open(jp, encoding="utf-8") as f:
                self.journal = [json.loads(ln) for ln in f if ln.strip()]
        self.eng = Engine()
        self.drafts = {}   # номер черновика → {"act", "who", "attempt", "ev"}
        self.dn = 0
        self.state = self._load()

    @staticmethod
    def create(cid, name, seed, master, root=STATE, vault=None, master_name=None):
        d = os.path.join(root, cid)
        os.makedirs(d, exist_ok=False)
        with open(os.path.join(d, "meta.json"), "w", encoding="utf-8") as f:
            json.dump({"имя": name, "seed": seed, "мастер": master, "мастер_имя": master_name or master, "игроки": {}}, f, ensure_ascii=False)
        return Campaign(cid, vault, root)

    def join(self, key, shown):
        """Игрок вошёл по коду (Р25): кампания попадает в его «Мои кампании»."""
        if key == self.meta.get("мастер") or key in self.meta.setdefault("игроки", {}):
            return
        self.meta["игроки"][key] = shown
        with open(os.path.join(self.dir, "meta.json"), "w", encoding="utf-8") as f:
            json.dump(self.meta, f, ensure_ascii=False)

    def _content(self):
        data, cards, errors = self.vault.get()
        self.eng.talk({"cmd": "content", "data": data})
        return errors

    # ---- мир кампании (ТЗ v2 §3): «акт1» — сетка Акта I из config «регион»; иначе — мир генератора из state/kampaniya/miry ----
    def region(self):
        """Сетка Акта I (web/data/region_*.json из config «регион») или None."""
        rp = self.vault.cfg.get("регион")
        if not rp:
            return None
        p = rp if os.path.isabs(rp) else os.path.join(ROOT, rp)
        if getattr(self, "_region_path", None) != p:
            try:
                with open(p, encoding="utf-8") as f:
                    self._region = json.load(f)
            except (OSError, ValueError):
                self._region = None
            self._region_path = p
        return self._region

    def world_msg(self, wid, use):
        """Команда движку для мира wid ("акт1" или id мира генератора)."""
        if wid == "акт1":
            reg = self.region()
            return None if reg is None else {"cmd": "world", "id": "акт1", "kind": "act", "data": reg, "use": use}
        w = load_world(wid)
        if w is None:
            return None
        prm = dict(w["параметры"], name=w["имя"])
        if prm.get("hmap"):
            prm["hmap"] = hmap_path(prm, wid)
        return {"cmd": "world", "id": wid, "kind": "gen", "params": prm, "use": use}

    def set_world(self, wid):
        """Сделать мир миром кампании: сразу в движок и в meta.json (переживает перезапуск)."""
        m = self.world_msg(wid, True)
        if m is None:
            return {"err": "нет такого мира"}
        r = self.eng.talk(m)
        if r.get("type") != "world":
            return {"err": r.get("err")}
        self.meta["мир"] = wid
        with open(os.path.join(self.dir, "meta.json"), "w", encoding="utf-8") as f:
            json.dump(self.meta, f, ensure_ascii=False)
        self.tiles, self.info = {}, r["info"]
        return {"info": r["info"]}

    def tile(self, wid, z, tx, ty):
        """Кусок обзора — из кэша (последние 300) или от движка."""
        key = (wid, z, tx, ty)
        t = self.tiles.get(key)
        if t is None:
            r = self.eng.talk({"cmd": "tile", "id": wid, "z": z, "tx": tx, "ty": ty})
            if r.get("type") != "tile":
                return None
            t = self.tiles[key] = r["tile"]
            if len(self.tiles) > 300:
                self.tiles.pop(next(iter(self.tiles)))
        return t

    def _load(self):
        self._content()
        self.tiles, self.info = {}, None
        wid = self.meta.get("мир") or ("акт1" if self.region() else None)
        m = self.world_msg(wid, True) if wid else None
        if m:
            r = self.eng.talk(m)
            if r.get("type") != "world":
                raise RuntimeError(r.get("err"))
            self.info = r["info"]
            self.meta.setdefault("мир", wid)
        r = self.eng.talk({"cmd": "load", "seed": self.meta["seed"], "journal": self.journal})
        if r.get("type") != "state":
            raise RuntimeError(f"журнал кампании {self.cid} не сворачивается: {r.get('err')}")
        return r["state"]

    def talk(self):
        """Разговоры по текущему содержимому хранилища (пересобираются, когда меняются файлы)."""
        data, cards, errors = self.vault.get()
        if getattr(self, "_talk_stamp", None) != self.vault.stamp or not hasattr(self, "_talk"):
            self.talk_errors = []
            self._talk, self._talk_stamp = Talk(cards, self.talk_errors), self.vault.stamp
        return self._talk

    def hero(self, name):
        return next((h for h in self.state["heroes"] if h["name"] == name), None)

    def propose(self, act, who="мастер"):
        """Черновик события (МАС-03): состояние не меняется. Возвращает {"id", "ev"} или {"err"}."""
        self._content()   # правки Obsidian — к следующему броску (ДАН-04)
        r = self.eng.talk({"cmd": "propose", "act": act, "who": who, "attempt": 0})
        if r.get("type") != "draft":
            return {"err": r.get("err")}
        self.dn += 1
        self.drafts[self.dn] = {"act": act, "who": who, "attempt": 0, "ev": r["ev"]}
        return {"id": self.dn, "ev": r["ev"]}

    def reroll(self, did):
        d = self.drafts.get(did)
        if d is None:
            return {"err": "нет такого черновика"}
        d["attempt"] += 1
        r = self.eng.talk({"cmd": "propose", "act": d["act"], "who": d["who"], "attempt": d["attempt"]})
        if r.get("type") != "draft":
            return {"err": r.get("err")}
        d["ev"] = r["ev"]
        return {"id": did, "ev": r["ev"]}

    def accept(self, did, edit=None):
        """Принять черновик; edit — правка мастера: поля, которые заменяют итог ("res") события."""
        d = self.drafts.get(did)
        if d is None:
            return {"err": "нет такого черновика"}
        ev = json.loads(json.dumps(d["ev"]))
        if ev["n"] != len(self.journal) + 1:
            return {"err": "черновик устарел: журнал ушёл вперёд, сделайте новый"}
        if edit:
            ev.setdefault("res", {}).update(edit)
            ev["edit"] = True
        return self._commit(ev, did)

    def cancel(self, did):
        self.drafts.pop(did, None)
        return {"ok": True}

    def direct(self, act, who="мастер"):
        """Прямое действие мастера (МАС-04) — сразу в журнал, без черновика."""
        r = self.propose(act, who)
        return r if "err" in r else self.accept(r["id"])

    def _commit(self, ev, did=None):
        r = self.eng.talk({"cmd": "apply", "ev": ev})
        if r.get("type") != "state":
            return {"err": r.get("err")}
        with open(os.path.join(self.dir, "journal.jsonl"), "a", encoding="utf-8") as f:   # сразу на диск (НФ-05)
            f.write(json.dumps(ev, ensure_ascii=False) + "\n")
        self.journal.append(ev)
        self.state = r["state"]
        if did is not None:
            self.drafts.pop(did, None)
        self.drafts = {k: v for k, v in self.drafts.items() if v["ev"]["n"] > len(self.journal)}   # остальные устарели
        return {"state": self.state, "ev": ev}

    def undo(self):
        """Отмена последнего события (МАС-06): журнал без него, свёртка заново."""
        if not self.journal:
            return {"err": "журнал пуст"}
        last = self.journal.pop()
        with open(os.path.join(self.dir, "journal.jsonl"), "w", encoding="utf-8") as f:
            f.writelines(json.dumps(e, ensure_ascii=False) + "\n" for e in self.journal)
        with open(os.path.join(self.dir, "undone.jsonl"), "a", encoding="utf-8") as f:   # отменённое не теряется
            f.write(json.dumps(last, ensure_ascii=False) + "\n")
        self.drafts.clear()
        self.state = self._load()
        return {"state": self.state, "undone": last}

    def view(self, player=None):
        """Что видит мастер (player=None) или игрок: у скрытых событий игроку — только «мастер бросил» (МАС-07)."""
        evs = []
        for e in self.journal[-100:]:
            if player and e.get("hidden"):
                evs.append({"n": e["n"], "type": e["type"], "hidden": True, "text": "мастер бросил"})
            else:
                evs.append(e)
        return {"state": self.state, "journal": evs}

    def export_md(self):
        """Журнал сессии в markdown (МАС-08) — текстом; куда сохранить, решает мастер."""
        out = [f"# {self.meta.get('имя', self.cid)} — журнал", ""]
        for e in self.journal:
            line = f"- **{e['n']}. {e['type']}**"
            for c in e.get("checks", []):
                line += f" · {c['who']}: {c['what']} {c['total']}" + (f" против {c['dc']}" if c.get("dc") else "")
            res = e.get("res", {})
            if e["type"] == "day":
                enc = res.get("enc", {})
                line += f" · {res.get('km')} км, {res.get('terrain')}"
                if enc.get("hit"):
                    line += f" · {'засада' if enc.get('ambush') else 'встреча'}: {enc.get('text')}"
            elif e["type"] in ("note", "gm") and e.get("text"):
                line += f" · {e['text']}"
            if e.get("edit"):
                line += " · (правка мастера)"
            out.append(line)
        return "\n".join(out) + "\n"

    def close(self):
        self.eng.close()


# ---------------------------------------------------------------- команды сервера (play_server.py: cmd «kmp_*»)

CAMPS, CAMPS_LOCK, VAULT = {}, threading.Lock(), None
MASTER_ONLY = {"kmp_hmap", "kmp_world_use", "kmp_world_preview", "kmp_accept", "kmp_reroll", "kmp_cancel", "kmp_direct", "kmp_undo", "kmp_export", "kmp_talk", "kmp_talk_end"}


def _key(name):
    return " ".join(str(name or "").lower().split())[:40]


def _camp(cid):
    global VAULT
    cid = re.sub(r"[^0-9A-Za-z_-]", "", str(cid or ""))[:32]
    if not cid or not os.path.exists(os.path.join(STATE, cid, "meta.json")):
        raise KeyError("нет такой кампании")
    if VAULT is None:
        VAULT = Vault()
    if cid not in CAMPS:
        CAMPS[cid] = Campaign(cid, VAULT, root=STATE)
    return CAMPS[cid]


def _draft_view(d):
    return {"id": d[0], "who": d[1]["who"], "ev": d[1]["ev"]}


def kmp_cmd(msg, sid=""):
    """Мастер — тот, кто создал кампанию (по имени, как хозяин комнаты). Игрок только предлагает: его запрос — черновик
    в очереди мастера (Р1); принимает, перебрасывает, правит и отменяет — мастер."""
    global VAULT
    cmd, name = msg.get("cmd"), _key(msg.get("name"))
    if not name:
        return {"type": "error", "err": "введите имя"}
    with CAMPS_LOCK:
        try:
            if cmd == "kmp_list":   # «Мои кампании»: где я мастер или вошёл по коду
                out = []
                if os.path.isdir(STATE):
                    for cid in os.listdir(STATE):
                        try:
                            with open(os.path.join(STATE, cid, "meta.json"), encoding="utf-8") as f:
                                m = json.load(f)
                        except (OSError, ValueError):
                            continue
                        if m.get("мастер") != name and name not in m.get("игроки", {}):
                            continue
                        jp = os.path.join(STATE, cid, "journal.jsonl")
                        heroes = 0
                        if os.path.exists(jp):
                            with open(jp, encoding="utf-8") as f:
                                heroes = sum(1 for ln in f if '"type": "hero"' in ln)
                        out.append({"id": cid, "title": m.get("имя", cid), "master": m.get("мастер") == name,
                                    "masterName": m.get("мастер_имя", m.get("мастер", "")), "players": len(m.get("игроки", {})),
                                    "heroes": heroes, "t": int(os.path.getmtime(jp if os.path.exists(jp) else os.path.join(STATE, cid, "meta.json")))})
                out.sort(key=lambda x: -x["t"])
                return {"type": "kmp_list", "list": out}
            if cmd == "kmp_create":   # код кампании — как у комнаты: 5 знаков, без похожих букв и цифр
                title = str(msg.get("title") or "Кампания")[:80]
                os.makedirs(STATE, exist_ok=True)
                cid = ""
                while not cid or os.path.exists(os.path.join(STATE, cid)):
                    cid = "".join(random.choice("ABCDEFGHJKLMNPQRSTUVWXYZ23456789") for _ in range(5))
                if VAULT is None:
                    VAULT = Vault()
                CAMPS[cid] = Campaign.create(cid, title, int(msg.get("seed") or random.randint(1, 99999)), name, root=STATE, vault=VAULT,
                                             master_name=str(msg.get("name"))[:40])
                c, cmd = CAMPS[cid], "kmp_open"
            else:
                c = _camp(str(msg.get("id", "")).strip().upper() if len(str(msg.get("id", "")).strip()) == 5 else msg.get("id"))
            master = c.meta.get("мастер") == name
            if not master:
                c.join(name, str(msg.get("name"))[:40])   # кто знает код — участник (как в комнате)
            if cmd in MASTER_ONLY and not master:
                return {"type": "error", "err": "это может только мастер"}
            r = {}
            if cmd == "kmp_worldinfo":   # мир кампании: размер, уровни обзора, поселения (один раз на страницу)
                return {"type": "kmp_worldinfo", "wid": c.meta.get("мир"), "info": c.info} if c.info else {"type": "error", "err": "мира нет"}
            if cmd == "kmp_tile":   # кусок обзора: мир кампании, а мастеру — и мир предпросмотра
                wid = str(msg.get("wid") or c.meta.get("мир") or "")
                if wid != c.meta.get("мир") and not (master and wid == "предпросмотр"):
                    return {"type": "error", "err": "чужой мир"}
                t = c.tile(wid, int(msg.get("z", 0)), int(msg.get("tx", 0)), int(msg.get("ty", 0)))
                return {"type": "kmp_tile", "tile": t} if t else {"type": "error", "err": "нет куска"}
            if cmd == "kmp_path":   # предпросмотр пути (ТЗ v2 §4): тот же расчёт, что у события «маршрут», без записи в журнал
                r = c.eng.talk({"cmd": "propose", "act": {"type": "route", "way": msg.get("way") or [], "mode": str(msg.get("mode") or "")},
                                "who": name, "attempt": 0})
                return {"type": "kmp_path", "res": r["ev"]["res"]} if r.get("type") == "draft" else {"type": "error", "err": r.get("err")}
            if cmd == "kmp_hmap":   # мастер загружает карту высот (страница уже перевела её в серое и уменьшила)
                if not master:
                    return {"type": "error", "err": "мир выбирает мастер"}
                return {"type": "kmp_hmap", "token": hmap_save(c.cid, msg.get("data"), msg.get("w", 0), msg.get("h", 0)), "w": int(msg["w"]), "h": int(msg["h"])}
            if cmd == "kmp_world_preview":   # мастер: мир по параметрам — посмотреть, не меняя мира кампании
                if not master:
                    return {"type": "error", "err": "мир выбирает мастер"}
                prm = world_params(msg.get("params"))
                if prm.get("hmap"):
                    prm["hmap"] = hmap_path(prm)
                    if not os.path.exists(prm["hmap"]):
                        return {"type": "error", "err": "карта высот не найдена — загрузите её снова"}
                r = c.eng.talk({"cmd": "world", "id": "предпросмотр", "kind": "gen", "params": prm, "use": False})
                c.tiles = {k: v for k, v in c.tiles.items() if k[0] != "предпросмотр"}
                return {"type": "kmp_preview", "info": r.get("info")} if r.get("type") == "world" else {"type": "error", "err": r.get("err")}
            if cmd == "kmp_world_use":   # мастер: взять мир (новый по параметрам или «акт1»)
                if not master:
                    return {"type": "error", "err": "мир выбирает мастер"}
                wid = "акт1" if msg.get("act1") else save_world(str(msg.get("title") or "Мир")[:60], world_params(msg.get("params")), name)
                r = c.set_world(wid)
                if r.get("err"):
                    return {"type": "error", "err": r["err"]}
                r = c.direct({"type": "note", "text": "мир кампании: " + (r["info"].get("name") or wid)})
                cmd = "kmp_open"   # дальше — как открытие: ответ с журналом и состоянием
            if cmd == "kmp_propose":
                act = msg.get("act") or {}
                if not master and act.get("type") not in ("hero", "check", "contest", "buy", "sell", "note", "route"):
                    return {"type": "error", "err": "игрок может предложить: героя, проверку, сделку, маршрут, заметку"}
                r = c.propose(act, "мастер" if master else str(msg.get("name"))[:40])
            elif cmd == "kmp_accept":
                r = c.accept(int(msg.get("draft", 0)), msg.get("edit") or None)
            elif cmd == "kmp_reroll":
                r = c.reroll(int(msg.get("draft", 0)))
            elif cmd == "kmp_cancel":
                r = c.cancel(int(msg.get("draft", 0)))
            elif cmd == "kmp_direct":
                r = c.direct(msg.get("act") or {})
            elif cmd == "kmp_talk":   # мастер начинает разговор с NPC по его дереву
                act = c.talk().start(str(msg.get("npc", "")))
                r = {"err": "у NPC нет дерева диалога"} if act is None else c.direct(act)
            elif cmd == "kmp_talk_end":
                r = c.direct({"type": "dialog", "node": ""})
            elif cmd == "kmp_say":   # вариант разговора: игрок — черновик мастеру, мастер — тоже черновик (видит бросок)
                hero = str(msg.get("hero", ""))
                act, e = c.talk().say(c.state, int(msg.get("i", -1)), hero, c.hero(hero))
                if e:
                    return {"type": "error", "err": e}
                if act["type"] == "dialog" and master:
                    r = c.direct(act)
                else:
                    r = c.propose(act, "мастер" if master else str(msg.get("name"))[:40])
            elif cmd == "kmp_undo":
                r = c.undo()
            elif cmd == "kmp_export":
                return {"type": "kmp_export", "md": c.export_md(), "title": c.meta.get("имя", c.cid)}
            elif cmd != "kmp_open":
                return {"type": "error", "err": "неизвестная команда кампании"}
            if r.get("err"):
                return {"type": "error", "err": r["err"]}
            v = c.view(None if master else name)
            tk = c.talk()
            v["talk"] = tk.view(c.state, c.hero(str(msg.get("hero", ""))))
            v.update({"type": "kmp", "id": c.cid, "title": c.meta.get("имя", c.cid), "master": master,
                      "masterName": c.meta.get("мастер_имя", c.meta.get("мастер", "")), "players": list(c.meta.get("игроки", {}).values()),
                      "drafts": [_draft_view(d) for d in sorted(c.drafts.items())
                                 if master or _key(d[1]["who"]) == name]})
            if master:
                v["errors"] = c.vault.get()[2] + c.talk_errors
                v["npcCards"] = [{"name": n, "dialog": tk.tree_of(n) is not None, "attitude": d.get("отношение", "безразличный"),
                                  "langs": d.get("языки") or [], "knows": d.get("знает") or []} for n, d in sorted(tk.npcs.items())]
                data = c.vault.get()[0]
                v["terrains"] = TERRAINS
                v["tables"] = {t: len(data["table"].get(t, [])) for t in TERRAINS}
            return v
        except KeyError as e:
            return {"type": "error", "err": str(e).strip("'")}
        except (OSError, RuntimeError, ValueError) as e:
            return {"type": "error", "err": "кампания: " + str(e)}


# ---------------------------------------------------------------- проверка

def _check():
    import tempfile
    ok = True

    def t(name, cond):
        nonlocal ok
        print(("PASS" if cond else "FAIL"), name)
        ok &= bool(cond)

    v = Vault()
    data, cards, errors = v.get()
    t("хранилище: таблицы Акта I разобраны без ошибок " + ("; ".join(errors[:3]) if errors else ""), not errors)
    t("пороги 7 местностей, болото 13, город 16", len(data["thr"]) == 7 and data["thr"].get("Болото") == 13 and data["thr"].get("Город") == 16)
    t("таблицы к12 — 7 местностей по 12 строк", len(data["table"]) == 7 and all(len(x) == 12 and all(x) for x in data["table"].values()))
    t("засады: холмы и горы — одна строка", data["ambush"].get("Горы") == data["ambush"].get("Холмы и плато") and "Кабан" in data["ambush"].get("Горы", ""))
    p, _ = frontmatter("---\nтип: npc\nимя: Ролан\nуровень: 5  # лорд\nязыки: [старофранцузский, латынь]\n"
                       "навыки: {Проницательность: 3, Убеждение: 5}\nзнает:\n  - болото\n---\nтекст")
    t("frontmatter: числа, списки, словари, пункты", p == {"тип": "npc", "имя": "Ролан", "уровень": 5, "языки": ["старофранцузский", "латынь"],
                                                         "навыки": {"Проницательность": 3, "Убеждение": 5}, "знает": ["болото"]})

    with open(os.path.join(ROOT, "demo", "rycar.json"), encoding="utf-8") as f:
        rycar = json.load(f)
    with tempfile.TemporaryDirectory() as tmp:
        c = Campaign.create("t1", "проверка", 42, "мастер", root=tmp, vault=v)
        c.direct({"type": "setup", "purse": "100 зм", "food": 20, "water": 20, "place": "Париж"})
        c.direct({"type": "hero", "name": "Рыцарь", "char": rycar})
        d = c.propose({"type": "day", "terrain": "Лес", "roles": {"Рыцарь": "проводник"}})
        r1 = c.reroll(d["id"])
        t("переброс даёт другие кости", r1["ev"] != d["ev"])
        c.accept(d["id"], edit={"km": 20})
        t("правка мастера записана", c.journal[-1]["res"]["km"] == 20 and c.journal[-1]["edit"] and c.state["km"] == 20)
        st = json.dumps(c.state, sort_keys=True)
        c.close()
        c2 = Campaign("t1", v, root=tmp)
        t("журнал переживает перезапуск, свёртка та же", json.dumps(c2.state, sort_keys=True) == st)
        c2.direct({"type": "note", "text": "лишнее"})
        u = c2.undo()
        t("отмена последнего события", json.dumps(u["state"], sort_keys=True) == st and u["undone"]["type"] == "note")
        c2.direct({"type": "check", "hero": "Рыцарь", "what": "Проницательность", "dc": 15, "hidden": True})
        t("скрытый бросок: игрок видит только «мастер бросил»", c2.view("игрок")["journal"][-1].get("text") == "мастер бросил"
          and "checks" in c2.view()["journal"][-1])
        t("экспорт журнала в markdown", "Лес" in c2.export_md())
        stale = c2.propose({"type": "note", "text": "a"})
        c2.direct({"type": "note", "text": "b"})
        t("устаревший черновик не принимается", "err" in c2.accept(stale["id"]))
        c2.close()

    # разговоры: примеры карточек docs/primery-kartochek во временном хранилище
    import shutil
    with tempfile.TemporaryDirectory() as tmp:
        vd = os.path.join(tmp, "vault")
        shutil.copytree(os.path.join(ROOT, "docs", "primery-kartochek"), vd)
        tv = Vault({"хранилище": vd, "акт": "нет.md"})
        _, cards, _ = tv.get()
        errs = []
        tk = Talk(cards, errs)
        t("примеры: деревья разобраны без ошибок " + "; ".join(errs[:3]), not errs and len(tk.trees) == 2 and len(tk.npcs) == 2)
        tr = tk.tree_of("Ролан")
        o = tr["nodes"]["приветствие"]["opts"]
        t("вариант: проверка, риск, условие, переходы", o[4]["check"] == {"what": "Убеждение", "dc": None, "hidden": False, "risk": "малый"}
          and o[4]["ok"] == "настойчивость" and o[4]["fail"] == "отказ" and o[3]["cond"] == ["язык латынь"])
        t("лавка Лис из свойств", tk.npcs["Лисы Лангедока"]["лавка"][0]["вещь"] == "подсказка о верхнем кристалле")
        c = Campaign.create("t2", "разговор", 5, "мастер", root=tmp, vault=tv)
        c.direct({"type": "hero", "name": "Рыцарь", "char": rycar})
        c.direct(tk.start("Ролан"))
        v = tk.view(c.state, c.hero("Рыцарь"))
        t("начало разговора: узел и отношение с карточки", v["node"] == "приветствие" and v["attitude"] == "безразличный" and not v["opts"][3]["ok"])
        act, e = tk.say(c.state, 4, "Рыцарь", c.hero("Рыцарь"))
        t("просьба с малым риском у безразличного — СЛ 20 (DMG)", act["type"] == "check" and act["dc"] == 20 and act["dlg"]["fail"] == "приветствие"
          and act["dlg"]["failDo"] == [{"type": "attitude", "npc": "Ролан", "delta": -1}])
        r = c.propose(act)
        c.accept(r["id"])
        won = c.journal[-1]["res"]["ok"]
        t("проверка двигает разговор", c.state["dialog"]["node"] == "приветствие"
          and c.state["npcs"]["Ролан"]["shift"] == (1 if won else -1))
        act, e = tk.say(c.state, 5, "Рыцарь", c.hero("Рыцарь"))
        c.direct(act)
        t("переход по цепочке: сведения, аванс 100 зм, конец разговора", c.state["purse"] == 10000 and "болотные признаки у Каркассона" in c.state["facts"]
          and c.state["dialog"]["npc"] == "")
        c.direct(tk.start("Лисы Лангедока"))
        act, e = tk.say(c.state, 1, "Рыцарь", c.hero("Рыцарь"))
        t("Проницательность в разговоре — состязание с Обманом NPC", act["type"] == "contest" and act["vsWhat"] == "Обман" and act["vsMod"] == 5)
        act, e = tk.say(c.state, 0, "Рыцарь", c.hero("Рыцарь"))
        c.direct(act)
        v = tk.view(c.state, c.hero("Рыцарь"))
        t("узел с лавкой показывает товары", v["shop"] and v["shop"][0]["цена"] == "20 зм" and v["opts"][0]["ok"])
        act, e = tk.say(c.state, 0, "Рыцарь", c.hero("Рыцарь"))
        c.direct(act)
        t("покупка подсказки за 20 зм", c.state["purse"] == 8000 and "где верхний кристалл" in c.state["facts"])
        c.close()
    return ok


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "check":
        sys.exit(0 if _check() else 1)
    print(__doc__)
