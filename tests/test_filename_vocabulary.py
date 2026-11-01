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


if __name__ == '__main__':
    unittest.main()
