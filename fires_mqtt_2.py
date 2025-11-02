# fires_mqtt.py — tiny wildfire publisher (Pi)
# Needs: pip install paho-mqtt requests ; run Mosquitto broker on the Pi.
import time, requests
from paho.mqtt import client as mqtt

BROKER   = "127.0.0.1"      # Pi running mosquitto
TOPIC    = "fires"          # base topic
DAYS     = 1                # last N days
MAP_KEY  = "0486f2822b77feeeddec7e83b1c3f73b"
URL      = f"https://firms.modaps.eosdis.nasa.gov/api/area/csv/{MAP_KEY}/VIIRS_NOAA20_NRT/world/{DAYS}"

# Rough continent boxes: (lat_min, lat_max, lon_min, lon_max)
BOX = {
 "europe": (34, 72, -25,  60),
 "asia":   (-11, 81,  60, 180),
 "africa": (-36, 38, -20,  52),
 "north_america": (5, 83, -170, -50),
 "south_america": (-56, 13, -82, -35),
 "australia": (-50, -10, 110, 180)
}

def counts_last_days():
    r = requests.get(URL, timeout=30)
    r.raise_for_status()
    c = {k:0 for k in BOX}
    first = True
    for line in r.iter_lines(decode_unicode=True):
        if not line: continue
        if first: first = False; continue  # skip header
        parts = line.split(",")
        if len(parts) < 2: continue
        try:
            lat, lon = float(parts[0]), float(parts[1])
        except: 
            continue
        for name,(la,LA,lo,LO) in BOX.items():
            if la <= lat <= LA and lo <= lon <= LO:
                c[name] += 1
                break
    return c

def main():
    m = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id="pi-fires-pub")
    m.connect(BROKER, 1883, 60)
    while True:
        try:
            c = counts_last_days()
            # publish per-continent (no JSON parsing needed on ESP32)
            for k,v in c.items():
                m.publish(f"{TOPIC}/{k}", str(v), qos=1, retain=True)
            print("Published:", c)
        except Exception as e:
            print("Error:", e)
        time.sleep(300)   # every 5 min

if __name__ == "__main__":
    main()
