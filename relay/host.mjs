// Компьютер хозяина: держит связь с посредником на Cloudflare Workers и отвечает на запросы игроков своим сервером игры.
// Запуск: node relay/host.mjs https://dnd-arena.<логин>.workers.dev   (ключ — relay/host_key.txt)
// Обрыв связи — переподключение через 2 с; ссылка для игроков при этом не меняется.
import { readFileSync } from "node:fs";
import { gzipSync } from "node:zlib";

const base = (process.argv[2] || "").replace(/\/+$/, "");
if (!/^https?:\/\//.test(base)) { console.error("укажите адрес Worker: node relay/host.mjs https://dnd-arena.<логин>.workers.dev"); process.exit(1); }
const key = readFileSync(new URL("./host_key.txt", import.meta.url), "utf8").trim();
const LOCAL = "http://127.0.0.1:8770";
const wsUrl = base.replace(/^http/, "ws") + "/__host?key=" + encodeURIComponent(key);

function connect() {
  const ws = new WebSocket(wsUrl);
  let alive = null;
  ws.onopen = () => {
    console.log(new Date().toLocaleTimeString(), "на связи — игроки заходят по", base);
    alive = setInterval(() => { try { ws.send(JSON.stringify({ ping: 1 })); } catch (e) {} }, 20000);
  };
  ws.onmessage = async (ev) => {
    let m;
    try { m = JSON.parse(ev.data); } catch (e) { return; }
    if (m.pong || m.id == null) return;
    let out;
    try {
      const r = await fetch(LOCAL + m.path, { method: m.method, body: m.method === "POST" ? m.body : undefined, headers: { "Content-Type": "application/json", "Accept-Encoding": "identity" } });
      let buf = Buffer.from(await r.arrayBuffer());
      const type = r.headers.get("Content-Type") || "application/octet-stream";
      let enc = "";
      if (m.gzip && buf.length > 1024) { buf = gzipSync(buf, { level: 6 }); enc = "gzip"; }
      out = { id: m.id, status: r.status, type, enc, body: buf.toString("base64") };
    } catch (e) {
      out = { id: m.id, status: 502, type: "application/json; charset=utf-8", enc: "", body: Buffer.from(JSON.stringify({ type: "error", err: "сервер игры не отвечает: " + e.message })).toString("base64") };
    }
    try { ws.send(JSON.stringify(out)); } catch (e) {}
  };
  ws.onclose = (ev) => {
    clearInterval(alive);
    if (String(ev.reason || "").includes("новое подключение хозяина")) {   // запущен второй хозяин — этот уступает
      console.log(new Date().toLocaleTimeString(), "подключился другой экземпляр хозяина — этот выключается");
      process.exit(0);
    }
    console.log(new Date().toLocaleTimeString(), "связь потеряна (" + (ev.code || "") + " " + (ev.reason || "") + "), переподключаюсь…");
    setTimeout(connect, 2000);
  };
  ws.onerror = (e) => { console.log(new Date().toLocaleTimeString(), "ошибка связи:", (e && (e.message || (e.error && e.error.message))) || e.type || ""); };
}
connect();
