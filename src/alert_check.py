from influxdb_client import InfluxDBClient
import smtplib
from email.mime.text import MIMEText
import time

# --- Config ---
INFLUX_URL = "http://localhost:8086"
TOKEN= "tMkQii-E0JYGHOnevmLO7Wnf5P0lQc6Qeh7vNpbMB90jUHcLgJHvdS16l7snbMJN8m7Ngc18a-CcCq3engj6ug=="
ORG = "iot"
BUCKET = "bott"
EMAIL_RECEIVER = "dhouhaguoud@gmail.com"
EMAIL_SENDER = "dhouhaguoud@gmail.com"
EMAIL_PASS = "xage nbeu fmoc fppl" 
THRESHOLD = -2
POLL_INTERVAL = 60  

client = InfluxDBClient(url=INFLUX_URL, token=TOKEN, org=ORG)
query_api = client.query_api()

def check_and_alert():
    query = """
from(bucket: \"bott\")
  |> range(start: -5m)
  |> filter(fn: (r) =>
      r._measurement == \"mentalHealth\" and
      r.person_id == \"0\" and
      r._field == \"score\"
    )
  |> last()
"""
    tables = query_api.query(query)
    for table in tables:
        for record in table.records:
            score = record.get_value()
            print(f"Checked score: {score}")
            if score < THRESHOLD:
                msg = MIMEText(f"Alert! Score is low: {score}")
                msg['Subject'] = "InfluxDB Alert"
                msg['From'] = EMAIL_SENDER
                msg['To'] = EMAIL_RECEIVER
                with smtplib.SMTP_SSL("smtp.gmail.com", 465) as smtp:
                    smtp.login(EMAIL_SENDER, EMAIL_PASS)
                    smtp.send_message(msg)
                print("📧 Alert sent")

if __name__ == "__main__":
    print("Starting continuous alert checker (press Ctrl+C to stop)...")
    try:
        while True:
            check_and_alert()
            time.sleep(POLL_INTERVAL)
    except KeyboardInterrupt:
        print("Stopped by user.")
    finally:
        client.close()
