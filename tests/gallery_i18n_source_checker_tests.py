"""Negative source probes: omissions must be located without a compiler."""
import importlib.util
from pathlib import Path
import unittest
from tempfile import TemporaryDirectory

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("gallery_checker", ROOT / "tools/check_gallery_i18n.py")
checker = importlib.util.module_from_spec(spec)
spec.loader.exec_module(checker)


class DisplaySourceChecks(unittest.TestCase):
    def test_corrupted_chinese_declaration_is_rejected(self):
        previous = checker.DIRECTORY
        try:
            with TemporaryDirectory() as directory:
                root = Path(directory)
                source = root / "examples/token_gallery/gallery_i18n.cpp"
                source.parent.mkdir(parents=True)
                source.write_text('{MessageKey{u8"gallery.example", String{u8"Example"}}, String{u8"????"}}',
                                  encoding="utf-8")
                with self.assertRaisesRegex(ValueError, "Corrupted Chinese message: gallery.example"):
                    checker.check(root)
        finally:
            checker.DIRECTORY = previous

    def check_text(self, body, fixtures=()):
        return checker.unlocalized_ui("example.cpp", "void page() {\n" + body + "\n}\n", set(fixtures))

    def test_both_scripts_and_computed_feedback_are_reported(self):
        for body in ('Text(u8"保存");', 'ButtonProps{}.title("Save");',
                     'feedback.set(String{u8"Saved: "} + payload);'):
            with self.subTest(body=body):
                result = self.check_text(body)
                self.assertEqual(len(result), 1)
                self.assertTrue(result[0].startswith("example.cpp:2: unbound application text:"))

    def test_model_labels_cannot_hide_behind_automation_keys(self):
        for body in ('TabItem{u8"stable.tab", u8"Settings"};',
                     'TreeItem{u8"stable.node", String{u8"设置"}};',
                     'Text("gallery.sample.stable");'):
            self.assertEqual(len(self.check_text(body)), 1)

    def test_fixture_is_exact_in_source_function_and_payload(self):
        fixture = ("example.cpp", "page", "中文 / abc")
        self.assertEqual(self.check_text('Text(u8"中文 / abc");', [fixture]), [])
        self.assertEqual(len(self.check_text('Text(u8"中文 / abcd");', [fixture])), 1)
        self.assertEqual(len(self.check_text('Text(u8"中文 / abc");',
                                           [("other.cpp", "page", "中文 / abc")])), 1)
        self.assertEqual(len(self.check_text('Text(u8"中文 / abc");',
                                           [("example.cpp", "other", "中文 / abc")])), 1)

    def test_bound_sources_and_non_display_data_are_not_omissions(self):
        source = '''// Text("comment");
Text(tr.text(key, args));
Text(gallery_text("gallery.example"));
TreeItem{u8"stable.node", String{}}.title(tr.text(key));
InputProps{}.defaultValue(u8"可编辑业务正文");
Text(u8"09:30");'''
        self.assertEqual(self.check_text(source), [])


if __name__ == "__main__":
    unittest.main()
