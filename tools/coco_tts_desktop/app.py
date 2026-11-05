import json
import os
from pathlib import Path
import queue
import sys
import threading
import time
import tkinter as tk
from tkinter import filedialog, messagebox, ttk
import winsound

sys.dont_write_bytecode = True
from engine import DEFAULT_CLIENT, DEFAULT_DEVICE, Cancelled, generate


class App:
    def __init__(self, root):
        self.root = root
        root.title('Coco 台词生成器')
        root.geometry('790x650')
        root.minsize(660, 560)
        self.events = queue.Queue()
        self.worker = None
        self.cancel = threading.Event()
        self.last_wav = None
        self.closing = False
        self.settings_file = Path(os.getenv('LOCALAPPDATA', str(Path.home()))) / 'CocoTTS/settings.json'
        try:
            settings = json.loads(self.settings_file.read_text(encoding='utf8'))
        except (OSError, ValueError):
            settings = {}
        self.device = tk.StringVar(value=settings.get('device', DEFAULT_DEVICE))
        self.client = tk.StringVar(value=settings.get('client', DEFAULT_CLIENT))
        self.output = tk.StringVar(value=settings.get('output', str(Path.home() / 'Desktop/Coco台词')))
        self.name = tk.StringVar(value=self.new_name())
        self.status = tk.StringVar(value='填写台词后点击“生成语音”。需要联网，请让机器人暂时保持待机。')
        self.auto_play = tk.BooleanVar(value=False)
        style = ttk.Style()
        style.theme_use('vista')
        style.configure('TLabel', font=('Microsoft YaHei UI', 10))
        frame = ttk.Frame(root, padding=18)
        frame.pack(fill='both', expand=True)
        ttk.Label(frame, text='Coco 台词生成器', font=('Microsoft YaHei UI', 19, 'bold')).pack(anchor='w')
        ttk.Label(frame, text='使用已绑定小智服务的当前音色 · 导出 OGG + WAV', foreground='#526176').pack(anchor='w', pady=(4, 14))
        ttk.Label(frame, text='台词（最多300字；每次生成一段）').pack(anchor='w')
        self.text = tk.Text(frame, height=7, font=('Microsoft YaHei UI', 12), wrap='word', undo=True)
        self.text.pack(fill='both', expand=True, pady=(6, 10))
        self.text.insert('1.0', '我来自烟台理工学院。')
        row = ttk.Frame(frame)
        row.pack(fill='x', pady=4)
        ttk.Label(row, text='文件名').pack(side='left')
        ttk.Entry(row, textvariable=self.name).pack(side='left', fill='x', expand=True, padx=8)
        ttk.Button(row, text='自动命名', command=lambda: self.name.set(self.new_name())).pack(side='left')
        row = ttk.Frame(frame)
        row.pack(fill='x', pady=4)
        ttk.Label(row, text='输出到').pack(side='left')
        ttk.Entry(row, textvariable=self.output).pack(side='left', fill='x', expand=True, padx=8)
        ttk.Button(row, text='选择文件夹', command=self.pick_folder).pack(side='left')
        advanced = ttk.LabelFrame(frame, text='设备设置（本机已预填；仅使用自己的已绑定设备）', padding=8)
        advanced.pack(fill='x', pady=10)
        ttk.Label(advanced, text='设备 MAC').grid(row=0, column=0, sticky='w')
        ttk.Entry(advanced, textvariable=self.device, width=22).grid(row=0, column=1, sticky='ew', padx=8)
        ttk.Label(advanced, text='客户端 UUID').grid(row=1, column=0, sticky='w', pady=(5, 0))
        ttk.Entry(advanced, textvariable=self.client).grid(row=1, column=1, sticky='ew', padx=8, pady=(5, 0))
        advanced.columnconfigure(1, weight=1)
        row = ttk.Frame(frame)
        row.pack(fill='x', pady=(0, 8))
        self.start_button = ttk.Button(row, text='生成语音', command=self.start)
        self.start_button.pack(side='left')
        self.cancel_button = ttk.Button(row, text='取消', command=self.cancel.set, state='disabled')
        self.cancel_button.pack(side='left', padx=6)
        self.play_button = ttk.Button(row, text='试听', command=self.play, state='disabled')
        self.play_button.pack(side='left')
        ttk.Button(row, text='停止试听', command=self.stop_audio).pack(side='left', padx=6)
        ttk.Button(row, text='打开文件夹', command=self.open_folder).pack(side='left')
        ttk.Checkbutton(row, text='生成后试听', variable=self.auto_play).pack(side='right')
        self.bar = ttk.Progressbar(frame, mode='indeterminate')
        self.bar.pack(fill='x', pady=(0, 8))
        ttk.Label(frame, textvariable=self.status, wraplength=720).pack(anchor='w')
        ttk.Label(frame, text='回复没有逐字复述也会保存；台词记录保留实际回答。结果不会自动刷入机器人。',
                  foreground='#526176', wraplength=720).pack(anchor='w', pady=(10, 0))
        root.protocol('WM_DELETE_WINDOW', self.close)
        root.after(100, self.poll)

    @staticmethod
    def new_name():
        return 'coco_line_' + time.strftime('%Y%m%d_%H%M%S')

    def pick_folder(self):
        result = filedialog.askdirectory(parent=self.root, title='选择音频保存位置')
        if result:
            self.output.set(result)

    def save_settings(self, snapshot):
        self.settings_file.parent.mkdir(parents=True, exist_ok=True)
        self.settings_file.write_text(json.dumps(snapshot, ensure_ascii=False, indent=2), encoding='utf8')

    def start(self):
        if self.worker and self.worker.is_alive():
            return
        text = self.text.get('1.0', 'end').strip()
        if not text or len(text) > 300:
            messagebox.showerror('请检查台词', '请填写1～300字台词。', parent=self.root)
            return
        snapshot = {'device': self.device.get().strip(), 'client': self.client.get().strip(), 'output': self.output.get().strip()}
        if not snapshot['output']:
            messagebox.showerror('请检查输出目录', '请选择保存文件夹。', parent=self.root)
            return
        try:
            self.save_settings(snapshot)
        except OSError:
            messagebox.showerror('无法保存设置', '请检查电脑的用户文件夹是否可写。', parent=self.root)
            return
        name = self.name.get().strip()
        self.cancel = threading.Event()
        self.stop_audio()
        self.start_button.configure(state='disabled')
        self.cancel_button.configure(state='normal')
        self.bar.start(12)
        self.status.set('准备生成…')
        def work():
            try:
                result = generate(text, snapshot['output'], name, snapshot['device'], snapshot['client'],
                                  cancel=self.cancel, progress=lambda msg: self.events.put(('progress', msg)))
                self.events.put(('done', result))
            except Cancelled:
                self.events.put(('cancelled', '已取消。'))
            except Exception as error:
                self.events.put(('error', str(error)))
        self.worker = threading.Thread(target=work, daemon=True)
        self.worker.start()

    def poll(self):
        while not self.events.empty():
            kind, value = self.events.get_nowait()
            if kind == 'progress':
                self.status.set(value)
                continue
            self.bar.stop()
            self.start_button.configure(state='normal')
            self.cancel_button.configure(state='disabled')
            if kind == 'done':
                self.last_wav = value
                self.play_button.configure(state='normal')
                self.status.set('已生成：' + value.stem + '（OGG / WAV / 台词记录）')
                self.name.set(self.new_name())
                if self.auto_play.get() and not self.closing:
                    self.play()
            elif kind == 'error':
                self.status.set(value)
                if not self.closing:
                    messagebox.showerror('生成未完成', value, parent=self.root)
            else:
                self.status.set(value)
        if self.closing and not (self.worker and self.worker.is_alive()):
            self.root.destroy()
            return
        self.root.after(100, self.poll)

    def play(self):
        if self.last_wav and self.last_wav.exists():
            try:
                winsound.PlaySound(str(self.last_wav), winsound.SND_FILENAME | winsound.SND_ASYNC | winsound.SND_NODEFAULT)
            except RuntimeError:
                self.status.set('试听失败，可打开输出文件夹播放WAV。')

    def stop_audio(self):
        winsound.PlaySound(None, 0)

    def open_folder(self):
        try:
            path = Path(self.output.get()).expanduser().resolve()
            path.mkdir(parents=True, exist_ok=True)
            os.startfile(path)
        except OSError:
            messagebox.showerror('无法打开', '请检查输出文件夹路径。', parent=self.root)

    def close(self):
        self.stop_audio()
        self.closing = True
        self.cancel.set()
        self.status.set('正在取消并退出…')
        self.start_button.configure(state='disabled')


if __name__ == '__main__':
    window = tk.Tk()
    App(window)
    window.mainloop()
