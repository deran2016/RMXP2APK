import queue
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

from gui import BuilderWindow, ROOT, console_build_command, validate_paths


class GuiTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(dir=ROOT / 'build')
        self.root = Path(self.temp.name)
        self.project = self.root / '게임 원본'
        self.project.mkdir()
        (self.project / 'Game.ini').write_text('[Game]\n', encoding='utf-8')

    def tearDown(self):
        self.temp.cleanup()

    def test_paths_preserve_project_and_existing_outputs(self):
        output = self.root / '게임.apk'
        self.assertEqual(validate_paths(str(self.project), str(output)), (self.project.resolve(), output.resolve()))
        with self.assertRaisesRegex(ValueError, '원본 프로젝트 밖'):
            validate_paths(str(self.project), str(self.project / 'game.apk'))
        output.write_bytes(b'existing')
        with self.assertRaisesRegex(ValueError, '이미 존재'):
            validate_paths(str(self.project), str(output))
        self.assertEqual(output.read_bytes(), b'existing')
        self.assertEqual((self.project / 'Game.ini').read_text(encoding='utf-8'), '[Game]\n')

    def test_worker_reports_log_and_success_only_if_apk_exists(self):
        window = object.__new__(BuilderWindow)
        window.messages = queue.Queue()
        output = self.root / 'result.apk'
        code = "import pathlib,sys; print('진행',flush=True); pathlib.Path(sys.argv[1]).write_bytes(b'apk')"
        window.run_build([sys.executable, '-c', code, str(output)], output)
        messages = list(window.messages.queue)
        self.assertEqual(messages[0], ('log', '진행\n'))
        self.assertEqual(messages[-1], ('done', True, 0, str(output)))
        self.assertEqual(output.read_bytes(), b'apk')
        missing = self.root / 'missing.apk'
        window.messages = queue.Queue()
        window.run_build([sys.executable, '-c', 'pass'], missing)
        self.assertEqual(window.messages.get_nowait(), ('done', False, 0, str(missing)))

    def test_pythonw_uses_console_interpreter_for_captured_build_logs(self):
        pythonw = self.root / 'pythonw.exe'
        python = self.root / 'python.exe'
        python.touch()
        with patch.object(sys, 'executable', str(pythonw)):
            command = console_build_command('project', 'output.apk', 'UTF-8', 'UTF-8', '', '', '')
        self.assertEqual(command[0], str(python))


if __name__ == '__main__':
    unittest.main()
