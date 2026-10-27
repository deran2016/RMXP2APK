"""Standalone Windows interface for the existing Android XP builder."""
import os
import queue
import subprocess
import sys
import threading
import tkinter as tk
from pathlib import Path
from tkinter import filedialog, messagebox, ttk

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from android.editor.builder_gui import build_command

BUILDER = ROOT / 'android' / 'builder' / 'build_android.py'


def console_build_command(*args):
    command = build_command(BUILDER, *args)
    interpreter = Path(sys.executable)
    if interpreter.name.lower() == 'pythonw.exe':
        console = interpreter.with_name('python.exe')
        if console.is_file():
            command[0] = str(console)
    return command


def validate_paths(project, output):
    if not project.strip():
        raise ValueError('XP 프로젝트 폴더를 선택하세요.')
    source = Path(project).expanduser().resolve()
    if not source.is_dir() or not (source / 'Game.ini').is_file():
        raise ValueError('Game.ini가 있는 XP 프로젝트 폴더를 선택하세요.')
    if not output.strip():
        raise ValueError('APK 저장 위치를 선택하세요.')
    destination = Path(output).expanduser().resolve()
    if destination.suffix.lower() != '.apk' or not destination.parent.is_dir():
        raise ValueError('존재하는 폴더 안의 .apk 파일을 선택하세요.')
    if destination.exists():
        raise ValueError('이미 존재하는 APK입니다. 다른 파일 이름을 선택하세요.')
    if destination.is_relative_to(source):
        raise ValueError('원본 프로젝트 밖에 APK를 저장하세요.')
    return source, destination


class BuilderWindow:
    def __init__(self, root):
        self.root = root
        root.title('RMXP2APK')
        root.geometry('850x720')
        root.minsize(720, 620)
        self.project = tk.StringVar()
        self.output = tk.StringVar()
        self.data_encoding = tk.StringVar(value='UTF-8')
        self.ini_encoding = tk.StringVar(value='UTF-8')
        self.rtp = tk.StringVar()
        self.ruby = tk.StringVar()
        self.application_id = tk.StringVar()
        self.high_refresh = tk.BooleanVar(value=False)
        self.status = tk.StringVar(value='프로젝트와 APK 저장 위치를 선택하세요.')
        self.messages = queue.Queue()
        self.running = False
        self.controls = []

        frame = ttk.Frame(root, padding=16)
        frame.pack(fill='both', expand=True)
        frame.columnconfigure(1, weight=1)
        ttk.Label(frame, text='RPG Maker XP → Android APK', font=('맑은 고딕', 16, 'bold')).grid(row=0, column=0, columnspan=3, sticky='w')
        ttk.Label(frame, text='기본 XP 프로젝트용 실험적 빌더 · 원본은 변경하지 않습니다.').grid(row=1, column=0, columnspan=3, sticky='w', pady=(3, 12))
        self.path_row(frame, 2, '프로젝트 폴더', self.project, self.choose_project)
        self.path_row(frame, 3, 'APK 저장 위치', self.output, self.choose_output)
        encoding = ttk.Frame(frame)
        encoding.grid(row=4, column=0, columnspan=3, sticky='w', pady=5)
        ttk.Label(encoding, text='게임 데이터 인코딩').pack(side='left')
        self.combo(encoding, self.data_encoding).pack(side='left', padx=(8, 20))
        ttk.Label(encoding, text='Game.ini 인코딩').pack(side='left')
        self.combo(encoding, self.ini_encoding).pack(side='left', padx=8)
        self.path_row(frame, 5, 'XP RTP 폴더 (선택)', self.rtp, self.choose_rtp)
        self.path_row(frame, 6, 'Lunetile Ruby (선택)', self.ruby, self.choose_ruby)
        ttk.Label(frame, text='앱 ID (선택)').grid(row=7, column=0, sticky='w', pady=5)
        entry = ttk.Entry(frame, textvariable=self.application_id)
        entry.grid(row=7, column=1, columnspan=2, sticky='ew', pady=5)
        self.controls.append(entry)
        ttk.Label(frame, text='비워두면 프로젝트 경로로 ID를 생성합니다. 프로젝트를 옮긴 뒤 기존 세이브를 유지하려면 이전 ID와 서명 키를 사용하세요.', wraplength=560).grid(row=8, column=0, columnspan=2, sticky='w', pady=(0, 8))
        high = ttk.Checkbutton(frame, text='120 FPS (검토된 기본 XP 스크립트만)', variable=self.high_refresh)
        high.grid(row=8, column=2, sticky='e')
        self.controls.append(high)
        self.button = ttk.Button(frame, text='APK 빌드 시작', command=self.start)
        self.button.grid(row=9, column=2, sticky='e', pady=5)
        ttk.Label(frame, textvariable=self.status).grid(row=9, column=0, columnspan=2, sticky='w', pady=5)
        ttk.Label(frame, text='진행 및 오류 로그').grid(row=10, column=0, columnspan=3, sticky='w', pady=(8, 4))
        log_frame = ttk.Frame(frame)
        log_frame.grid(row=11, column=0, columnspan=3, sticky='nsew')
        frame.rowconfigure(11, weight=1)
        self.log = tk.Text(log_frame, wrap='word', height=10, state='disabled')
        scroll = ttk.Scrollbar(log_frame, command=self.log.yview)
        self.log.configure(yscrollcommand=scroll.set)
        self.log.pack(side='left', fill='both', expand=True)
        scroll.pack(side='right', fill='y')
        ttk.Label(frame, text='지원 범위: 640×480 기본 XP, ARM64 Android 8 이상. Windows DLL·MP3/WMA·암호화 프로젝트 등은 지원하지 않습니다.', wraplength=800).grid(row=12, column=0, columnspan=3, sticky='w', pady=(10, 0))
        root.protocol('WM_DELETE_WINDOW', self.close)
        root.after(100, self.poll)

    def path_row(self, parent, row, label, variable, command):
        ttk.Label(parent, text=label).grid(row=row, column=0, sticky='w', pady=5, padx=(0, 10))
        entry = ttk.Entry(parent, textvariable=variable)
        entry.grid(row=row, column=1, sticky='ew', pady=5)
        button = ttk.Button(parent, text='찾아보기', command=command)
        button.grid(row=row, column=2, padx=(8, 0), pady=5)
        self.controls.extend((entry, button))

    def combo(self, parent, variable):
        combo = ttk.Combobox(parent, textvariable=variable, values=('UTF-8', 'CP949', 'CP932'), state='readonly', width=9)
        self.controls.append(combo)
        return combo

    def choose_project(self):
        path = filedialog.askdirectory(parent=self.root, title='XP 프로젝트 폴더 선택')
        if path:
            self.project.set(path)
            if not self.output.get().strip():
                self.output.set(str(ROOT / 'build' / (Path(path).name + '-android.apk')))

    def choose_output(self):
        path = filedialog.asksaveasfilename(parent=self.root, title='APK 저장 위치', defaultextension='.apk', filetypes=[('Android APK', '*.apk')], confirmoverwrite=False)
        if path:
            self.output.set(path)

    def choose_rtp(self):
        path = filedialog.askdirectory(parent=self.root, title='XP RTP 폴더 선택')
        if path:
            self.rtp.set(path)

    def choose_ruby(self):
        path = filedialog.askopenfilename(parent=self.root, title='Lunetile Ruby 선택', filetypes=[('실행 파일', '*.exe')])
        if path:
            self.ruby.set(path)

    def set_controls(self, enabled):
        for widget in self.controls:
            widget.configure(state=('readonly' if isinstance(widget, ttk.Combobox) else 'normal') if enabled else 'disabled')
        self.button.configure(state='normal' if enabled else 'disabled')

    def write_log(self, text):
        self.log.configure(state='normal')
        self.log.insert('end', text)
        self.log.see('end')
        self.log.configure(state='disabled')

    def start(self):
        if self.running:
            return
        try:
            source, destination = validate_paths(self.project.get(), self.output.get())
        except (ValueError, OSError) as error:
            messagebox.showerror('입력 확인', str(error), parent=self.root)
            return
        command = console_build_command(str(source), str(destination), self.data_encoding.get(), self.ini_encoding.get(), self.rtp.get().strip(), self.ruby.get().strip(), self.application_id.get(), self.high_refresh.get())
        self.log.configure(state='normal')
        self.log.delete('1.0', 'end')
        self.log.configure(state='disabled')
        self.running = True
        self.set_controls(False)
        self.status.set('빌드 중입니다. 완료될 때까지 기다려 주세요.')
        threading.Thread(target=self.run_build, args=(command, destination), daemon=True).start()

    def run_build(self, command, destination):
        try:
            env = dict(os.environ, PYTHONIOENCODING='utf-8', PYTHONUTF8='1', PYTHONUNBUFFERED='1')
            process = subprocess.Popen(command, cwd=ROOT, stdin=subprocess.DEVNULL, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                       text=True, encoding='utf-8', errors='replace', bufsize=1, env=env,
                                       creationflags=getattr(subprocess, 'CREATE_NO_WINDOW', 0))
            with process.stdout:
                for line in process.stdout:
                    self.messages.put(('log', line))
            code = process.wait()
            self.messages.put(('done', code == 0 and destination.is_file(), code, str(destination)))
        except Exception as error:
            self.messages.put(('error', str(error)))

    def poll(self):
        try:
            while True:
                message = self.messages.get_nowait()
                if message[0] == 'log':
                    self.write_log(message[1])
                else:
                    self.running = False
                    self.set_controls(True)
                    if message[0] == 'error':
                        self.status.set('빌드 실패')
                        self.write_log('\n' + message[1] + '\n')
                    elif message[1]:
                        self.status.set('빌드 완료: ' + message[3])
                        self.write_log('\nAPK 생성 완료: ' + message[3] + '\n')
                    else:
                        self.status.set('빌드 실패. 로그를 확인한 후 다시 시도하세요.')
                        self.write_log('\n빌드 종료 코드: ' + str(message[2]) + '\n')
        except queue.Empty:
            pass
        self.root.after(100, self.poll)

    def close(self):
        if self.running:
            messagebox.showinfo('빌드 진행 중', '빌드가 끝난 뒤 창을 닫을 수 있습니다.', parent=self.root)
        else:
            self.root.destroy()


def main():
    root = tk.Tk()
    BuilderWindow(root)
    root.mainloop()


if __name__ == '__main__':
    main()
