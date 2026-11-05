"""Desktop wrapper of Coco's verified current-voice capture workflow.

The cloud does not expose a direct TTS API here: Windows speaks a request to
the bound assistant, whose exact reply and Opus audio are captured and checked.
Tokens stay in memory. This module never opens a robot serial port.
"""
import argparse
import io
import json
import os
from pathlib import Path
import re
import struct
import subprocess
import tempfile
import threading
import time
import uuid
import wave

import av
import requests
import websocket

DEFAULT_DEVICE = "84:c7:bb:5a:ae:dc"
DEFAULT_CLIENT = "8d67e2f5-940f-4d4e-8ad2-ed80d0274f1d"
OTA_URL = "https://api.tenclass.net/xiaozhi/ota/"


class Cancelled(Exception):
    pass


def normalize(text):
    # Ignore punctuation/spacing, as in the existing release tool.
    return "".join(c for c in text if c.isalnum())


def check_cancel(cancel):
    if cancel.is_set():
        raise Cancelled("已取消")


def request_wav(text, directory, cancel):
    path = directory / "request.json"
    path.write_text(json.dumps({"text": "请你只说下面这句话，不要添加其他内容：" + text}, ensure_ascii=False), encoding="utf8")
    output = directory / "request.wav"
    cmd = ["powershell.exe", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File",
           str(Path(__file__).with_name("speak_request.ps1")), "-InputPath", str(path), "-OutputPath", str(output)]
    process = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                               creationflags=subprocess.CREATE_NO_WINDOW)
    while process.poll() is None:
        if cancel.wait(.1):
            process.kill()
            process.communicate()
            raise Cancelled("已取消")
    _, error = process.communicate()
    if process.returncode:
        # Never echo a service credential or arbitrary HTTP response in errors.
        raise RuntimeError("Windows中文语音请求生成失败，请确认已安装 Microsoft Huihui Desktop 中文语音。")
    return output


def connection_config(device, client):
    if not re.fullmatch(r"(?:[0-9a-fA-F]{2}:){5}[0-9a-fA-F]{2}", device):
        raise ValueError("设备 MAC 格式应为 84:c7:bb:5a:ae:dc")
    uuid.UUID(client)
    try:
        response = requests.post(OTA_URL, headers={
            "Device-Id": device, "Client-Id": client, "Activation-Version": "1",
            "User-Agent": "coco-voice-uart/2.5.2"}, json={
                "version": 2, "application": {"name": "xiaozhi", "version": "2.5.2"},
                "board": {"type": "coco-voice-uart", "mac": device}}, timeout=(10, 25))
        response.raise_for_status()
        config = response.json()
    except (requests.RequestException, ValueError):
        raise RuntimeError("获取小智服务连接失败，请检查网络后重试。") from None
    if config.get("activation"):
        raise RuntimeError("该设备需要由主人先完成小智绑定；本工具不会绕过绑定。")
    cfg = config.get("websocket", {})
    if not isinstance(cfg.get("url"), str) or not cfg["url"].startswith("wss://"):
        raise RuntimeError("服务器未提供可用的安全语音连接。")
    return cfg


def capture(text, wav, cfg, device, client, cancel, progress):
    headers = {"Device-Id": device, "Client-Id": client, "Protocol-Version": "1"}
    token = cfg.get("token", "")
    if token:
        headers["Authorization"] = token if " " in token else "Bearer " + token
    try:
        ws = websocket.create_connection(cfg["url"], header=headers, timeout=12)
    except Exception:
        raise RuntimeError("语音连接失败，请检查网络或设备绑定状态。") from None
    try:
        ws.send(json.dumps({"type": "hello", "version": 1, "transport": "websocket", "features": {"mcp": False},
                            "audio_params": {"format": "opus", "sample_rate": 16000, "channels": 1, "frame_duration": 60}}))
        hello = json.loads(ws.recv())
        if hello.get("type") != "hello":
            raise RuntimeError("服务器未完成语音握手。")
        params = hello.get("audio_params", {})
        if params.get("format", "opus") != "opus" or params.get("channels", 1) != 1:
            raise RuntimeError("服务器返回的音频格式不受支持。")
        rate = int(params.get("sample_rate", 24000))
        duration = int(params.get("frame_duration", 60))
        if rate not in (8000, 12000, 16000, 24000, 48000) or duration not in (10, 20, 40, 60):
            raise RuntimeError("服务器返回的音频参数不受支持。")
        session = hello.get("session_id", "")
        ws.send(json.dumps({"session_id": session, "type": "listen", "state": "start", "mode": "manual"}))
        encoder = av.CodecContext.create("libopus", "w")
        encoder.sample_rate = 16000
        encoder.layout = "mono"
        encoder.format = "s16"
        encoder.bit_rate = 24000
        encoder.options = {"frame_duration": "60", "application": "voip"}
        encoder.open()
        progress("正在发送台词请求…")
        with wave.open(str(wav), "rb") as source:
            pos = 0
            started = time.monotonic()
            while True:
                check_cancel(cancel)
                pcm = source.readframes(960)
                if not pcm:
                    break
                frame = av.AudioFrame(format="s16", layout="mono", samples=960)
                frame.planes[0].update(pcm.ljust(1920, b"\0"))
                frame.sample_rate = 16000
                frame.pts = pos
                pos += 960
                for packet in encoder.encode(frame):
                    ws.send_binary(bytes(packet))
                cancel.wait(max(0, started + pos / 16000 - time.monotonic()))
            for packet in encoder.encode(None):
                ws.send_binary(bytes(packet))
        ws.send(json.dumps({"session_id": session, "type": "listen", "state": "stop"}))
        progress("等待服务器按当前音色生成…")
        ws.settimeout(1)
        deadline = time.monotonic() + 100
        packets, sentences = [], []
        stopped = False
        total = 0
        while time.monotonic() < deadline:
            check_cancel(cancel)
            try:
                message = ws.recv()
            except websocket.WebSocketTimeoutException:
                continue
            if not message:
                raise RuntimeError("服务器提前关闭连接，请重试。")
            if isinstance(message, bytes):
                total += len(message)
                if total > 12 * 1024 * 1024:
                    raise RuntimeError("返回音频超过单段限制，请缩短台词。")
                packets.append(message)
                continue
            obj = json.loads(message)
            if obj.get("type") == "tts":
                if obj.get("state") == "sentence_start":
                    sentences.append(obj.get("text", ""))
                if obj.get("state") == "stop":
                    stopped = True
                    break
        if not stopped:
            raise RuntimeError("等待音频超时，请缩短台词或重试。")
        actual = "".join(sentences)
        if normalize(actual) != normalize(text):
            progress("回复与台词不同，仍将保存服务器实际音频。")
        if not packets:
            raise RuntimeError("没有收到语音音频。")
        return packets, rate, duration, actual
    except (Cancelled, RuntimeError):
        raise
    except Exception:
        raise RuntimeError("语音会话中断，请检查网络后重试。") from None
    finally:
        ws.close()


def ogg_crc(data):
    value = 0
    for byte in data:
        value ^= byte << 24
        for _ in range(8):
            value = ((value << 1) ^ (0x04c11db7 if value & 0x80000000 else 0)) & 0xffffffff
    return value


def make_ogg(packets, rate, duration):
    # Match the robot's existing raw-Opus packaging, with one packet per page.
    head = b"OpusHead" + struct.pack("<BBHIhB", 1, 1, 0, rate, 0, 0)
    tags = b"OpusTags" + struct.pack("<I", 4) + b"Coco" + struct.pack("<I", 0)
    output = bytearray()
    serial = uuid.uuid4().int & 0xffffffff
    for i, packet in enumerate([head, tags] + packets):
        segments = bytes([255] * (len(packet) // 255) + [len(packet) % 255])
        if len(segments) > 255:
            raise RuntimeError("音频包过大。")
        flags = (2 if i == 0 else 0) | (4 if i == len(packets) + 1 else 0)
        granule = max(0, i - 1) * 48 * duration
        page = bytearray(b"OggS" + struct.pack("<BBQIIIB", 0, flags, granule, serial, i, 0, len(segments)) + segments + packet)
        struct.pack_into("<I", page, 22, ogg_crc(page))
        output.extend(page)
    return bytes(output)


def wav_bytes(ogg):
    output = io.BytesIO()
    with av.open(io.BytesIO(ogg), format="ogg") as source, wave.open(output, "wb") as dest:
        dest.setnchannels(1)
        dest.setsampwidth(2)
        dest.setframerate(24000)
        resampler = av.AudioResampler(format="s16", layout="mono", rate=24000)
        frames = 0
        for frame in source.decode(audio=0):
            for converted in resampler.resample(frame):
                dest.writeframes(bytes(converted.planes[0])[:converted.samples * 2])
                frames += converted.samples
        for converted in resampler.resample(None):
            dest.writeframes(bytes(converted.planes[0])[:converted.samples * 2])
            frames += converted.samples
        if not frames:
            raise RuntimeError("生成的音频无法解码。")
    return output.getvalue()


def generate(text, output, name, device=DEFAULT_DEVICE, client=DEFAULT_CLIENT,
             cancel=None, progress=lambda _: None):
    cancel = cancel or threading.Event()
    text = text.strip()
    if not normalize(text) or len(text) > 300:
        raise ValueError("请填写1～300字台词；长段落请分段生成。")
    name = name.strip()
    if not re.fullmatch(r"[A-Za-z0-9_\-\u4e00-\u9fff]{1,80}", name):
        raise ValueError("文件名请用汉字、字母、数字、下划线或短横线，最多80字。")
    if name.upper() in {'CON', 'PRN', 'AUX', 'NUL', *(f'COM{i}' for i in range(1, 10)), *(f'LPT{i}' for i in range(1, 10))}:
        raise ValueError("这个名称是Windows保留名称，请换一个文件名。")
    output = Path(output).expanduser().resolve()
    output.mkdir(parents=True, exist_ok=True)
    if any((output / (name + ext)).exists() for ext in (".ogg", ".wav", ".json")):
        raise ValueError("这个文件名已经存在，请换名，已有音频不会覆盖。")
    progress("正在连接自己的小智服务…")
    check_cancel(cancel)
    cfg = connection_config(device.lower().strip(), client.strip())
    check_cancel(cancel)
    with tempfile.TemporaryDirectory(prefix="coco-tts-") as directory:
        progress("正在准备台词请求…")
        wav = request_wav(text, Path(directory), cancel)
        packets, rate, duration, actual = capture(text, wav, cfg, device.lower().strip(), client.strip(), cancel, progress)
    check_cancel(cancel)
    progress("核对台词、解码并保存…")
    ogg = make_ogg(packets, rate, duration)
    preview = wav_bytes(ogg)
    metadata = json.dumps({"requested_text": text, "returned_text": actual,
                           "text_matches": normalize(actual) == normalize(text),
                           "text_check": "informational only; punctuation and whitespace ignored; differing replies are saved",
                           "voice": "bound XiaoZhi service current voice at generation time",
                           "generated_at": time.strftime("%Y-%m-%dT%H:%M:%S%z"),
                           "sample_rate": rate, "frame_duration_ms": duration,
                           "wav_sample_rate": 24000}, ensure_ascii=False, indent=2).encode("utf8")
    # Reserve each path exclusively; on any failure remove only files we created.
    created = []
    try:
        for ext, data in ((".ogg", ogg), (".wav", preview), (".json", metadata)):
            check_cancel(cancel)
            path = output / (name + ext)
            with path.open("xb") as f:
                created.append(path)
                f.write(data)
    except BaseException:
        for path in created:
            path.unlink(missing_ok=True)
        raise
    return output / (name + ".wav")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("text")
    parser.add_argument("--output", required=True)
    parser.add_argument("--name", required=True)
    parser.add_argument("--device", default=DEFAULT_DEVICE)
    parser.add_argument("--client", default=DEFAULT_CLIENT)
    args = parser.parse_args()
    print(generate(args.text, args.output, args.name, args.device, args.client, progress=print))
