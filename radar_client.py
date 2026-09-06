import socket
import threading
import tkinter as tk
import math

# ================= CONFIG =================

ESP32_IP = "192.168.1.129"
ESP32_PORT = 5000
MAX_DISTANCE = 100

# ================= CONECTARE =================

client = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
client.connect((ESP32_IP, ESP32_PORT))

# ================= FEREASTRA =================

root = tk.Tk()
root.title("ESP32 Mini Radar")
root.geometry("950x650")
root.configure(bg="black")

canvas = tk.Canvas(
    root,
    width=750,
    height=520,
    bg="black",
    highlightthickness=0
)
canvas.pack(side="left", padx=20, pady=20)

# Centrul si raza radarului
CX = 375
CY = 470
R = 350

GREEN = "#00ff66"
DARK_GREEN = "#075c2c"
RED = "#ff3030"
WHITE = "#ffffff"
YELLOW = "#ffff00"

# ================= RADAR =================

def draw_radar():

    # Cercuri 25 / 50 / 75 / 100 cm
    for d in [25, 50, 75, 100]:

        r = R * d / MAX_DISTANCE

        canvas.create_arc(
            CX-r, CY-r,
            CX+r, CY+r,
            start=0,
            extent=180,
            outline=DARK_GREEN
        )

        canvas.create_text(
            CX+8, CY-r,
            text=f"{d} cm",
            fill=DARK_GREEN,
            font=("Consolas", 9)
        )

    # Linii unghiuri
    for angle in [-90, -45, 0, 45, 90]:

        a = math.radians(angle)

        x = CX + R * math.sin(a)
        y = CY - R * math.cos(a)

        canvas.create_line(
            CX, CY, x, y,
            fill=DARK_GREEN
        )

        canvas.create_text(
            x, y,
            text=f"{angle}°",
            fill=WHITE,
            font=("Consolas", 10)
        )

    # Linia de baza
    canvas.create_line(
        CX-R, CY,
        CX+R, CY,
        fill=DARK_GREEN
    )

draw_radar()

# ================= ELEMENTE DINAMICE =================

scan_line = canvas.create_line(
    CX, CY,
    CX, CY-R,
    fill=GREEN,
    width=3
)

object_point = canvas.create_oval(
    0, 0, 0, 0,
    fill=RED,
    outline=WHITE,
    width=2
)

# ================= PANOU =================

panel = tk.Frame(
    root,
    bg="#101010",
    width=220
)

panel.pack(
    side="right",
    fill="y",
    padx=10,
    pady=20
)

panel.pack_propagate(False)

tk.Label(
    panel,
    text="ESP32 MINI RADAR",
    font=("Consolas", 16, "bold"),
    fg=GREEN,
    bg="#101010"
).pack(pady=25)

status = tk.Label(
    panel,
    text="● STOPPED",
    font=("Consolas", 14, "bold"),
    fg=RED,
    bg="#101010"
)

status.pack(pady=10)

tk.Label(
    panel,
    text="DISTANCE",
    font=("Consolas", 10),
    fg="gray",
    bg="#101010"
).pack(pady=(30, 0))

distance_label = tk.Label(
    panel,
    text="--- cm",
    font=("Consolas", 25, "bold"),
    fg=WHITE,
    bg="#101010"
)

distance_label.pack()

tk.Label(
    panel,
    text="ANGLE",
    font=("Consolas", 10),
    fg="gray",
    bg="#101010"
).pack(pady=(30, 0))

angle_label = tk.Label(
    panel,
    text="--- °",
    font=("Consolas", 25, "bold"),
    fg=WHITE,
    bg="#101010"
)

angle_label.pack()

# ================= RADAR UPDATE =================

def update_radar(distance, angle):

    distance_label.config(
        text=f"{distance:.1f} cm"
    )

    angle_label.config(
        text=f"{angle:.0f}°"
    )

    # Corectia care functiona
    a = math.radians(-angle)

    # Directia senzorului
    x = CX + R * math.sin(a)
    y = CY - R * math.cos(a)

    canvas.coords(
        scan_line,
        CX, CY, x, y
    )

    # Afisam obiectul
    if 0 < distance <= MAX_DISTANCE:

        r = R * distance / MAX_DISTANCE

        px = CX + r * math.sin(a)
        py = CY - r * math.cos(a)

        canvas.coords(
            object_point,
            px-8, py-8,
            px+8, py+8
        )

    else:

        canvas.coords(
            object_point,
            0, 0, 0, 0
        )

# ================= TCP =================

def receive_data():

    buffer = ""

    while True:

        try:

            data = client.recv(1024).decode()

            if not data:
                break

            buffer += data

            while "\n" in buffer:

                line, buffer = buffer.split("\n", 1)
                line = line.strip()

                if line.startswith("DIST="):

                    try:

                        parts = line.split(",")

                        distance = float(
                            parts[0].split("=")[1]
                        )

                        angle = float(
                            parts[1].split("=")[1]
                        )

                        root.after(
                            0,
                            update_radar,
                            distance,
                            angle
                        )

                    except:
                        pass

        except:
            break

# ================= COMENZI =================

def start():

    client.sendall(b"START\n")

    status.config(
        text="● SCANNING",
        fg=GREEN
    )


def stop():

    client.sendall(b"STOP\n")

    status.config(
        text="● STOPPED",
        fg=RED
    )

# ================= BUTOANE =================

tk.Button(
    panel,
    text="START",
    command=start,
    width=15,
    height=2,
    font=("Consolas", 11, "bold"),
    bg="#075c2c",
    fg=WHITE
).pack(pady=(60, 5))

tk.Button(
    panel,
    text="STOP",
    command=stop,
    width=15,
    height=2,
    font=("Consolas", 11, "bold"),
    bg="#5c1515",
    fg=WHITE
).pack(pady=5)

# ================= THREAD =================

threading.Thread(
    target=receive_data,
    daemon=True
).start()

# ================= INCHIDERE =================

def close():

    try:
        client.close()
    except:
        pass

    root.destroy()

root.protocol(
    "WM_DELETE_WINDOW",
    close
)

root.mainloop()