"""Статические проверки интерфейсного контракта собранной игровой страницы."""
import os
import sys
import unittest

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "web"))
import play_server as ps  # noqa: E402


class TestUiContract(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.page = ps.page()

    def test_theme_picker_and_palettes_are_in_page(self):
        for value in ("system", "dark", "light", "red-black", "red-navy", "forest"):
            with self.subTest(theme=value):
                self.assertIn(f'<option value="{value}"', self.page)
                self.assertIn(f'data-theme="{value}"', self.page)
        self.assertIn("localStorage.setItem('dnd_theme', value)", self.page)

    def test_additional_damage_starts_with_one_and_adds_explicitly(self):
        self.assertIn("const rows = Math.min(filled, dt.length)", self.page)
        self.assertIn('data-moreadd="${P(\'more\')}"', self.page)
        self.assertIn("if (keys.length < DT_OPTS().length)", self.page)
        self.assertNotIn("Math.max(2, filled + 1)", self.page)


if __name__ == "__main__":
    unittest.main()
