#!/usr/bin/env python3
"""On-demand waveform comparison against a preserved reference CLAP binary."""
import argparse
from array import array
import hashlib
import json
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--before', type=Path, required=True)
    parser.add_argument('--after', type=Path, required=True)
    parser.add_argument('--host', type=Path, default=ROOT/'build-grid/NeuralAmpModeler'/('nam-fx-performance.exe' if sys.platform == 'win32' else 'nam-fx-performance'))
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    folder = args.output.resolve()
    folder.mkdir(parents=True, exist_ok=False)
    subprocess.run([str(args.host), '--fixtures', str(folder/'fixtures')], check=True, capture_output=True)
    modules = json.loads(subprocess.check_output([str(args.host), '--modules'], text=True, encoding='utf-8'))
    # Native parameter values are converted to the adapter's host representation.
    shared = [(100, 'Split Crossover', 1.), (120, 'Split Frequency', 3500.),
              (160, 'Lane A Level', -6.), (180, 'Lane B Level', 3.),
              (200, 'Parallel Blend', 60.), (240, 'Lane Fine Timing', 1.),
              (260, 'Split Frequency', 40.), (300, 'Lane A Mute', 1.),
              (330, 'Lane A Mute', 0.), (370, 'Split Crossover', 0.),
              (420, 'Global Input', -12.),
              (440, 'Output', -6.), (460, 'Tuner', 1.), (480, 'Tuner Mute', 1.),
              (520, 'Tuner Mute', 0.), (540, 'Tuner', 0.)]
    ir_changes = [(10, 'Pre 1 IR Level', -6.), (80, 'Pre 1 Enabled', 0.),
                  (400, 'Pre 1 Enabled', 1.), (500, 'Pre 1 IR Level', 0.),
                  (600, 'LFO Target', 'Parallel blend'), (601, 'LFO Depth', 40.)]
    cases = [('shared', {'id': 'empty', 'effect': 0, 'controls': {}}, shared)]
    for name in ['ir-2048.wav', 'ir-16384.wav']:
        cases.append((name, {'id': name, 'effect': modules['Cabinet IR'], 'controls': {},
                             'asset': str(folder/'fixtures'/name)}, ir_changes))
    rows = []
    try:
        for rate in [44100, 48000, 96000, 192000]:
            for buffer in [32, 64, 128]:
                for name, case, changes in cases:
                    case_path = folder/'case.json'
                    case_path.write_text(json.dumps(case), encoding='utf-8')
                    waveforms = []
                    for label, plugin in [('before', args.before), ('after', args.after)]:
                        render = folder/(label+'.f64')
                        settings = {'rates': [rate], 'buffers': [buffer], 'seconds': 2., 'warmup_seconds': .5,
                                    'repeats': 1, 'paced': False, 'render_path': str(render),
                                    'parameter_events': [{'block': b, 'name': n, 'value': v} for b,n,v in changes]}
                        settings_path = folder/'settings.json'
                        settings_path.write_text(json.dumps(settings), encoding='utf-8')
                        run = subprocess.run([str(args.host), '--run', str(plugin.resolve()), str(case_path), str(settings_path)],
                                             capture_output=True, text=True, timeout=120)
                        result = json.loads(run.stdout.splitlines()[-1])
                        if run.returncode or result.get('error') or result.get('process_errors') or result.get('nonfinite_samples'):
                            raise RuntimeError(f'{label}/{name}/{rate}/{buffer}: {result}')
                        samples = array('d')
                        samples.frombytes(render.read_bytes())
                        waveforms.append(samples)
                        render.unlink()  # Only this audit's own generated file.
                    assert len(waveforms[0]) == len(waveforms[1])
                    maximum = max(abs(a-b) for a,b in zip(*waveforms))
                    # Shared changes should be bit-identical; FFT has float rounding differences.
                    tolerance = 0. if name == 'shared' else 1.e-6
                    passed = maximum <= tolerance
                    rows.append({'case': name, 'rate': rate, 'buffer': buffer, 'samples': len(waveforms[0]),
                                 'max_error': maximum, 'tolerance': tolerance, 'pass': passed})
                    print(f'{name} {rate}/{buffer}: max error {maximum:.6g}, pass={passed}', flush=True)
    finally:
        metadata = {'before_sha256': hashlib.sha256(args.before.read_bytes()).hexdigest(),
                    'after_sha256': hashlib.sha256(args.after.read_bytes()).hexdigest(), 'rows': rows,
                    'complete': len(rows) == 36, 'pass': len(rows) == 36 and all(r['pass'] for r in rows)}
        (folder/'report.json').write_text(json.dumps(metadata, indent=2), encoding='utf-8')
    return int(not metadata['pass'])


if __name__ == '__main__':
    sys.exit(main())
