"""Generate portable filename hints from a NAM Catalog name-registry.json."""
import argparse
import json
from pathlib import Path
import re


def normalized(value):
    return re.sub(r'[^a-z0-9]+', ' ', value.lower()).strip()


def model_label(value):
    # The catalog shortens this VOX channel name to keep filenames portable.
    return re.sub(r'\bTB\b', 'Top Boost', value)


def generate(registry):
    tables = {k: {} for k in ['vendors', 'creators', 'settings', 'gains', 'compactModels']}
    def add(table, key, token, label):
        if token:
            tables[table][(key, normalized(token))] = (key, token, label)
    vendor_codes = {}
    for section in ['amp_vendors', 'pedal_vendors', 'preamp_vendors']:
        for name, item in registry.get(section, {}).items():
            if name == 'Unknown':
                continue
            aliases = [name, item.get('code3'), item.get('filename_name'), *item.get('aliases', [])]
            vendor_codes.setdefault(name, set()).update(a for a in aliases if a)
            for alias in aliases:
                add('vendors', 'vendor', alias, name)
    for name, code in registry.get('export', {}).get('make_codes', {}).items():
        add('vendors', 'vendor', code, name)
    for code, name in [('HWTT', 'Hiwatt'), ('CRR', 'Carr'), ('MAGN', 'Magnatone')]:
        add('vendors', 'vendor', code, name)
    for name, item in registry.get('nam_vendors', {}).items():
        for alias in [name, item.get('code3'), *item.get('aliases', [])]:
            add('creators', 'platform' if item.get('kind') == 'platform' else 'creator', alias, name)
    categories = {'channel_voice': 'channel', 'eq': 'eq', 'pedal': 'chain', 'knob': 'settings', 'other': 'settings', 'mic_cab': 'mic/cab'}
    # Cab-standard entries supply richer typed labels than the flat token index.
    typed = {token for section in ['cabs', 'cab_variants', 'speakers', 'mics', 'positions']
             for token in registry.get('cab_standard', {}).get(section, {})}
    for section, values in registry.get('setting_tokens', {}).items():
        for token, label in values.items():
            if section in ['mic_cab', 'knob'] and token in typed:
                continue
            add('settings', categories[section], token, label[0].upper() + label[1:])
    for section, key in [('cabs', 'mic/cab'), ('cab_variants', 'mic/cab'), ('speakers', 'speaker'), ('mics', 'mic/cab'), ('positions', 'mic/cab')]:
        for token, item in registry.get('cab_standard', {}).get(section, {}).items():
            for alias in [token, *item.get('aliases', [])]:
                add('settings', key, alias, item['desc'][0].upper() + item['desc'][1:])
    # Common IR spellings retained alongside the current NAM naming convention.
    for token, label in [('G12H30', 'Celestion G12H30'), ('G12M65', 'Celestion G12M-65 Creamback'),
                         ('G12-65', 'Celestion G12-65'), ('EV12L', 'Electro-Voice EVM12L'),
                         ('C12N', 'Jensen C12N'), ('P12R', 'Jensen P12R'), ('WGS', 'Warehouse Guitar Speakers')]:
        add('settings', 'speaker', token, label)
    for token, label in [('Mesa2x12 V30', 'Mesa 2x12 with Vintage 30'), ('Marshall4x12 M25', 'Marshall 4x12 with G12M25'),
                         ('Top Boost', 'Top-boost channel'), ('Texas Flood', 'Creator Texas Flood setting'), ('El Mocambo', 'Creator El Mocambo setting')]:
        add('settings', 'channel' if token == 'Top Boost' else 'settings' if ' ' in token and token in ['Texas Flood', 'El Mocambo'] else 'mic/cab', token, label)
    for token, label in registry.get('gain', {}).get('aliases', {}).items():
        add('gains', 'gain', token, 'Edge of breakup' if label == 'Edge' else label)
    for value in registry.get('gain', {}).get('categories', []):
        add('gains', 'gain', value, 'Edge of breakup' if value == 'Edge' else value)
    for value in ['Boost', 'Overdrive', 'Distortion', 'Fuzz', 'Compression', 'Saturation', 'Color']:
        add('gains', 'gain', value, value)
    for name, token in registry.get('export', {}).get('model_codes', {}).items():
        add('compactModels', 'model', token, model_label(name))
    models, originals = {}, {}
    def model_alias(target, token, label, vendor, priority):
        token = normalized(token)
        if len(token) < 3:
            return
        previous = target.get(token)
        if previous is None or priority < previous[0]:
            target[token] = (priority, {(model_label(label), vendor)})
        elif priority == previous[0]:
            previous[1].add((model_label(label), vendor))
    identities = {}
    for section in ['amp_models', 'pedal_models', 'preamp_models']:
        for name, item in registry.get(section, {}).items():
            brand, model = item['brand'], item['model']
            identities[name] = (name, brand)
            model_alias(models, name, name, brand, 0)
            for code in vendor_codes.get(brand, [brand]):
                model_alias(models, code + ' ' + model, name, brand, 1)
                model_alias(models, code + ' ' + model_label(model), name, brand, 1)
            for alias in item.get('aliases', []):
                model_alias(models, alias, name, brand, 2)
            for alias in item.get('original_names', []):
                model_alias(originals, alias, name, brand, 0)
    for code, item in registry.get('dual_amps', {}).items():
        label = item['amp1'] + ' + ' + item['amp2']
        for token in [code, label, *item.get('aliases', [])]:
            model_alias(models, token, label, '', 0)
    for alias, name in registry.get('alias_index', {}).items():
        if name in identities:
            label, vendor = identities[name]
            model_alias(originals, alias, label, vendor, 1)
    q = lambda value: json.dumps(value, ensure_ascii=True)
    lines = ['#pragma once', '// Generated by scripts/generate-filename-vocabulary.py from the NAM Catalog registry.',
             '// Hints describe names, never verified equipment or changes to the captured sound.',
             'namespace nam_library { namespace filename_hints {',
             'struct Alias { const char* key; const char* token; const char* label; };']
    for name, entries in tables.items():
        lines.append('inline constexpr Alias ' + name + '[] = {')
        for entry in sorted(entries.values(), key=lambda row: (row[0], normalized(row[1]))):
            lines.append('  {' + ', '.join(q(v) for v in entry) + '},')
        lines.append('};')
    lines.append('struct ModelAlias { const char* token; const char* label; const char* vendor; };')
    for name, entries in [('models', models), ('originalModels', originals)]:
        lines.append('inline constexpr ModelAlias ' + name + '[] = {')
        for token, (_, candidates) in sorted(entries.items(), key=lambda pair: (-len(pair[0]), pair[0])):
            if len(candidates) == 1:
                label, vendor = next(iter(candidates))
                lines.append('  {' + ', '.join(q(v) for v in [token, label, vendor]) + '},')
        lines.append('};')
    return '\n'.join(lines + ['} } // namespace nam_library::filename_hints', ''])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('registry', type=Path)
    parser.add_argument('--output', type=Path, default=Path(__file__).resolve().parents[1] / 'NeuralAmpModeler/FilenameHintVocabulary.h')
    args = parser.parse_args()
    registry = json.loads(args.registry.read_text(encoding='utf-8-sig'))
    if registry.get('schema_version') != 1:
        parser.error('expected NAM Catalog registry schema_version 1')
    content = generate(registry)
    with args.output.open('w', encoding='utf-8', newline='\n') as output:
        output.write(content)


if __name__ == '__main__':
    main()
