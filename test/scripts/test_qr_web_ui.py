import re
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
APP_PAGE = ROOT / "src" / "network" / "html" / "AppPage.html"


class QRWebUiTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.html = APP_PAGE.read_text(encoding="utf-8")
        match = re.search(r"<script>(.*)</script>", cls.html, re.S)
        cls.script = match.group(1)

    def test_qr_tab_and_panel_are_present(self):
        self.assertIn('data-tab="qrcodes"', self.html)
        self.assertIn('id="panel-qrcodes"', self.html)
        self.assertIn('id="qr-form"', self.html)
        self.assertIn('aria-live="polite"', self.html)
        self.assertIn("Codes are saved on the SD card. After leaving WiFi Transfer, open Apps &gt; QR Codes.", self.html)

    def test_qr_byte_limits_and_api_contract(self):
        self.assertIn("maxNameBytes:48", self.script)
        self.assertIn("maxDataBytes:512", self.script)
        self.assertIn("new TextEncoder().encode(s).length", self.script)
        self.assertIn("fetch('/api/qrcodes'", self.script)
        self.assertIn("method:'POST'", self.script)
        self.assertIn("method:'DELETE'", self.script)
        self.assertIn("'Content-Type':'application/json'", self.script)

    def test_qr_user_strings_are_rendered_as_text(self):
        qr_section = self.script.split("// --- QR Codes ---", 1)[1].split("// --- Init ---", 1)[0]
        self.assertNotIn("innerHTML", qr_section)
        self.assertIn("name.textContent=code.name||'Untitled'", qr_section)
        self.assertIn("data.textContent=code.data||''", qr_section)

    def test_embedded_script_has_valid_syntax(self):
        node = shutil.which("node")
        if not node:
            self.skipTest("node is not available")
        with tempfile.NamedTemporaryFile("w", suffix=".js", delete=False, encoding="utf-8") as f:
            f.write(self.script)
            temp_path = f.name
        try:
            result = subprocess.run([node, "--check", temp_path], text=True, capture_output=True)
        finally:
            Path(temp_path).unlink(missing_ok=True)
        self.assertEqual(result.returncode, 0, result.stderr)


if __name__ == "__main__":
    unittest.main()
