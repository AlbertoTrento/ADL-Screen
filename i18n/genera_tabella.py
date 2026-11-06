# Trasforma i18n/testi.tsv in src/i18n_table.inc. Da lanciare dalla cartella del progetto.
import os

base = os.path.dirname(os.path.abspath(__file__))
seen = set()
rows = []
for n, line in enumerate(open(os.path.join(base, 'testi.tsv'), encoding='utf-8'), 1):
    line = line.rstrip('\r\n')
    if not line or line.startswith('#'):
        continue
    cols = line.split('\t')
    assert len(cols) == 6, (n, line)
    assert cols[0] not in seen, (n, cols[0])
    seen.add(cols[0])
    rows.append('{' + ', '.join('"%s"' % c for c in cols[:1]) + ', {' + ', '.join('"%s"' % c for c in cols[1:]) + '}},')

out = os.path.join(base, '..', 'src', 'i18n_table.inc')
with open(out, 'w', encoding='utf-8', newline='\n') as f:
    f.write('// Generato da i18n/genera_tabella.py: non modificare a mano.\n')
    f.write('\n'.join(rows) + '\n')
print(len(rows), 'testi')
