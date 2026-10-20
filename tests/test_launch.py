"""Exercise real launch.sh argument forwarding without starting Wine or a game."""
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class LaunchTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="settlers launch ")
        self.addCleanup(self.temp.cleanup)
        self.data = Path(self.temp.name) / "Game Data"
        self.compat = self.data / "MacCompatibility"
        self.compat.mkdir(parents=True)
        shutil.copyfile(ROOT / "launch.sh", self.compat / "launch.sh")
        (self.compat / "libMoltenVK.dylib").touch()
        self.result = self.data / "arguments.json"
        self.fake = self.data / "fake wine"
        self.fake.write_text(
            '#!/usr/bin/env python3\nimport json, os, sys\n'
            'with open(os.environ["TEST_RESULT"], "w") as f:\n'
            ' json.dump({"args": sys.argv[1:], "vulkan": os.environ.get("CX_LIBVULKAN"), '
            '"initialized": os.environ.get("CX_INITIALIZED")}, f)\n'
            'sys.exit(int(os.environ.get("TEST_EXIT", "0")))\n'
        )
        self.fake.chmod(0o700)
        self.env = {k: v for k, v in os.environ.items()
                    if k not in {"CX_BOTTLE", "SETTLERS7_BOTTLE", "CROSSOVER_WINE"}}
        self.env.update(CROSSOVER_WINE=str(self.fake), CX_BOTTLE="A bottle with spaces",
                        CX_INITIALIZED="inherited", TEST_RESULT=str(self.result))

    def run_script(self, *args):
        return subprocess.run(["/bin/sh", str(self.compat / "launch.sh"), *args],
                              env=self.env, capture_output=True, text=True)

    def test_forwards_literal_arguments_and_environment(self):
        command = [r"C:\Games\A game\Settlers7R.exe", 'quoted "text"',
                   "$(touch MUST_NOT_EXIST)", "semi;colon", "-flag"]
        result = self.run_script(*command)
        self.assertEqual(result.returncode, 0, result.stderr)
        recorded = json.loads(self.result.read_text())
        args = recorded["args"]
        self.assertEqual(args[args.index("--cx-app") + 1:], command)
        self.assertEqual(args[args.index("--bottle") + 1], "A bottle with spaces")
        self.assertEqual(args[args.index("--workdir") + 1], str(self.data))
        self.assertEqual(recorded["vulkan"], str(self.compat / "libMoltenVK.dylib"))
        self.assertIsNone(recorded["initialized"])

    def test_missing_inputs_fail_before_launch(self):
        self.assertEqual(self.run_script().returncode, 2)
        self.env.pop("CX_BOTTLE")
        self.assertEqual(self.run_script("game.exe").returncode, 2)
        self.assertFalse(self.result.exists())

    def test_explicit_bottle_and_exit_code(self):
        self.env.update(SETTLERS7_BOTTLE="Selected bottle", TEST_EXIT="7")
        self.assertEqual(self.run_script("game.exe").returncode, 7)
        args = json.loads(self.result.read_text())["args"]
        self.assertEqual(args[args.index("--bottle") + 1], "Selected bottle")

    def test_missing_library_fails_before_launch(self):
        (self.compat / "libMoltenVK.dylib").unlink()
        self.assertEqual(self.run_script("game.exe").returncode, 2)
        self.assertFalse(self.result.exists())


if __name__ == "__main__":
    unittest.main()
