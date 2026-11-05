"""Offline content checks. This never opens serial ports or cloud services."""
from pathlib import Path
import hashlib, json, re

root=Path(__file__).resolve().parent.parent
errors=[]
normalized=json.loads((root/'manifests/line-ending-normalization.json').read_text(encoding='utf8'))
fixtures=json.loads((root/'manifests/vendor-test-fixtures.json').read_text(encoding='utf8'))
for name,digest in fixtures.items():
    assert hashlib.sha256((root/name).read_bytes()).hexdigest()==digest, 'vendor fixture changed: '+name
def check(ok,message):
    if not ok: errors.append(message)
for manifest in ('SHA256.json','manifests/SOURCE-SHA256.json'):
    entries=json.loads((root/manifest).read_text(encoding='utf8'))
    for name,digest in entries.items():
        p=root/name
        check(p.is_file() and hashlib.sha256(p.read_bytes()).hexdigest() in (digest,normalized.get(name)),'hash mismatch: '+name)
voice=root/'source/voice/main/boards/coco-voice-uart'
motor=root/'source/motor/Coco'
remote=root/'source/remote/WallERemote'
for name in ('coco_uart_protocol.h','coco_dialogue_protocol.h'):
    check(re.sub(rb'\s+',b'',(voice/name).read_bytes())==re.sub(rb'\s+',b'',(motor/name).read_bytes()),'protocol copies differ: '+name)
check((motor/'WallEProtocol.h').read_bytes()==(remote/'WallEProtocol.h').read_bytes(),'ESP-NOW protocol copies differ')
config=json.loads((voice/'config.json').read_text(encoding='utf8'))
options=config['builds'][0]['sdkconfig_append']
for option in ('CONFIG_CUSTOM_WAKE_WORD="hei kou kou"','CONFIG_CUSTOM_WAKE_WORD_DISPLAY="嘿Coco"','CONFIG_CUSTOM_WAKE_WORD_THRESHOLD=20','CONFIG_SEND_WAKE_WORD_DATA=n'):
    check(option in options,'wake config missing: '+option)
truth=(root/'CURRENT_STATE.md').read_text(encoding='utf8')
check(truth.startswith('THIS FILE IS THE CURRENT SOURCE OF TRUTH.'),'truth header absent')
check('## DO NOT USE' in truth,'DO NOT USE absent')
check('已刷入Coco-HeadWake-20261104版' in truth and '2～16为空' in truth,'deployment boundaries missing')
check('MAX_DUTY = 255' in (motor/'MotorConfig.h').read_text(encoding='utf8'),'duty changed')
own_docs=[root/x for x in ('README.md','CURRENT_STATE.md','TODO.md','DECISIONS.md','DEPRECATED.md','AGENTS.md')]
for folder in ('hardware','software','debug'):
    own_docs.extend((root/folder).glob('*.md'))
for doc in own_docs:
    for link in re.findall(r'\[[^\]]*\]\(([^)]+)\)',doc.read_text(encoding='utf8')):
        if '://' in link or link.startswith('#'):continue
        check((doc.parent/link.split('#')[0]).exists(),'broken doc link: '+str(doc.relative_to(root))+' -> '+link)
for p in root.rglob('*'):
    if not p.is_file() or '.git' in p.relative_to(root).parts:continue
    rel=p.relative_to(root)
    check(not any(x in ('private-backups','__pycache__','build') for x in rel.parts),'excluded directory: '+str(rel))
    check((not p.name.endswith(('.log','.pcm')) or rel.as_posix() in fixtures) and 'first64k' not in p.name and not p.name.endswith('-private.json'),'excluded private/debug file: '+str(rel))
    if p.suffix.lower() in ('.py','.js','.json','.yml','.yaml','.h','.hpp','.c','.cpp','.cc','.ino','.md','.txt','.pem','.key','.ps1','.sh','.conf','.ini'):
        data=p.read_bytes()
        for pattern in (rb'(?:gh[pousr]_[A-Za-z0-9]{30,}|github_pat_[A-Za-z0-9_]{50,})',rb'-----BEGIN (?:RSA |EC |OPENSSH )?PRIVATE KEY-----',rb'eyJ[A-Za-z0-9_-]{12,}\.[A-Za-z0-9_-]{16,}\.[A-Za-z0-9_-]{16,}'):
            check(not re.search(pattern,data) or rel.as_posix() in fixtures,'unreviewed credential-shaped content: '+str(rel))
    check(p.stat().st_size < 100*1024**2,'GitHub oversized file: '+str(rel))
print('Offline audit:', 'FAIL' if errors else 'PASS')
for error in errors: print(error)
raise SystemExit(bool(errors))
