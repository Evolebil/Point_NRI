# Изменения (ГМ-РКМ-03-1.0)

## 2026-11-19

### Лаги сайта (Д-34, отдельная задача)
- `relay/worker.js`, `relay/host.mjs`: кэш ответа — как велит сервер игры (раньше всем `no-store`).
- `web/play_server.py`: `/` — оболочка страницы, стили и скрипт — `/app.<хеш>.css|js` с кэшем на год.
- `relay/wrangler.toml`, `relay/deploy.sh`: библиотеки 3D (`/vendor/…`) и модели бойцов (`/assets/models/…`) раздаёт сам
  Cloudflare (статические файлы Worker), без туннеля.

### Ш-00: Разведка кода
- `docs/GM-RKM_sootvetstvie.md` — таблица соответствия «в ТЗ — в коде».
- `QUESTIONS.md`, `ACCEPTANCE.md`, `DEPENDENCIES.md` — журналы по разделу 9 ТЗ.
