import importlib.util
from pathlib import Path
import unittest

spec = importlib.util.spec_from_file_location('ir_comparison', Path(__file__).parents[1] / 'scripts/ir-convolution-comparison.py')
benchmark = importlib.util.module_from_spec(spec)
spec.loader.exec_module(benchmark)


def row(engine, cpu):
    return {'asset_id': 'cab', 'rate': 96000, 'buffer': 32, 'engine': engine, 'round': 1,
            'timing': {'cpu_percent': cpu, 'p99_us': 10., 'max_us': 20., 'deadline_misses': 0},
            'allocations': 0, 'releases': 0, 'nonfinite': 0, 'effective_taps': 8192}


class ComparisonTests(unittest.TestCase):
    def test_median_cost_is_not_selected_from_fastest_repeat(self):
        values = [row('direct', n) for n in [10., 11., 100.]] + [row('wdl-div', n) for n in [2., 2.2, 80.]]
        result = benchmark.summarize(values)
        wdl = next(r for r in result if r['engine'] == 'wdl-div')
        self.assertAlmostEqual(wdl['reduction_percent'], 80.)
        self.assertEqual(wdl['cpu_percent'], 2.2)

    def test_partial_run_has_no_improvement_without_reference(self):
        result = benchmark.summarize([row('wdl-div', 2.)])
        self.assertNotIn('reduction_percent', result[0])

    def test_failed_or_nonfinite_results_never_claim_an_improvement(self):
        failed = row('wdl-div', 2.)
        failed['error'] = 'worker crashed during teardown'
        result = benchmark.summarize([row('direct', 10.), failed])
        self.assertNotIn('reduction_percent', next(r for r in result if r['engine'] == 'wdl-div'))
        failed.pop('error')
        failed['nonfinite'] = 1
        result = benchmark.summarize([row('direct', 10.), failed])
        self.assertNotIn('reduction_percent', next(r for r in result if r['engine'] == 'wdl-div'))


if __name__ == '__main__':
    unittest.main()
