import json
import pathlib
import subprocess
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]


class CityGenerationTest(unittest.TestCase):
    def test_engine_city_command_returns_seeded_plan_for_world_town(self):
        binary = ROOT / "build" / "kampaniya"
        self.assertTrue(binary.exists(), "сначала соберите make kampaniya")
        commands = [
            {"cmd": "world", "id": "city-test", "kind": "gen", "use": True,
             "params": {"name": "Тестовый мир", "size": 250, "seed": 7162, "sea": 0.3,
                        "mount": 0.5, "rough": 0.5, "climate": 0, "humid": 0,
                        "towns": 1, "rivers": 1}},
        ]
        proc = subprocess.Popen([str(binary)], cwd=ROOT, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                stderr=subprocess.PIPE, text=True)
        try:
            proc.stdin.write(json.dumps(commands[0], ensure_ascii=False) + "\n")
            proc.stdin.flush()
            world = json.loads(proc.stdout.readline())
            self.assertEqual(world["type"], "world")
            town = world["info"]["towns"][0]
            cmd = {"cmd": "city", "id": "city-test", "town": town[0], "x": town[1], "y": town[2]}
            results = []
            for _ in range(2):
                proc.stdin.write(json.dumps(cmd, ensure_ascii=False) + "\n")
                proc.stdin.flush()
                results.append(json.loads(proc.stdout.readline()))
            self.assertEqual([r["type"] for r in results], ["city", "city"])
            plan = results[0]["plan"]
            self.assertEqual(plan, results[1]["plan"])
            self.assertGreater(len(plan["roads"]), 4)
            self.assertGreater(len(plan["buildings"]), 8)
            self.assertEqual(len(plan["fields"]), 8)
        finally:
            proc.kill()
            proc.wait(timeout=5)
            proc.stdin.close()
            proc.stdout.close()
            proc.stderr.close()


if __name__ == "__main__":
    unittest.main()
