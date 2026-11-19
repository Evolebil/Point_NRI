# Изменения (ГМ-РКМ-03-1.0)

## 2026-11-19

### Лаги сайта (Д-34, отдельная задача)
- `relay/worker.js`: кэш ответа — как велит сервер игры (раньше всем `no-store`); страница — с `ETag` и ответом 304.
- `relay/host.mjs`: передаёт `Cache-Control`; для страницы считает `ETag` и при совпадении отвечает 304 без тела.
- `relay/wrangler.toml`, `relay/deploy.sh`: библиотеки 3D (`/vendor/…`) и модели бойцов (`/assets/models/…`) раздаёт сам
  Cloudflare (статические файлы Worker), без туннеля.

### Ш-00: Разведка кода
- `docs/GM-RKM_sootvetstvie.md` — таблица соответствия «в ТЗ — в коде».
- `QUESTIONS.md`, `ACCEPTANCE.md`, `DEPENDENCIES.md` — журналы по разделу 9 ТЗ.
