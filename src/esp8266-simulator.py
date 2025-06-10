from influxdb_client import InfluxDBClient, Point
from influxdb_client.client.write_api import SYNCHRONOUS
import time
import random
import os
from datetime import datetime

# Firebase Admin SDK
import firebase_admin
from firebase_admin import credentials, db

# Initialize Firebase app
cred = credentials.Certificate(
    "PATH"
)
firebase_admin.initialize_app(cred, {
    'databaseURL': 'URL'
})


ref = db.reference('sensor_data')

# influx
token = ""
org = ""
bucket = ""
url = "http://localhost:8086"

client = InfluxDBClient(url=url, token=token, org=org)
write_api = client.write_api(write_options=SYNCHRONOUS)

# Logic for simulated sensor values
# Temperature (°C): cooler at night, warmer in afternoon
# Brightness (lux): high during daylight hours, low at night
# Humidity (%): inverse to temperature (higher temp lowers humidity), peak at early morning and late evening
# Air Quality Index (AQI): correlated with temperature and brightness (higher temp -> higher AQI, lower brightness -> worse AQI)
def simulate_values():
    now = datetime.now()
    hour = now.hour

    # Temperature: simulate hotter days in North Africa
    if 6 <= hour < 12:
        temp = random.uniform(22, 32)
    elif 12 <= hour < 18:
        temp = random.uniform(33, 45)
    else:
        temp = random.uniform(18, 25)

    # Brightness: high midday, lower at sunrise/sunset, very low at night
    if 6 <= hour < 18:
        brightness = max(0, min(1200, random.gauss(1000 - abs(12-hour)*80, 120)))
    else:
        brightness = random.uniform(0, 30)

    # Humidity: low during hot periods, slightly higher at night
    base_humidity = random.uniform(10, 30)
    temp_hum_factor = (30 - temp) * 0.8  # hot = dry
    time_hum_factor = 10 if (hour < 6 or hour > 20) else 0  # boost at night
    humidity = base_humidity + temp_hum_factor + time_hum_factor
    humidity = max(5, min(60, humidity))

    # AQI: affected by heat and dust, slightly better at night
    aqi_base = random.uniform(60, 100)
    temp_factor = (temp - 30) * 2.5 if temp > 30 else 0
    bright_factor = (1200 - brightness) / 80
    time_factor = -10 if (hour < 6 or hour > 20) else 0
    aqi = aqi_base + temp_factor + bright_factor + time_factor
    aqi = max(10, min(300, aqi))

    point = (
        Point("environment")
        .field('timestamp', now.isoformat())
        .field('temperature', round(temp, 2))
        .field('brightness', round(brightness, 2))
        .field('humidity', round(humidity, 2))
        .field('aqi', round(aqi, 2))
    )
    write_api.write(bucket=bucket, org=org, record=point)
    print("data sent to influx")


    return {
        'timestamp': now.isoformat(),
        'temperature': round(temp, 2),
        'brightness': round(brightness, 2),
        'humidity': round(humidity, 2),
        'aqi': round(aqi, 2)
    }


def update_loop(interval=10):
    print(f"Starting sensor simulation updating every {interval} seconds...")
    try:
        while True:
            data = simulate_values()
            ref.set(data)
            print(f"Updated: {data}")
            time.sleep(interval)
    except KeyboardInterrupt:
        print("Simulation stopped by user.")

if __name__ == '__main__':
    # Default interval 10s, can override via env VAR SIM_INTERVAL
    interval = int(os.getenv('SIM_INTERVAL', '10'))
    update_loop(interval)
