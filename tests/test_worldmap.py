"""Интеграционные проверки европейской карты и переключения слоя биомов."""
import json
import os
import subprocess
import sys
import unittest
from types import SimpleNamespace

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "web"))
import kampaniya  # noqa: E402


class TestEuropeWorld(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        fake_campaign = SimpleNamespace(vault=SimpleNamespace(cfg={"регион": "web/data/region_europe.json.gz"}))
        cls.region_path = kampaniya.Campaign.region_path(fake_campaign)

    def test_full_region_loads_in_cpp_and_keeps_layers(self):
        self.assertTrue(self.region_path and os.path.isfile(self.region_path))
        proc = subprocess.Popen([os.path.join(ROOT, "build", "kampaniya")], cwd=ROOT, stdin=subprocess.PIPE,
                                stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, encoding="utf-8", bufsize=1)
        try:
            request = {"cmd": "world_file", "id": "europe-test", "kind": "act", "path": self.region_path, "use": True}
            proc.stdin.write(json.dumps(request, ensure_ascii=False, separators=(",", ":")) + "\n")
            proc.stdin.flush()
            world = json.loads(proc.stdout.readline())
            self.assertEqual(world["type"], "world")
            info = world["info"]
            self.assertEqual(info["name"], "Европа и Средиземноморье 1208")
            self.assertGreater(info["w"], 5300)
            self.assertGreater(info["h"], 4800)
            self.assertGreater(info["cellKm"], 0.7)
            self.assertLess(info["cellKm"], 1.2)
            self.assertGreater(len(info["towns"]), 1500)
            self.assertGreater(len(info["labels"]), 30)
            self.assertTrue(info["realmLayer"])
            self.assertTrue(info["peopleLayer"])
            city_names = {town[0] for town in info["towns"]}
            self.assertTrue({"Париж", "Рим", "Константинополь"} <= city_names)
            self.assertGreater(info["zMax"], 0)

            proc.stdin.write('{"cmd":"tile","id":"europe-test","z":0,"tx":3,"ty":9}\n')
            proc.stdin.flush()
            tile = json.loads(proc.stdout.readline())["tile"]
            self.assertEqual(len(tile["biome"]), 256 * 256)
            self.assertEqual(len(tile["terr"]), 256 * 256)
            for layer in ("cover", "relief", "people", "realm"):
                self.assertEqual(len(tile[layer]), 256 * 256)
            self.assertTrue(set(tile["biome"]) <= set("0123456789"))
            self.assertGreater(sum(flag in "13" for flag in tile["flags"]), 100)
            self.assertGreater(sum(flag in "23" for flag in tile["flags"]), 0)

            proc.stdin.write('{"cmd":"tile","id":"europe-test","z":2,"tx":1,"ty":2}\n')
            proc.stdin.flush()
            overview = json.loads(proc.stdout.readline())["tile"]
            self.assertGreater(sum("1" <= flag <= "3" for flag in overview["flags"]), 0)

            proc.stdin.write('{"cmd":"tile","id":"europe-test","z":0,"tx":3,"ty":9,"fog":true}\n')
            proc.stdin.flush()
            hidden = json.loads(proc.stdout.readline())["tile"]
            self.assertEqual(set(hidden["terr"]), {":"})
            self.assertEqual(set(hidden["biome"]), {"0"})
            self.assertEqual(set(hidden["flags"]), {"0"})
            for layer in ("cover", "relief", "people", "realm"):
                self.assertEqual(set(hidden[layer]), {"0"})
        finally:
            if proc.poll() is None:
                proc.stdin.write('{"cmd":"quit"}\n')
                proc.stdin.flush()
                proc.wait(timeout=10)
            proc.stdin.close()
            proc.stdout.close()
            proc.stderr.close()

    def test_campaign_map_has_biome_selector_and_visible_fog_legend(self):
        with open(os.path.join(ROOT, "web", "js", "60_kampaniya.js"), encoding="utf-8") as f:
            source = f.read()
        self.assertIn('data-kmpreset="${id}"', source)
        self.assertIn("['physical','Физическая']", source)
        self.assertIn("['player','Для игроков']", source)
        self.assertIn('id="kmLegend"', source)
        self.assertIn("if (kmFogged())", source)
        self.assertIn("KM_BIOME_RGB", source)
        self.assertIn('data-kmlayer="people"', source)
        self.assertIn('data-kmlayer="realm"', source)
        for layer in ("water", "relief", "cover", "landforms", "rivers", "roads", "settlements", "labels", "grid", "compass", "game"):
            self.assertIn(f'data-kmlayer="{layer}"', source)
        self.assertIn("Под курсором:", source)


if __name__ == "__main__":
    unittest.main()
