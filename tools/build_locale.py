"""Compile locale sources without changing resource identifiers or source files."""
from pathlib import Path
import re

root = Path(__file__).resolve().parents[1]
header = (root / 'src/resource.h').read_text(encoding='utf-8-sig')
resource = (root / 'src/resource.rc').read_text(encoding='utf-8-sig')
ids = {key: int(value) for key, value in re.findall(r'^#define\s+(IDS_\w+)\s+(\d+)', header, re.M)}
defaults = dict(re.findall(r'^\s*(IDS_\w+)\s+"(.*)"\s*$', resource, re.M))
previous = (root / 'bin/memreduct.lng').read_text(encoding='utf-16')
timestamp = re.search(r'^000=(\d+)$', previous, re.M)
output = ['; memreduct', '; Built by tools/build_locale.py; edit bin/i18n/*.ini instead.', '']
for path in sorted((root / 'bin/i18n').glob('*.ini')):
    source = path.read_text(encoding='utf-16')
    output.extend(line for line in source.splitlines() if line.startswith(';'))
    output.append(f'[{path.stem}]')
    if path.stem == 'Russian' and timestamp:
        output.append('000=' + timestamp.group(1))
    for line in source.splitlines():
        key, delimiter, value = line.partition('=')
        if delimiter and key in ids and value and value != defaults.get(key):
            output.append(f'{ids[key]:03}={value}')
    output.append('')
(root / 'bin/memreduct.lng').write_text('\n'.join(output), encoding='utf-16', newline='\r\n')
print('Built bin/memreduct.lng with stable resource identifiers.')
