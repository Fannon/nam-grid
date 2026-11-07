import importlib.util
from pathlib import Path
import unittest

spec = importlib.util.spec_from_file_location('filename_vocabulary', Path(__file__).resolve().parents[1] / 'scripts/generate-filename-vocabulary.py')
vocabulary = importlib.util.module_from_spec(spec)
spec.loader.exec_module(vocabulary)


class FilenameVocabularyTests(unittest.TestCase):
    def test_ambiguous_aliases_do_not_override_canonical_names(self):
        registry = {'amp_vendors': {'Foo': {'code3': 'FOO'}, 'Bar': {'code3': 'BAR'}},
                    'amp_models': {'Foo Alpha': {'brand': 'Foo', 'model': 'Alpha', 'aliases': ['Shared']},
                                   'Bar Beta': {'brand': 'Bar', 'model': 'Beta', 'aliases': ['Shared', 'Foo Alpha']}}}
        text = vocabulary.generate(registry)
        self.assertNotIn('{"shared",', text)
        self.assertIn('{"foo alpha", "Foo Alpha", "Foo"}', text)
        self.assertEqual(text, vocabulary.generate(registry))

    def test_current_creator_codes_and_compact_model_names(self):
        text = vocabulary.generate({'nam_vendors': {'AmalgamAudio': {'code3': 'AA', 'kind': 'creator', 'aliases': ['AMG']}},
                                    'export': {'model_codes': {'VOX AC30 TB Super Twin': 'TB30'}}})
        self.assertIn('{"creator", "AA", "AmalgamAudio"}', text)
        self.assertIn('{"creator", "AMG", "AmalgamAudio"}', text)
        self.assertIn('{"model", "TB30", "VOX AC30 Top Boost Super Twin"}', text)

    def test_function_words_are_not_model_names(self):
        text = vocabulary.generate({'gain': {'categories': ['Clean']},
                                    'pedal_models': {'MXR Distortion+': {'brand': 'MXR', 'model': 'Distortion+', 'original_names': ['Distortion', 'Dist Plus']}}})
        self.assertNotIn('{"distortion", "MXR Distortion+", "MXR"}', text)
        self.assertIn('{"dist plus", "MXR Distortion+", "MXR"}', text)

    def test_ir_hints_keep_real_gear_and_drop_creator_blends(self):
        text = vocabulary.generate({'cab_standard': {'mics': {
            'SM57': {'desc': 'Shure SM57'}, 'R121': {'desc': 'Royer R-121', 'aliases': ['121']},
            '57+VR2': {'desc': 'SM57 + VR2 blend'}, 'Blend': {'desc': 'creator mic blend (2dor: SM57+SM58)'}}}},
            {'vendors': [], 'creators': [], 'hints': [], 'micCodes': [], 'categories': [], 'models': []})
        ir = text[text.index('irHints[]'):text.index('irMicCodes[]')]
        self.assertIn('{"mic/cab", "SM57", "Shure SM57"}', ir)
        self.assertIn('{"mic/cab", "R121", "Royer R-121"}', ir)
        self.assertNotIn('"121"', ir)
        self.assertNotIn('blend', ir.lower())


if __name__ == '__main__':
    unittest.main()
