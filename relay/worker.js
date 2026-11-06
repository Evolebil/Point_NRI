// Посредник «Арены Боссов» на Cloudflare Workers (v21): постоянная ссылка для игроков, игра работает на компьютере хозяина.
// Компьютер хозяина (host.mjs) держит WebSocket к /__host?key=…; каждый HTTP-запрос игрока Worker передаёт по нему
// хозяину, тот спрашивает свой сервер игры (127.0.0.1:8770) и возвращает ответ. Хранилище — один Durable Object «hub».
// Секрет хозяина — переменная HOST_KEY (wrangler secret put HOST_KEY).

export class Hub {
  constructor(state, env) {
    this.state = state;
    this.env = env;
    this.pending = new Map();   // id запроса -> {resolve, timer}
    this.seq = 0;
  }

  host() {
    const ws = this.state.getWebSockets("host");
    return ws.length ? ws[ws.length - 1] : null;
  }

  async fetch(req) {
    const url = new URL(req.url);
    if (url.pathname === "/__host") {   // хозяин подключается
      if (url.searchParams.get("key") !== this.env.HOST_KEY) return new Response("нет доступа", { status: 403 });
      if (req.headers.get("Upgrade") !== "websocket") return new Response("нужен WebSocket", { status: 426 });
      for (const old of this.state.getWebSockets("host")) try { old.close(1000, "новое подключение хозяина"); } catch (e) {}
      const [client, server] = Object.values(new WebSocketPair());
      this.state.acceptWebSocket(server, ["host"]);
      return new Response(null, { status: 101, webSocket: client });
    }
    const ws = this.host();
    if (!ws) return new Response(OFFLINE, { status: 503, headers: { "Content-Type": "text/html; charset=utf-8", "Cache-Control": "no-store" } });
    const id = ++this.seq, t0 = Date.now();
    const body = req.method === "POST" ? await req.text() : "";
    const msg = { id, method: req.method, path: url.pathname + url.search, body, gzip: (req.headers.get("Accept-Encoding") || "").includes("gzip") };
    const res = await new Promise((resolve) => {
      const timer = setTimeout(() => { this.pending.delete(id); resolve(null); }, 40000);
      this.pending.set(id, { resolve, timer });
      try { ws.send(JSON.stringify(msg)); } catch (e) { clearTimeout(timer); this.pending.delete(id); resolve(null); }
    });
    if (!res) return new Response(JSON.stringify({ type: "error", err: "нет связи с сервером: компьютер хозяина не ответил" }), { status: 504, headers: { "Content-Type": "application/json; charset=utf-8" } });
    const bytes = Uint8Array.from(atob(res.body || ""), (c) => c.charCodeAt(0));
    const h = new Headers({ "Content-Type": res.type || "application/octet-stream", "Cache-Control": "no-store" });
    if (res.enc) h.set("Content-Encoding", res.enc);
    h.set("X-Relay-Ms", String(Date.now() - t0));   // время хаб -> хозяин -> хаб
    return new Response(bytes, { status: res.status || 200, headers: h, encodeBody: res.enc ? "manual" : "automatic" });
  }

  async webSocketMessage(ws, data) {
    let m;
    try { m = JSON.parse(typeof data === "string" ? data : new TextDecoder().decode(data)); } catch (e) { return; }
    if (m.ping) { ws.send(JSON.stringify({ pong: 1 })); return; }
    const p = this.pending.get(m.id);
    if (!p) return;
    clearTimeout(p.timer);
    this.pending.delete(m.id);
    p.resolve(m);
  }

  async webSocketClose(ws) {
    try { ws.close(); } catch (e) {}
  }
}

const OFFLINE = `<!doctype html><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Арена Боссов</title><body style="background:#15171c;color:#eee;font:16px system-ui;display:flex;align-items:center;justify-content:center;height:100vh;margin:0;text-align:center">
<div><h2>Сервер игры сейчас не в сети</h2><p>Компьютер хозяина отключился. Страница обновится сама через 10 секунд.</p></div>
<script>setTimeout(() => location.reload(), 10000)</script>`;

export default {
  async fetch(req, env) {
    // хаб — в Западной Европе: рядом и с компьютером хозяина (выход VPN), и с игроками из России
    const t0 = Date.now();
    const stub = env.HUB.get(env.HUB.idFromName("hub-weur"), { locationHint: "weur" });
    const r = await stub.fetch(req);
    if (r.status === 101 || r.webSocket) return r;   // подключение хозяина (WebSocket) — как есть
    const h = new Headers(r.headers);
    h.set("X-Edge-Ms", String(Date.now() - t0));   // время узел -> хаб -> узел
    h.set("X-Colo", (req.cf && req.cf.colo) || "");
    return new Response(r.body, { status: r.status, headers: h });   // тело уже распаковано рантаймом — он и сожмёт заново
  },
};
