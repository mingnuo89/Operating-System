#!/usr/bin/env python3
"""Compare protected tests with hashes recorded from the official ZIP."""
from pathlib import Path
import hashlib, json, sys
root=Path(__file__).resolve().parents[1]
expected=json.loads((root/'evidence/official-test-hashes.json').read_text())
results=[]
for name,digest in expected.items():
    data=(root/name).read_bytes()
    if name.endswith('best_fit_pmm.c'):
        text=data.decode()
        text=text[text.index('// Framework tests:'):text.index('\nconst struct pmm_manager best_fit_pmm_manager')].strip()
        data=text.encode()
    actual=hashlib.sha256(data).hexdigest()
    ok=actual==digest
    results.append(ok)
    print(('PASS' if ok else 'FAIL')+' '+name+' '+actual)
sys.exit(0 if all(results) else 1)
