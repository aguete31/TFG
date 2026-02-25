def limpiar_terminal():
    output.config(state=tk.NORMAL)
    output.delete(1.0, tk.END)

monitor_thread = None
monitor_stop = None

def monitor_serial():
    puerto = puerto_combo.get()
    baudios = baud_combo.get()
    if not puerto:
        messagebox.showwarning("Faltan datos", "Selecciona un puerto serial.")
        return
    output.config(state=tk.NORMAL)
    output.delete(1.0, tk.END)
    output.insert(tk.END, f"Conectando a {puerto} ({baudios} baudios)...\n\n")
    output.see(tk.END)
    output.update()
    try:
        import serial
        ser = serial.Serial(puerto, int(baudios), timeout=0.2)
        while not monitor_stop.is_set():
            try:
                data = ser.readline()
                if data:
                    output.insert(tk.END, data.decode(errors='replace'))
                    output.see(tk.END)
                    output.update()
            except Exception as e:
                output.insert(tk.END, f"\nError leyendo serial: {e}\n")
                break
        ser.close()
        output.insert(tk.END, "\nMonitor serie detenido.\n")
    except Exception as e:
        output.insert(tk.END, f"\nError abriendo puerto serie: {e}\n")
    output.config(state=tk.DISABLED)

def conectar_monitor():
    global monitor_thread, monitor_stop
    if btn_monitor['text'] == "Connect":
        if monitor_stop is None:
            monitor_stop = threading.Event()
        monitor_stop.clear()
        btn_monitor['text'] = "Disconnect"
        monitor_thread = threading.Thread(target=monitor_serial, daemon=True)
        monitor_thread.start()
    else:
        monitor_stop.set()
        btn_monitor['text'] = "Connect"

def detener_monitor():
    global monitor_thread, monitor_stop
    estaba_activo = bool(monitor_thread and monitor_thread.is_alive())
    if estaba_activo and monitor_stop is not None:
        monitor_stop.set()
    if monitor_thread and monitor_thread.is_alive():
        monitor_thread.join(timeout=1.5)
    monitor_thread = None
    if 'btn_monitor' in globals():
        btn_monitor['text'] = "Connect"
    return estaba_activo

def uploader():
    placa = combo.get()
    puerto = puerto_combo.get()
    baudios = baud_combo.get()
    if not placa or not puerto:
        messagebox.showwarning("Faltan datos", "Selecciona una placa y un puerto serial.")
        return
    info = ENTORNOS.get(placa)
    if not info:
        messagebox.showerror("Error", f"No se ha encontrado la placa '{placa}' en boards/.")
        return
    env_name = info.get("env")
    if not env_name:
        messagebox.showerror("Error", f"No hay un entorno PlatformIO asociado a '{placa}'. Revisa platformio.ini.")
        return
    if not os.path.isfile(PLATFORMIO_INI):
        messagebox.showerror("Error", "No se ha encontrado platformio.ini en la carpeta del proyecto.")
        return
    monitor_was_running = detener_monitor()
    pio_cmd = [
        PIO_CMD,
        "run",
        "-e",
        env_name,
        "-t",
        "upload",
        "--upload-port",
        puerto,
    ]
    output.config(state=tk.NORMAL)
    output.delete(1.0, tk.END)
    if monitor_was_running:
        output.insert(tk.END, "Monitor serie detenido temporalmente para liberar el puerto.\n\n")
    output.insert(tk.END, f"Subiendo firmware ({env_name}) por {puerto} ({baudios} baudios)...\n\n")
    output.see(tk.END)
    output.update()
    try:
        proc = subprocess.Popen(
            pio_cmd,
            cwd=PROJECT_DIR,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
        )
        for line in proc.stdout:
            output.insert(tk.END, line)
            output.see(tk.END)
            output.update()
        proc.wait()
        if proc.returncode == 0:
            output.insert(tk.END, "\nSubida exitosa.\n")
        else:
            output.insert(tk.END, f"\nError de subida (código {proc.returncode}).\n")
    except Exception as e:
        output.insert(tk.END, f"\nError ejecutando PlatformIO: {e}\n")
    output.config(state=tk.DISABLED)
def compilar():
    placa = combo.get()
    ejemplo = ejemplos_combo.get()
    if not placa or not ejemplo:
        messagebox.showwarning("Faltan datos", "Selecciona una placa y un firmware.")
        return
    info = ENTORNOS.get(placa)
    if not info:
        messagebox.showerror("Error", f"No se ha encontrado la placa '{placa}' en boards/.")
        return
    env_name = info.get("env")
    if not env_name:
        messagebox.showerror("Error", f"No hay un entorno PlatformIO asociado a '{placa}'. Revisa platformio.ini.")
        return
    try:
        filter_line = _construir_build_src_filter(info, ejemplo)
    except ValueError as err:
        messagebox.showerror("Error", str(err))
        return
    ini_path = PLATFORMIO_INI
    if not os.path.isfile(ini_path):
        messagebox.showerror("Error", "No se ha encontrado platformio.ini en la carpeta del proyecto.")
        return
    ini_bak = ini_path + ".bak"
    with open(ini_path, "r", encoding="utf-8") as f:
        ini_lines = f.readlines()
    with open(ini_bak, "w", encoding="utf-8") as f:
        f.writelines(ini_lines)
    section_header = f"[env:{env_name}]"
    target_env = None
    for idx, line in enumerate(ini_lines):
        if line.strip() == section_header:
            target_env = idx
            break
    if target_env is None:
        os.remove(ini_bak)
        messagebox.showerror("Error", f"No se ha encontrado la sección {section_header} en platformio.ini.")
        return
    next_env = None
    for j in range(target_env + 1, len(ini_lines)):
        if ini_lines[j].strip().startswith("[env:"):
            next_env = j
            break
    insert_idx = target_env + 1
    limit = next_env if next_env is not None else len(ini_lines)
    reemplazado = False
    for idx in range(target_env + 1, limit):
        stripped = ini_lines[idx].strip()
        if stripped.startswith("build_src_filter"):
            ini_lines[idx] = filter_line
            reemplazado = True
            break
        if stripped.startswith("src_filter"):
            # limpieza de configuraciones antiguas
            ini_lines[idx] = ""
        if stripped.startswith("[") and idx != target_env + 1:
            break
        insert_idx = idx + 1
    if not reemplazado:
        ini_lines.insert(insert_idx, filter_line)
    # eliminar líneas vacías creadas durante la limpieza
    ini_lines = [line for line in ini_lines if line.strip() != "" or line == "\n"]
    with open(ini_path, "w", encoding="utf-8") as f:
        f.writelines(ini_lines)
    output.config(state=tk.NORMAL)
    output.delete(1.0, tk.END)
    output.insert(tk.END, f"Compilando {ejemplo} para {placa} ({env_name})...\n\n")
    output.see(tk.END)
    output.update()
    try:
        proc = subprocess.Popen(
            [PIO_CMD, "run", "-e", env_name],
            cwd=PROJECT_DIR,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
        )
        for line in proc.stdout:
            output.insert(tk.END, line)
            output.see(tk.END)
            output.update()
        proc.wait()
        if proc.returncode == 0:
            output.insert(tk.END, "\nCompilación exitosa.\n")
        else:
            output.insert(tk.END, f"\nError de compilación (código {proc.returncode}).\n")
    except Exception as e:
        output.insert(tk.END, f"\nError ejecutando PlatformIO: {e}\n")
    finally:
        if os.path.exists(ini_bak):
            with open(ini_bak, "r", encoding="utf-8") as f:
                orig = f.read()
            with open(ini_path, "w", encoding="utf-8") as f:
                f.write(orig)
            os.remove(ini_bak)
    output.config(state=tk.DISABLED)

import configparser
import os
import re
import subprocess
import threading
import tkinter as tk
from tkinter import ttk, messagebox

import serial.tools.list_ports

PROJECT_DIR = os.path.dirname(__file__)
PLATFORMIO_INI = os.path.join(PROJECT_DIR, "platformio.ini")
PIO_EXECUTABLE = os.path.join(PROJECT_DIR, ".venv", "bin", "pio")
PIO_CMD = PIO_EXECUTABLE if os.path.isfile(PIO_EXECUTABLE) else "pio"


def _leer_entornos_desde_boards():
    """
    Analiza platformio.ini y la carpeta boards/ para construir
    el mapa de entornos disponibles.
    """
    entornos = {}
    boards_dir = os.path.join(PROJECT_DIR, "boards")
    if not os.path.isdir(boards_dir):
        return entornos

    config = configparser.ConfigParser()
    config.optionxform = str  # mantener mayúsculas/minúsculas
    try:
        config.read(PLATFORMIO_INI, encoding="utf-8")
    except Exception:
        config = None

    env_por_board = {}
    if config:
        pattern = re.compile(r"[+-]<([^>]+)>")
        for section in config.sections():
            if not section.startswith("env:"):
                continue
            env_name = section[4:]
            filter_value = None
            for option in ("build_src_filter", "src_filter"):
                if config.has_option(section, option):
                    try:
                        filter_value = config.get(section, option)
                    except (configparser.NoOptionError, configparser.NoSectionError):
                        filter_value = None
                    break
            if not filter_value:
                continue

            for match in pattern.findall(filter_value):
                normalized = match.replace("\\", "/")
                while normalized.startswith("./"):
                    normalized = normalized[2:]
                while normalized.startswith("../"):
                    normalized = normalized[3:]
                if not normalized.startswith("boards/"):
                    continue
                parts = normalized.split("/")
                if len(parts) < 2:
                    continue
                board_name = parts[1]
                env_por_board[board_name] = env_name
                break

    for nombre in sorted(os.listdir(boards_dir)):
        abs_board = os.path.join(boards_dir, nombre)
        if not os.path.isdir(abs_board):
            continue
        env_name = env_por_board.get(nombre)
        entornos[nombre] = {
            "env": env_name,
            "abs_board_dir": abs_board,
            "rel_board_dir": os.path.join("boards", nombre),
        }
    return entornos

ENTORNOS = _leer_entornos_desde_boards()

def _construir_build_src_filter(info, ejemplo):
    abs_board_dir = os.path.abspath(info["abs_board_dir"])
    if not os.path.isdir(abs_board_dir):
        raise ValueError(f"La carpeta de la placa no existe: {info['abs_board_dir']}")

    src_root = os.path.join(PROJECT_DIR, "src")
    rel_board_dir = os.path.relpath(abs_board_dir, src_root).replace(os.sep, "/")
    if ejemplo == "src/main.cpp":
        src_dir = os.path.join(abs_board_dir, "src")
        if not os.path.isdir(src_dir):
            raise ValueError(f"No se ha encontrado la carpeta 'src' dentro de {rel_board_dir}.")
        target = os.path.join(rel_board_dir, "src").replace(os.sep, "/")
        return f"build_src_filter = -<*> +<{target}>\n"

    abs_example = os.path.abspath(os.path.join(abs_board_dir, ejemplo))
    if os.path.commonpath([abs_board_dir, abs_example]) != abs_board_dir:
        raise ValueError("El firmware seleccionado está fuera de la carpeta de la placa.")
    if not os.path.isfile(abs_example):
        raise ValueError(f"No se ha encontrado el fichero '{ejemplo}'.")
    rel_example = os.path.relpath(abs_example, src_root).replace(os.sep, "/")
    return f"build_src_filter = -<*> +<{rel_example}>\n"

def obtener_ejemplos_y_main(board_dir):
    ejemplos = []
    main_path = os.path.join(board_dir, "src", "main.cpp")
    if os.path.isfile(main_path):
        ejemplos.append("src/main.cpp")
    examples_dir = os.path.join(board_dir, "examples")
    if os.path.isdir(examples_dir):
        for root_dir, _, files in os.walk(examples_dir):
            for f in files:
                if f.endswith(".cpp"):
                    rel_path = os.path.relpath(os.path.join(root_dir, f), board_dir)
                    ejemplos.append(rel_path.replace("\\", "/"))
    return sorted(ejemplos)

def on_core_selected(event=None):
    placa = combo.get()
    info = ENTORNOS.get(placa)
    if not info:
        ejemplos_combo['values'] = []
        ejemplos_combo.set("")
        return
    ejemplos = obtener_ejemplos_y_main(info["abs_board_dir"])
    if ejemplos:
        ejemplos_combo['values'] = ejemplos
        ejemplos_combo.set(ejemplos[0])
    else:
        ejemplos_combo['values'] = []
        ejemplos_combo.set("")

def actualizar_puertos():
    puertos = [p.device for p in serial.tools.list_ports.comports()]
    puerto_combo['values'] = puertos
    if puertos:
        puerto_combo.set(puertos[0])
    else:
        puerto_combo.set("")

if __name__ == "__main__":
    monitor_stop = threading.Event()
    root = tk.Tk()
    root.title("Gestor de Placas")
    root.geometry("900x600")
    root.resizable(False, False)

    # Estilo visual moderno
    style = ttk.Style()
    try:
        style.theme_use('clam')
    except:
        pass
    style.configure('TButton', font=('Segoe UI', 11, 'bold'), foreground='#222', background='#e0e0e0', padding=6)
    style.map('TButton', background=[('active', '#b0b0b0')])
    style.configure('TLabel', font=('Segoe UI', 11), foreground='#222', background='#f5f5f5')
    style.configure('TCombobox', font=('Segoe UI', 11), padding=4)
    root.configure(bg='#f5f5f5')

    # Frame para la selección en una sola línea
    frame_seleccion = tk.Frame(root, bg='#f5f5f5')
    frame_seleccion.pack(pady=10, padx=20, fill=tk.X)

    lbl_placa = tk.Label(frame_seleccion, text="Selecciona una placa:", bg='#f5f5f5')
    lbl_placa.pack(side=tk.LEFT, padx=(0,5))
    board_names = sorted(ENTORNOS.keys())
    combo = ttk.Combobox(frame_seleccion, values=board_names, state="readonly", width=25)
    combo.pack(side=tk.LEFT, padx=(0,20))
    combo.bind("<<ComboboxSelected>>", on_core_selected)

    lbl_firmware = tk.Label(frame_seleccion, text="Selecciona un firmware:", bg='#f5f5f5')
    lbl_firmware.pack(side=tk.LEFT, padx=(0,5))
    ejemplos_combo = ttk.Combobox(frame_seleccion, values=[], state="readonly", width=40)
    ejemplos_combo.pack(side=tk.LEFT)
    if board_names:
        combo.set(board_names[0])
        on_core_selected()
    else:
        messagebox.showinfo("Sin placas", "No se han encontrado carpetas en boards/. Crea una para empezar.")

    # Frame para selección Serial y Baudios
    frame_serial = tk.Frame(root, bg='#f5f5f5')
    frame_serial.pack(pady=5, padx=20, fill=tk.X)

    lbl_serial = tk.Label(frame_serial, text="Serial:", bg='#f5f5f5')
    lbl_serial.pack(side=tk.LEFT, padx=(0,5))
    puerto_combo = ttk.Combobox(frame_serial, values=[], state="readonly", width=25)
    puerto_combo.pack(side=tk.LEFT, padx=(0,20))
    actualizar_puertos()

    lbl_baud = tk.Label(frame_serial, text="Baudios:", bg='#f5f5f5')
    lbl_baud.pack(side=tk.LEFT, padx=(0,5))
    baud_combo = ttk.Combobox(frame_serial, values=["115200", "9600", "57600", "38400", "19200", "74880", "230400"], state="readonly", width=10)
    baud_combo.set("115200")
    baud_combo.pack(side=tk.LEFT)

    # Frame para botones
    frame_botones = tk.Frame(root, bg='#f5f5f5')
    frame_botones.pack(pady=10, padx=20, fill=tk.X)

    btn_compilar = tk.Button(frame_botones, text="Compiler", command=compilar)
    btn_compilar.pack(side=tk.LEFT, padx=10)

    btn_upload = tk.Button(frame_botones, text="Uploader", command=uploader)
    btn_upload.pack(side=tk.LEFT, padx=10)

    btn_monitor = tk.Button(frame_botones, text="Connect", width=12, command=conectar_monitor)
    btn_monitor.pack(side=tk.LEFT, padx=10)
    btn_clear = tk.Button(frame_botones, text="Clear", width=8, command=limpiar_terminal)
    btn_clear.pack(side=tk.LEFT, padx=10)

    # Frame para separar el botón cerrar
    frame_cerrar = tk.Frame(frame_botones, bg='#f5f5f5')
    frame_cerrar.pack(side=tk.RIGHT, padx=(100,0), fill=tk.Y)
    btn_cerrar = tk.Button(frame_cerrar, text="Cerrar", width=10, command=root.destroy)
    btn_cerrar.pack()

    # Frame para área de terminal/salida
    frame_terminal = tk.Frame(root, bg='#222')
    frame_terminal.pack(fill=tk.BOTH, expand=True, pady=10, padx=20)

    output = tk.Text(frame_terminal, height=18, width=110, state=tk.DISABLED, bg="#181818", fg="#00FF00", insertbackground="#00FF00", font=("Consolas", 11))
    output.pack(side=tk.LEFT, fill=tk.BOTH, expand=True, padx=(0,2), pady=2)

    scroll = tk.Scrollbar(frame_terminal, command=output.yview)
    scroll.pack(side=tk.RIGHT, fill=tk.Y)
    output.config(yscrollcommand=scroll.set)

    root.mainloop()
