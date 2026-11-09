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
import re
import subprocess
import sys
import threading

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
                    props, body = frontmatter(fh.read())
            except (OSError, UnicodeDecodeError) as e:
                errors.append(f"{os.path.relpath(p, vault)}: не читается ({e})")
                continue
            t = props.get("тип")
            if not t:
                continue
            name = str(props.get("имя") or f[:-3])
            props["_файл"] = os.path.relpath(p, vault)
            if t == "диалог":
                props["_текст"] = body
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
    def create(cid, name, seed, master, root=STATE, vault=None):
        d = os.path.join(root, cid)
        os.makedirs(d, exist_ok=False)
        with open(os.path.join(d, "meta.json"), "w", encoding="utf-8") as f:
            json.dump({"имя": name, "seed": seed, "мастер": master}, f, ensure_ascii=False)
        return Campaign(cid, vault, root)

    def _content(self):
        data, cards, errors = self.vault.get()
        self.eng.talk({"cmd": "content", "data": data})
        return errors

    def _load(self):
        self._content()
        r = self.eng.talk({"cmd": "load", "seed": self.meta["seed"], "journal": self.journal})
        if r.get("type") != "state":
            raise RuntimeError(f"журнал кампании {self.cid} не сворачивается: {r.get('err')}")
        return r["state"]

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
    return ok


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "check":
        sys.exit(0 if _check() else 1)
    print(__doc__)
