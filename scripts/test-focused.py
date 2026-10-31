#!/usr/bin/env python3
"""Run selected existing tests without native editor automation.

Requires an already configured build directory. Full validation remains build.sh --test.
"""
import argparse
import os
from pathlib import Path
import re
import subprocess
import sys

SUITES = {
    'library': (['nam-library-tests', 'nam-clap-smoke', 'nam-grid-clap'], ['capture-library', 'library-startup']),
    'state': (['nam-parameter-contract', 'nam-clap-smoke', 'nam-grid-clap'], ['parameter-contract', 'clap-smoke']),
    'dsp': (['nam-stage-tests', 'nam-aliasing-tests'], ['stages', 'aliasing']),
    'ir': (['nam-ir-tests', 'nam-wav-tests'], ['ir-convolution', 'wav-decoder']),
    'resampling': (['nam-resampling-tests'], ['resampling']),
    'parity': (['nam-grid-clap', 'nam-reference-parity'], ['reference-parity', 'reference-parity-lite']),
}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    selection = parser.add_mutually_exclusive_group(required=True)
    selection.add_argument('--suite', choices=SUITES)
    selection.add_argument('--stage-case', help='DSP group name; use --list-stage-cases to discover names')
    selection.add_argument('--list-stage-cases', action='store_true')
    parser.add_argument('--build-dir', type=Path, required=True)
    parser.add_argument('--config', default='Release')
    parser.add_argument('--jobs', type=int, default=3)
    args = parser.parse_args()
    if args.jobs < 1:
        parser.error('--jobs must be positive')
    build = args.build_dir.resolve()
    if not (build / 'CMakeCache.txt').is_file():
        parser.error('configure the build directory first (see docs/build.md)')
    targets, names = SUITES[args.suite] if args.suite else (['nam-stage-tests'], [])
    if args.suite == 'state' and sys.platform == 'win32':
        targets = targets + ['nam-grid-vst3', 'nam-vst3-parameter-contract']
        names = names + ['vst3-parameter-contract']
    subprocess.run(['cmake', '--build', str(build), '--config', args.config,
                    '--target', *targets, '--parallel', str(args.jobs)], check=True)
    env = dict(os.environ, NAM_GRID_HEADLESS_SMOKE='1')
    if args.suite:
        subprocess.run(['ctest', '--test-dir', str(build), '-C', args.config,
                        '-R', '^(' + '|'.join(map(re.escape, names)) + ')$',
                        '--output-on-failure', '--no-tests=error'], check=True, env=env)
    else:
        suffix = '.exe' if sys.platform == 'win32' else ''
        candidates = [build / 'NeuralAmpModeler' / args.config / ('nam-stage-tests' + suffix),
                      build / 'NeuralAmpModeler' / ('nam-stage-tests' + suffix)]
        executable = next((p for p in candidates if p.is_file()), None)
        if executable is None:
            parser.error('cannot locate built nam-stage-tests executable')
        command = ['--list'] if args.list_stage_cases else ['--case', args.stage_case]
        subprocess.run([str(executable), *command], cwd=build / 'NeuralAmpModeler', check=True, env=env)


if __name__ == '__main__':
    try:
        main()
    except subprocess.CalledProcessError as error:
        sys.exit(error.returncode if error.returncode > 0 else 1)
