#!/bin/sh
# Выкладка посредника: копирует библиотеки 3D и модели бойцов в relay/public (их раздаёт Cloudflare без туннеля) и публикует Worker.
# Запуск из корня проекта: sh relay/deploy.sh
set -e
cd "$(dirname "$0")"
rm -rf public
mkdir -p public/vendor public/assets/models
cp ../web/vendor/three-r159.min.js ../web/vendor/three-r159-gltf.js public/vendor/
cp ../web/assets/models/*.glb public/assets/models/
# заголовки кэша: имена библиотек содержат версию — на год; модели — на сутки (как отдаёт play_server.py)
cat > public/_headers <<'H'
/vendor/*
  Cache-Control: public, max-age=31536000, immutable
/assets/models/*
  Cache-Control: public, max-age=86400
H
npx wrangler deploy "$@"
