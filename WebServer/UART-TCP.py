import socket
from flask import Flask, request, send_from_directory
from flask_cors import CORS
import threading
import csv
import time

app = Flask(__name__)
CORS(app)

# --------------------------
# Shared state
# --------------------------
esp_conn = None
esp_lock = threading.Lock()
esp_queue = []

# --------------------------
# TCP server
# --------------------------
def tcp_server():
    global esp_conn
    HOST = ''
    PORT = 5000

    server_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server_sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    server_sock.bind((HOST, PORT))
    server_sock.listen()
    print(f"[TCP] Listening on port {PORT}...")

    while True:
        conn, addr = server_sock.accept()
        print("[TCP] ESP32 connected from:", addr)
        with esp_lock:
            esp_conn = conn
        threading.Thread(target=handle_esp, args=(conn,), daemon=True).start()

def handle_esp(conn):
    global esp_conn
    buffer = ""
    conn.settimeout(0.1)  # allow periodic checks for queued messages
    try:
        while True:
            # 1️⃣ Send all queued commands
            with esp_lock:
                queued = esp_queue.copy()
                esp_queue.clear()

            for msg in queued:
                try:
                    conn.sendall(msg.encode())
                    print(f"[ESP] Sent: {msg.strip()}")
                except Exception as e:
                    print("[ESP] Send failed:", e)
                    return  # will close connection

            # 2️⃣ Receive any data from ESP32
            try:
                data = conn.recv(1024)
                if data:
                    buffer += data.decode()
                    while '\n' in buffer:
                        line, buffer = buffer.split('\n', 1)
                        line = line.strip()
                        if line:
                            print("[TCP] ESP32 says:", line)
                            if "," in line:
                                with open("data.csv", "a", newline="") as f:
                                    writer = csv.writer(f)
                                    writer.writerow(line.split(","))
            except socket.timeout:
                continue
            except ConnectionResetError:
                print("[TCP] ESP32 disconnected (reset)")
                break
    finally:
        with esp_lock:
            if esp_conn == conn:
                esp_conn = None
        conn.close()
        print("[TCP] Waiting for new ESP32 connection...")

# Start TCP server in background
threading.Thread(target=tcp_server, daemon=True).start()

# --------------------------
# Flask routes
# --------------------------
@app.route('/')
def serve_index():
    return send_from_directory('static', 'index.html')

@app.route('/led', methods=['GET'])
def led():
    state = request.args.get('state')
    if not state:
        return "Missing state parameter", 400

    cmd = "LED_ON\n" if state == "1" else "LED_OFF\n"
    with esp_lock:
        esp_queue.append(cmd)
    print(f"[HTTP] Queued command: {cmd.strip()}")
    return f"LED set to {state}", 200

@app.route('/data.csv')
def serve_csv():
    try:
        return send_from_directory('.', 'data.csv')
    except Exception as e:
        print("[HTTP] CSV not found:", e)
        return "CSV not found", 404

# --------------------------
# Start Flask
# --------------------------
if __name__ == "__main__":
    print("[HTTP] Flask server starting on port 8000...")
    # ⚡ Important: debug=False avoids Flask spawning multiple processes
    app.run(host="0.0.0.0", port=8000, debug=False, threaded=True)