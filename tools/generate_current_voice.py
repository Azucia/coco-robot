import json, pathlib, struct, time, websocket, wave, av, argparse, requests, subprocess

ROOT = pathlib.Path(__file__).resolve().parent.parent
parser=argparse.ArgumentParser(description='Render exact local phrases with your own bound XiaoZhi device voice.')
parser.add_argument('--device-id',required=True)
parser.add_argument('--client-id',required=True)
parser.add_argument('--output',type=pathlib.Path,default=ROOT/'source/voice/main/boards/coco-voice-uart/sounds')
args=parser.parse_args()
subprocess.run(['powershell.exe','-NoProfile','-ExecutionPolicy','Bypass','-File',str(ROOT/'tools/render_sapi_requests.ps1')],check=True)
response=requests.post('https://api.tenclass.net/xiaozhi/ota/',headers={'Device-Id':args.device_id,'Client-Id':args.client_id,'Activation-Version':'1','User-Agent':'coco-voice-uart/2.5.2'},json={'version':2,'application':{'name':'xiaozhi','version':'2.5.2'},'board':{'type':'coco-voice-uart','mac':args.device_id}},timeout=30)
response.raise_for_status()
secret={'uuid':args.client_id,'ota':response.json()}
if secret['ota'].get('activation'): raise RuntimeError('This device must be bound by its owner before voice rendering')

cfg = secret['ota']['websocket']
destination = args.output
destination.mkdir(exist_ok=True)

def crc(data):
    value = 0
    for b in data:
        value ^= b << 24
        for _ in range(8):
            value = ((value << 1) ^ (0x04c11db7 if value & 0x80000000 else 0)) & 0xffffffff
    return value

def ogg(packets, rate, duration):
    output = bytearray()
    head = b'OpusHead' + struct.pack('<BBHIhB', 1, 1, 0, rate, 0, 0)
    tags = b'OpusTags' + struct.pack('<I', 4) + b'Coco' + struct.pack('<I', 0)
    all_packets = [head, tags] + packets
    for i, packet in enumerate(all_packets):
        segments = bytes([255] * (len(packet)//255) + [len(packet)%255])
        flags = (2 if i == 0 else 0) | (4 if i == len(all_packets)-1 else 0)
        granule = max(0, i-1) * 48 * duration
        page = bytearray(b'OggS' + struct.pack('<BBQIIIB', 0, flags, granule, 0x434f434f, i, 0, len(segments)) + segments + packet)
        struct.pack_into('<I', page, 22, crc(page))
        output.extend(page)
    return output

for name, phrase in json.loads((ROOT/'tools/coco_speech_lines.json').read_text(encoding='utf8')).items():
    if (destination/(name+'.ogg')).exists(): continue
    token = cfg.get('token','')
    if token and ' ' not in token: token = 'Bearer ' + token
    headers = {'Device-Id':args.device_id, 'Client-Id':secret['uuid'], 'Protocol-Version':'1'}
    if token: headers['Authorization'] = token
    ws = websocket.create_connection(cfg['url'], header=headers, timeout=30)
    packets, sentences = [], []
    try:
        ws.send(json.dumps({'type':'hello','version':1,'transport':'websocket','features':{'mcp':False},'audio_params':{'format':'opus','sample_rate':16000,'channels':1,'frame_duration':60}}))
        hello = json.loads(ws.recv())
        if hello.get('type')!='hello': raise RuntimeError('No server hello: '+str(hello.get('type')))
        rate=hello.get('audio_params',{}).get('sample_rate',24000)
        duration=hello.get('audio_params',{}).get('frame_duration',60)
        ws.send(json.dumps({'session_id':hello.get('session_id',''), 'type':'listen','state':'start','mode':'manual'}))
        encoder=av.CodecContext.create('libopus','w')
        encoder.sample_rate=16000;encoder.layout='mono';encoder.format='s16';encoder.bit_rate=24000
        encoder.options={'frame_duration':'60','application':'voip'};encoder.open()
        with wave.open(str(ROOT/'tools'/(name+'-request.wav')),'rb') as wav:
            pos=0
            while True:
                pcm=wav.readframes(960)
                if not pcm: break
                pcm=pcm.ljust(1920,b'\0')
                frame=av.AudioFrame(format='s16',layout='mono',samples=960)
                frame.planes[0].update(pcm);frame.sample_rate=16000;frame.pts=pos;pos+=960
                for packet in encoder.encode(frame): ws.send_binary(bytes(packet))
                time.sleep(.06)
            for packet in encoder.encode(None):ws.send_binary(bytes(packet))
        ws.send(json.dumps({'session_id':hello.get('session_id',''), 'type':'listen','state':'stop'}))
        deadline=time.monotonic()+60
        while time.monotonic()<deadline:
            message=ws.recv()
            if isinstance(message,bytes): packets.append(message); continue
            obj=json.loads(message)
            if obj.get('type')=='stt': print('request transcription:',obj.get('text'),flush=True)
            if obj.get('type')=='tts':
                if obj.get('state')=='sentence_start': sentences.append(obj.get('text',''))
                if obj.get('state')=='stop': break
        print(name,'text=',sentences,'packets=',len(packets),flush=True)
        if not packets: raise RuntimeError('No speech audio received')
        normalize=lambda x: ''.join(c for c in x if c.isalnum())
        if normalize(''.join(sentences)) != normalize(phrase): raise RuntimeError('Server changed the requested phrase; refusing to package')
        (destination/(name+'.ogg')).write_bytes(ogg(packets,rate,duration))
    finally:
        ws.close()
