import asyncio
import random
import threading
import time
import requests

import cv2
import numpy as np
import firebase_admin
from firebase_admin import credentials, db
from deepface import DeepFace
import websockets
from influxdb_client import InfluxDBClient, Point
from influxdb_client.client.write_api import SYNCHRONOUS

# Initialize Firebase
cred = credentials.Certificate(
    r"C:\Users\USER\Documents\ICE3\Sem2\IoT\Project\iot-projects-6ba0c-firebase-adminsdk-fbsvc-d449788963.json"
)
firebase_admin.initialize_app(cred, {
    'databaseURL': 'https://iot-projects-6ba0c-default-rtdb.europe-west1.firebasedatabase.app/'
})

# influx
token = "tMkQii-E0JYGHOnevmLO7Wnf5P0lQc6Qeh7vNpbMB90jUHcLgJHvdS16l7snbMJN8m7Ngc18a-CcCq3engj6ug=="
org = "iot"
bucket = "bott"
url = "http://localhost:8086"

client = InfluxDBClient(url=url, token=token, org=org)
write_api = client.write_api(write_options=SYNCHRONOUS)

# Load OpenCV’s face detector
face_cascade = cv2.CascadeClassifier(cv2.data.haarcascades + "haarcascade_frontalface_default.xml")

# Globals
known_embeddings = []
known_ids = []
face_scores = []
next_id = 0
lock = threading.Lock()

async def process_frame(ws):
    global next_id
    try: 
        async for frame_bytes in ws:
            # Decode JPEG
            img = cv2.imdecode(np.frombuffer(frame_bytes, np.uint8), cv2.IMREAD_COLOR)
            gray = cv2.cvtColor(img, cv2.COLOR_BGR2GRAY)
            faces = face_cascade.detectMultiScale(gray, 1.3, 5)

            for (x, y, w, h) in faces:
                face_img = img[y:y+h, x:x+w]

                # 1) Get an embedding
                emb = DeepFace.represent(face_img, model_name="Facenet", enforce_detection=False)[0]["embedding"]
                emb = np.array(emb)

                # 2) Compare to known
                with lock:
                    if known_embeddings:
                        sims = np.dot(known_embeddings, emb) / (
                            np.linalg.norm(known_embeddings, axis=1) * np.linalg.norm(emb)
                        )
                        best = np.argmax(sims)
                        if sims[best] > 0.7:
                            fid = known_ids[best]
                        else:
                            fid = next_id; next_id += 1
                            known_ids.append(fid); known_embeddings.append(emb); face_scores.append(0)
                    else:
                        fid = next_id; next_id += 1
                        known_ids.append(fid); known_embeddings.append(emb); face_scores.append(0)

                # 3) Emotion via DeepFace
                emo_res = DeepFace.analyze(
                    face_img,
                    actions=['emotion'],
                    enforce_detection=False,
                    detector_backend="opencv"
                )

                # unwrap if returned as a list
                if isinstance(emo_res, list):
                    emo_res = emo_res[0]

                dom = emo_res.get('dominant_emotion', None)
                delta = {'happy':2, 'neutral':1, 'sad':-1, 'angry':-2, 'fear':-2}.get(dom, 0)

                with lock:
                    idx = known_ids.index(fid)
                    face_scores[idx] += delta
                    print("Face ID:", fid, "Emotion:", dom, "Score:", face_scores[idx])
                    face_ref = db.reference(f"/mentalHealth/ppl/{fid}")
                    face_ref.update({
                        "score": face_scores[idx],
                        "emotion": dom
                    })

                    point = (
                        Point("mentalHealth")
                        .tag("person_id", str(idx))                 
                        .field("score", float(face_scores[idx]))   
                        .field("emotion", dom)
                    )
                    write_api.write(bucket=bucket, org=org, record=point)
                    print("data sent to influx person id:", idx)
                    

            await send_updates()
    except (websockets.exceptions.ConnectionClosedError, ConnectionAbortedError) as e:
        # Client disconnected or network aborted the socket
        print(f"Client disconnected: {e!r}")
    except Exception as e:
        # Some other unexpected exception
        print(f"Error in process_frame: {e!r}")
            

async def send_updates():
    with lock:
        avg = sum(face_scores)/len(face_scores) if face_scores else 0
    db.reference('/mentalHealth/average_scores').set({'avg_score': avg})

    point = (
        Point("mentalHealth")
        .field('globalScore', float(avg))
    )
    write_api.write(bucket=bucket, org=org, record=point)
    print("data sent to influx")

async def serve():
    async with websockets.serve(process_frame, '0.0.0.0', 8002):
        print("WS server on 8002…")
        await asyncio.Future()

if __name__ == '__main__':
    asyncio.run(serve())


















'''while True:
        point = (
            Point("mentalHealth")
            .field('globalScore', float(random.randint(0, 100)))
        )
        write_api.write(bucket=bucket, org=org, record=point)
        print("data sent to influx")

        point = (
            Point("mentalHealth")
            .field("person1", float(random.randint(0, 100)))
        )
        write_api.write(bucket=bucket, org=org, record=point)
        print("data sent to influx")
        time.sleep(1)'''
