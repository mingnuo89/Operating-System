#!/usr/bin/env python3
"""Check official tests; explicitly allow only the known QEMU loader patch."""
from pathlib import Path
import hashlib
import json
import sys

ROOT = Path(__file__).resolve().parents[1]
ORIGINAL_QEMU_LINE = (
    b'qemuopts="-machine virt -nographic -bios default '
    b'-device loader,file=bin/ucore.img,addr=0x80200000"'
)
COMPATIBLE_QEMU_LINE = (
    b'qemuopts="-machine virt -nographic -bios default -kernel bin/ucore.img"'
)


def digest(data):
    return hashlib.sha256(data).hexdigest()


def main():
    expected = json.loads(
        (ROOT / 'evidence/official-test-hashes.json').read_text()
    )
    success = True
    for name, original_digest in expected.items():
        try:
            data = (ROOT / name).read_bytes()
            if name.endswith('best_fit_pmm.c'):
                text = data.decode()
                text = text[
                    text.index('// Framework tests:'):
                    text.index('\nconst struct pmm_manager best_fit_pmm_manager')
                ].strip()
                data = text.encode()
        except (OSError, ValueError, UnicodeError) as error:
            print(f'FAIL {name}: {error}')
            success = False
            continue

        actual = digest(data)
        if actual == original_digest:
            print(f'PASS {name} {actual}')
            continue

        # Restore only this exact line, then require the full original hash.
        if name == 'tools/grade.sh' and data.count(COMPATIBLE_QEMU_LINE) == 1:
            restored = data.replace(COMPATIBLE_QEMU_LINE, ORIGINAL_QEMU_LINE, 1)
            if digest(restored) == original_digest:
                print(
                    f'PASS {name} {actual} '
                    '(known QEMU -kernel compatibility line only; '
                    'restored file matches original hash)'
                )
                continue

        print(f'FAIL {name} {actual}; expected original {original_digest}')
        success = False
    return 0 if success else 1


if __name__ == '__main__':
    sys.exit(main())
