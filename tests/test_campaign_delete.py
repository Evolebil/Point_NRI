"""Deletion from the campaign list is master-only and recoverable."""
import json
import os
import sys
import tempfile
import unittest
from types import SimpleNamespace
from unittest.mock import patch

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "web"))
import kampaniya  # noqa: E402


class FakeProcess:
    def __init__(self):
        self.killed = False

    def kill(self):
        self.killed = True

    def wait(self, timeout=None):
        return 0


class CampaignDeleteTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.state = os.path.join(self.tmp.name, "kampaniya")
        os.makedirs(self.state)
        self.campaign_dir = os.path.join(self.state, "ABCDE")
        os.makedirs(self.campaign_dir)
        with open(os.path.join(self.campaign_dir, "meta.json"), "w", encoding="utf-8") as f:
            json.dump({"имя": "Тест", "мастер": "master", "игроки": {}}, f)
        with open(os.path.join(self.campaign_dir, "journal.jsonl"), "w", encoding="utf-8") as f:
            f.write('{"type":"note"}\n')
        self.proc = FakeProcess()
        self.campaign = SimpleNamespace(cid="ABCDE", dir=self.campaign_dir, meta={"мастер": "master", "игроки": {}},
                                        eng=SimpleNamespace(p=self.proc), join=lambda key, shown: None)
        self.camps = {"ABCDE": self.campaign}
        self.patches = [patch.object(kampaniya, "STATE", self.state), patch.object(kampaniya, "CAMPS", self.camps)]
        for p in self.patches:
            p.start()

    def tearDown(self):
        for p in reversed(self.patches):
            p.stop()
        self.tmp.cleanup()

    def test_master_moves_campaign_to_trash_and_stops_engine(self):
        result = kampaniya.kmp_cmd({"cmd": "kmp_delete", "id": "ABCDE", "name": "master"})
        self.assertEqual(result["type"], "kmp_deleted")
        self.assertNotIn("ABCDE", self.camps)
        entries = os.listdir(os.path.join(self.state, "trash"))
        self.assertEqual(len(entries), 1)
        self.assertTrue(os.path.isfile(os.path.join(self.state, "trash", entries[0], "journal.jsonl")))
        self.assertTrue(self.proc.killed)

    def test_non_master_cannot_delete_campaign(self):
        result = kampaniya.kmp_cmd({"cmd": "kmp_delete", "id": "ABCDE", "name": "player"})
        self.assertEqual(result["type"], "error")
        self.assertTrue(os.path.isdir(self.campaign_dir))
        self.assertFalse(self.proc.killed)


if __name__ == "__main__":
    unittest.main()
