"""Which characters each font is exported with (fonts.font_chars)."""
import os
import sys
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(HERE))
from spire_convert import fonts  # noqa: E402

LANGS = {"eng", "deu", "jpn", "pol", "rus", "zhs"}


class FontChars(unittest.TestCase):
    def test_main_font_gets_chosen_and_native_names(self):
        got = fonts.font_chars("fonts/kreon_regular.ttf", "abc", {"eng"}, LANGS)
        self.assertTrue(set("abc") <= set(got))
        self.assertIn("ü", got)  # Türkçe
        self.assertIn("日", got)

    def test_language_font_not_chosen_gets_native_names_only(self):
        got = fonts.font_chars("fonts/jpn/NotoSansCJKjp-Regular.otf", "abcxyz", {"eng"}, LANGS)
        self.assertIn("日", got)
        self.assertNotIn("x", got)

    def test_language_font_chosen_gets_its_characters(self):
        got = fonts.font_chars("fonts/jpn/NotoSansCJKjp-Regular.otf", "abcxyz", {"jpn"}, LANGS)
        self.assertIn("x", got)

    def test_polish_uses_the_russian_folder(self):
        got = fonts.font_chars("fonts/rus/FiraSansExtraCondensed-Regular.ttf", "ąęł", {"pol"}, LANGS)
        self.assertIn("ł", got)

    def test_all_characters(self):
        self.assertIsNone(fonts.font_chars("fonts/kreon_regular.ttf", None, {"eng"}, LANGS))


if __name__ == "__main__":
    unittest.main()
