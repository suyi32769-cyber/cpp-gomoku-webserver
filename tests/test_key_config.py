import os
import pathlib
import shutil
import subprocess
import tempfile

root = pathlib.Path(__file__).resolve().parent.parent
with tempfile.TemporaryDirectory() as tmp:
    path = pathlib.Path(tmp)
    shutil.copyfile(root / 'start-linux.sh', path / 'start-linux.sh')
    (path / 'bin').mkdir()
    fake = path / 'bin' / 'webserver'
    fake.write_text('#!/bin/sh\n[ "${DEEPSEEK_API_KEY:-}" = "${EXPECTED_KEY:-}" ]\n')
    fake.chmod(0o700)
    env = os.environ.copy()
    env.pop('DEEPSEEK_API_KEY', None)
    env['EXPECTED_KEY'] = ''
    config = path / 'deepseek-key.local.txt'
    config.write_bytes(b'DEEPSEEK_API_KEY=PASTE_YOUR_API_KEY_HERE\r\n')
    subprocess.run(['bash', str(path / 'start-linux.sh')], env=env, check=True, stdout=subprocess.DEVNULL)
    config.write_bytes(b'\xef\xbb\xbfDEEPSEEK_API_KEY=test-key\r\n')
    env['EXPECTED_KEY'] = 'test-key'
    subprocess.run(['bash', str(path / 'start-linux.sh')], env=env, check=True, stdout=subprocess.DEVNULL)
    env['DEEPSEEK_API_KEY'] = 'provided-key'
    env['EXPECTED_KEY'] = 'provided-key'
    subprocess.run(['bash', str(path / 'start-linux.sh')], env=env, check=True, stdout=subprocess.DEVNULL)
print('PASS: placeholder, Windows UTF-8/CRLF, environment override')
