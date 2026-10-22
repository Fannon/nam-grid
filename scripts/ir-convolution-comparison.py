#!/usr/bin/env python3
"""Explicit, standalone IR engine experiment; no plugin or device is modified."""
import argparse
from collections import defaultdict
import csv
from datetime import datetime, timezone
import hashlib
import itertools
import json
from pathlib import Path
import platform
import random
import statistics
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
ENGINES = ['direct', 'fft-uniform', 'fft-two-stage', 'wdl-div']


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def worker(executable, mode, config, folder):
    path = folder / 'case.json'
    path.write_text(json.dumps(config), encoding='utf-8')
    try:
        run = subprocess.run([str(executable), mode, str(path)], capture_output=True,
                             text=True, timeout=120, encoding='utf-8', errors='replace')
        result = json.loads(run.stdout.splitlines()[-1])
        if run.returncode:
            result['error'] = result.get('error', f'exit {run.returncode}: {run.stderr[-1000:]}')
    except (OSError, subprocess.TimeoutExpired, ValueError, IndexError) as error:
        result = {'error': str(error)}
    return dict(config, **result)


def summarize(rows):
    groups = defaultdict(list)
    for row in rows:
        groups[(row['asset_id'], row['rate'], row['buffer'], row['engine'])].append(row)
    result = []
    for (asset, rate, buffer, engine), group in sorted(groups.items()):
        value = {'asset_id': asset, 'rate': rate, 'buffer': buffer, 'engine': engine,
                 'measurements': len(group), 'errors': sum(bool(r.get('error')) for r in group)}
        valid = [r for r in group if 'timing' in r and not r.get('error')]
        if len(valid) == len(group):
            value.update(cpu_percent=statistics.median(r['timing']['cpu_percent'] for r in valid),
                         p99_us=statistics.median(r['timing']['p99_us'] for r in valid),
                         max_us=max(r['timing']['max_us'] for r in valid),
                         misses=sum(r['timing']['deadline_misses'] for r in valid),
                         allocations=sum(r['allocations'] for r in valid),
                         releases=sum(r['releases'] for r in valid),
                         nonfinite=sum(r['nonfinite'] for r in valid),
                         effective_taps=valid[0]['effective_taps'],
                         rounds={str(round): statistics.median(r['timing']['cpu_percent'] for r in valid
                                                              if r['round'] == round)
                                 for round in sorted({r['round'] for r in valid})})
        result.append(value)
    baselines = {(r['asset_id'], r['rate'], r['buffer']): r for r in result if r['engine'] == 'direct'}
    for row in result:
        baseline = baselines.get((row['asset_id'], row['rate'], row['buffer']), {})
        if ('cpu_percent' in baseline and 'cpu_percent' in row and not row.get('nonfinite')
                and not baseline.get('nonfinite') and baseline['measurements'] == row['measurements']):
            row['reduction_percent'] = 100. * (1. - row['cpu_percent'] / baseline['cpu_percent'])
    return result


def reports(folder, metadata, audits, rows):
    summary = summarize(rows)
    metadata['complete'] = len(rows) == metadata['planned_measurements']
    data = {'metadata': metadata, 'audits': audits, 'rows': rows, 'summary': summary}
    (folder / 'report.json').write_text(json.dumps(data, indent=2, allow_nan=False), encoding='utf-8')
    fields = ['asset_id', 'rate', 'buffer', 'engine', 'measurements', 'cpu_percent', 'reduction_percent',
              'p99_us', 'max_us', 'misses', 'allocations', 'releases', 'nonfinite', 'effective_taps', 'errors', 'rounds']
    with (folder / 'summary.csv').open('w', newline='', encoding='utf-8') as stream:
        writer = csv.DictWriter(stream, fieldnames=fields)
        writer.writeheader()
        writer.writerows(summary)
    failed = [a for a in audits if not a.get('pass') or a.get('error')]
    lines = ['# IR convolution comparison', '',
             'Standalone stereo engine costs, including float/double adapters; excludes the grid/plugin overhead. '
             'CPU % is wall-clock processing time divided by buffer duration, not total OS CPU.', '',
             f"Machine: {metadata['cpu']}; {metadata['platform']}.", '',
             f"Settings: {metadata['settings']}", '',
             f"Complete: {metadata['complete']}; {len(rows)} / {metadata['planned_measurements']} measured rows; "
             f"{len(audits)-len(failed)} / {len(audits)} numerical checks passed.", '',
             'Medians include all rounds/repeats. Each round shuffles engine order per matched asset/rate/buffer. '
             'The same prepared coefficients, current resampling/gain and 8192-tap cap are used. '
             'Each case runs in a fresh process. No outliers are removed.', '',
             '## Mean cost and callback tails', '',
             '| IR | Hz | Frames | Engine | CPU % | Reduction % | p99 µs | Worst µs | Misses | Allocations |',
             '|---|---:|---:|---|---:|---:|---:|---:|---:|---:|']
    for row in summary:
        if 'cpu_percent' not in row:
            lines.append(f"| {row['asset_id']} | {row['rate']} | {row['buffer']} | {row['engine']} | error | | | | | |")
            continue
        reduction = f"{row['reduction_percent']:.1f}" if 'reduction_percent' in row else 'n/a'
        lines.append(f"| {row['asset_id']} | {row['rate']} | {row['buffer']} | {row['engine']} | "
                     f"{row['cpu_percent']:.3f} | {reduction} | {row['p99_us']:.2f} | "
                     f"{row['max_us']:.1f} | {row['misses']} | {row['allocations']} |")
    lines += ['', '## Numerical checks', '',
              'Variable blocks include 1/3/7/13/31/32/64/128 frames (bounded by the configured maximum). '
              'Checks compare full random stereo excitation and tails against the actual direct engine, '
              'and offset impulse output against the prepared coefficients. '
              'Gates: <1e-5 FS maximum error, <1e-4 relative RMS error, no crosstalk, zero declared latency. '
              'Quiet-tail fixtures expose implicit coefficient trimming.', '',
              '| IR | Hz | Frames | Engine | Pass | Max error | Relative RMS error | Impulse error | Allocations |',
              '|---|---:|---:|---|---|---:|---:|---:|---:|']
    for row in audits:
        lines.append(f"| {row['asset_id']} | {row['rate']} | {row['buffer']} | {row['engine']} | {row.get('pass', False)} | "
                     f"{row.get('max_error', 0):.3g} | {row.get('relative_rms_error', 0):.3g} | "
                     f"{row.get('impulse_error', 0):.3g} | {row.get('allocations', 0)} |")
        if row.get('error'):
            lines.append(f"\nError: {row['error']}\n")
    lines += ['', '## Limits', '',
              'Windows x64 CPU comparison; macOS/ARM performance and actual DAW/driver xruns are untested. '
              'Two-stage FFT uses a 1024-frame tail partition on one thread; WDL uses zero allowed latency. '
              'Preparation primes 32768 zero samples offline to reserve lazy storage before timing. '
              'Global C++ allocations/deletions and WDL C malloc/realloc/free are instrumented; '
              'the callback clock and allocation counters are separate from output validation. '
              'These wrappers are an experiment, not a production IR loader integration.', '']
    (folder / 'report.md').write_text('\n'.join(lines), encoding='utf-8')
    return data


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--host', type=Path, default=ROOT / 'build-ir-comparison' / ('nam-ir-comparison.exe' if sys.platform == 'win32' else 'nam-ir-comparison'))
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--ir', type=Path, action='append', default=[])
    parser.add_argument('--rates', type=int, nargs='+', default=[44100, 48000, 96000])
    parser.add_argument('--buffers', type=int, nargs='+', default=[32, 64, 128])
    parser.add_argument('--lengths', type=int, nargs='+', default=[64, 256, 2048, 16384])
    parser.add_argument('--seconds', type=float, default=3.)
    parser.add_argument('--warmup', type=float, default=.5)
    parser.add_argument('--rounds', type=int, default=2)
    parser.add_argument('--repeats', type=int, default=3)
    args = parser.parse_args()
    if (not 0 < args.seconds <= 300 or not 0 < args.warmup <= 30 or args.rounds < 1 or args.repeats < 1
            or any(not 8000 <= rate <= 192000 for rate in args.rates)
            or any(buffer not in (32, 64, 128) for buffer in args.buffers)
            or any(not 1 <= length <= 262144 for length in args.lengths)):
        parser.error('Invalid duration, repeats, rates, buffers or lengths')
    folder = args.output.resolve()
    folder.mkdir(parents=True, exist_ok=False)
    assets = [{'asset_id': f'dense-{n}', 'asset': 'dense', 'length': n} for n in args.lengths]
    for index, path in enumerate(args.ir):
        assets.append({'asset_id': f'wav-{index}-{path.stem}', 'asset': 'wav', 'path': str(path.resolve()),
                       'sha256': sha256(path)})
    settings = {k: getattr(args, k) for k in ['rates', 'buffers', 'seconds', 'warmup', 'rounds', 'repeats']}
    metadata = {'generated_utc': datetime.now(timezone.utc).isoformat(), 'platform': platform.platform(),
                'cpu': platform.processor(), 'settings': settings, 'assets': assets,
                'host_sha256': sha256(args.host), 'fftconvolver_commit': 'f2cdeb04c42141d2caec19ca4f137398b2a76b85',
                'planned_measurements': len(assets)*len(args.rates)*len(args.buffers)*len(ENGINES)*args.rounds*args.repeats}
    build_info = subprocess.run([str(args.host), '--build-info'], capture_output=True, text=True)
    if build_info.returncode == 0:
        metadata['build_info'] = json.loads(build_info.stdout)
    metadata['source_commit'] = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip()
    audits, rows = [], []
    audit_assets = assets + [{'asset_id': 'quiet-tail', 'asset': 'quiet-tail', 'length': 8192},
                             {'asset_id': 'silence', 'asset': 'silence', 'length': 2048}]
    try:
        with (folder / 'raw.jsonl').open('w', encoding='utf-8') as raw:
            for asset, rate, buffer, engine in itertools.product(audit_assets, args.rates, args.buffers, ENGINES):
                row = worker(args.host, '--verify', dict(asset, rate=rate, buffer=buffer, engine=engine), folder)
                audits.append(row)
                raw.write(json.dumps(dict(kind='audit', **row))+'\n'); raw.flush()
            print(f"Audits: {sum(a.get('pass', False) for a in audits)}/{len(audits)} passed", flush=True)
            order = random.Random(1337)
            for round in range(1, args.rounds+1):
                for asset, rate, buffer in itertools.product(assets, args.rates, args.buffers):
                    engines = ENGINES.copy(); order.shuffle(engines)
                    for repeat in range(1, args.repeats+1):
                        for engine in engines:
                            config = dict(asset, rate=rate, buffer=buffer, engine=engine, round=round,
                                          repeat=repeat, seconds=args.seconds, warmup=args.warmup)
                            row = worker(args.host, '--measure', config, folder)
                            rows.append(row)
                            raw.write(json.dumps(dict(kind='measurement', **row))+'\n'); raw.flush()
                    print(f"Round {round}: {asset['asset_id']} {rate} Hz / {buffer} frames ({len(rows)} rows)", flush=True)
    finally:
        reports(folder, metadata, audits, rows)
    print(folder / 'report.md', flush=True)
    return int(any(r.get('error') or r.get('nonfinite') for r in rows))


if __name__ == '__main__':
    sys.exit(main())
