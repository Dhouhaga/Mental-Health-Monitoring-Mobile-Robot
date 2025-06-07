import asyncio
import websockets
import cv2
import numpy as np
import socket
import firebase_admin
from firebase_admin import credentials, db
import logging

logging.basicConfig(level=logging.INFO)

# Initialize Firebase
cred = credentials.Certificate(r"C:\Users\USER\Documents\ICE3\Sem2\IoT\Project\iot-projects-6ba0c-firebase-adminsdk-fbsvc-d449788963.json")
 
firebase_admin.initialize_app(cred, {
   'databaseURL': 'https://iot-projects-6ba0c-default-rtdb.europe-west1.firebasedatabase.app/' 
})

FACE_RECOG_URI = 'ws://localhost:8002/'
frame_queue: asyncio.Queue[bytes] = asyncio.Queue()

async def forwarder():
    """Persistent connection to face-recog server, draining the frame_queue."""
    while True:
        try:
            logging.info("Connecting to face-recognition WS…")
            async with websockets.connect(FACE_RECOG_URI, ping_interval=20) as recog_ws:
                logging.info("Connected to face-recog server")
                while True:
                    frame = await frame_queue.get()
                    try:
                        await recog_ws.send(frame)
                    except Exception as e:
                        logging.warning(f"Send failed, requeuing frame: {e}")
                        # Put it back and break to reconnect
                        await frame_queue.put(frame)
                        break
        except Exception as e:
            logging.error(f"Forwarder error: {e}. Retrying in 2 s…")
            await asyncio.sleep(2)

def get_local_ip():
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        s.connect(("8.8.8.8", 80))
        return s.getsockname()[0]
    finally:
        s.close()

async def process_image(websocket):
    """WebSocket server handler: detect faces, enqueue frames."""
    face_cascade = cv2.CascadeClassifier(
        cv2.data.haarcascades + 'haarcascade_frontalface_default.xml'
    )

    async for msg in websocket:
        nparr = np.frombuffer(msg, np.uint8)
        img = cv2.imdecode(nparr, cv2.IMREAD_COLOR)
        gray = cv2.cvtColor(img, cv2.COLOR_BGR2GRAY)
        faces = face_cascade.detectMultiScale(gray, 1.1, 4)

        db.reference("/face_detection/status").set({
            "faces_detected": len(faces) > 0
        })

        if len(faces)>0:
            # fire-and-forget enqueuing—non-blocking
            frame_queue.put_nowait(msg)

async def main():
    # 1) Start the forwarder task
    asyncio.create_task(forwarder())

    # 2) Publish our FD server IP
    ip = get_local_ip() + ":8765"
    db.reference("/server/fd_ip").set(ip)
    logging.info(f"Face-detect server IP → Firebase: {ip}")

    # 3) Launch the WebSocket
    async with websockets.serve(process_image, "0.0.0.0", 8765):
        await asyncio.Future()   # run forever

if __name__ == "__main__":
    asyncio.run(main())
