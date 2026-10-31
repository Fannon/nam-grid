import importlib.util
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('test_focused', ROOT / 'scripts/test-focused.py')
runner = importlib.util.module_from_spec(spec)
spec.loader.exec_module(runner)


class FocusedRunnerTests(unittest.TestCase):
    def test_library_is_scoped_and_headless(self):
        with tempfile.TemporaryDirectory() as directory:
            (Path(directory) / 'CMakeCache.txt').touch()
            with patch.object(sys, 'argv', ['test-focused', '--build-dir', directory, '--suite', 'library']), patch.object(runner.subprocess, 'run') as run:
                runner.main()
            self.assertEqual(run.call_count, 2)
            build, test = run.call_args_list
            self.assertIn('nam-library-tests', build.args[0])
            self.assertNotIn('nam-stage-tests', build.args[0])
            self.assertIn('^(capture\\-library|library\\-startup)$', test.args[0])
            self.assertEqual(test.kwargs['env']['NAM_GRID_HEADLESS_SMOKE'], '1')
            self.assertIn('--no-tests=error', test.args[0])

    def test_stage_selection_runs_only_requested_group(self):
        with tempfile.TemporaryDirectory() as directory:
            build = Path(directory)
            (build / 'CMakeCache.txt').touch()
            folder = build / 'NeuralAmpModeler'
            folder.mkdir()
            (folder / ('nam-stage-tests.exe' if sys.platform == 'win32' else 'nam-stage-tests')).touch()
            with patch.object(sys, 'argv', ['test-focused', '--build-dir', directory, '--stage-case', 'pedal-comp']), patch.object(runner.subprocess, 'run') as run:
                runner.main()
            self.assertEqual(run.call_args.args[0][1:], ['--case', 'pedal-comp'])
            # Windows temporary paths may use an 8.3 alias that the runner resolves.
            self.assertTrue(run.call_args.kwargs['cwd'].samefile(folder))

    def test_test_failure_propagates(self):
        with tempfile.TemporaryDirectory() as directory:
            (Path(directory) / 'CMakeCache.txt').touch()
            failure = runner.subprocess.CalledProcessError(8, 'ctest')
            with patch.object(sys, 'argv', ['test-focused', '--build-dir', directory, '--suite', 'ir']), patch.object(runner.subprocess, 'run', side_effect=[None, failure]):
                with self.assertRaises(runner.subprocess.CalledProcessError):
                    runner.main()
