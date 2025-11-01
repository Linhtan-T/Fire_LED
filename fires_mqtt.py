#!/usr/bin/env python3
"""
fires_mqtt.py — Publish NASA FIRMS fire counts to MQTT (per continent)

Usage (on Raspberry Pi):
  pip3 install requests paho-mqtt
  # Make sure an MQTT broker is running (e.g., Mosquitto on the Pi)
  python3 fires_mqtt.py --map-key YOUR_FIRMS_KEY --broker 192.168.1.70 --days 1 --period 300

This script:
- Pulls VIIRS fire detections (NRT) for the whole world from NASA FIRMS
- Buckets counts per continent (rough boxes, fast & non-overlapping)
- Publishes each continent's count as a retained message to topics:
    fires/asia, fires/africa, fires/north_america, fires/europe, fires/australia, fires/south_america
- Also publishes a combined JSON to fires/all
- Sets an LWT (last will) on fires/status/pi -> "offline" and sends "online" when connected
"""

import argparse
import time
import sys
import signal
from typing import Dict, Tuple
import requests
import paho.mqtt.client as mqtt
import json

# ---- Continent boxes (rough, non-overlapping) ----
EUROPE_LAT, EUROPE_LON       = (34, 72), (-25, 60)
ASIA_LAT,   ASIA_LON         = (-11, 81), (60, 180)
AFRICA_LAT, AFRICA_LON       = (-36, 38), (-20, 52)
NA_LAT,     NA_LON           = (5, 83),   (-170, -50)
GREENLAND_LAT, GREENLAND_LON = (58, 83),  (-73, -12)  # excluded from NA box above
SA_LAT,     SA_LON           = (-56, 13), (-82, -35)
AU_LAT,     AU_LON           = (-44, -8), (110, 155)  # Australia/Oceania focus (simple)

CONTINENTS = {
    "asia":          (ASIA_LAT, ASIA_LON),
    "africa":        (AFRICA_LAT, AFRICA_LON),
    "north_america": (NA_LAT, NA_LON),
    "europe":        (EUROPE_LAT, EUROPE_LON),
    "australia":     (AU_LAT, AU_LON),
    "south_america": (SA_LAT, SA_LON),
}

def in_box(lat: float, lon: float, lat_rng: Tuple[float, float], lon_rng: Tuple[float, float]) -> bool:
    return (lat_rng[0] <= lat <= lat_rng[1]) and (lon_rng[0] <= lon <= lon_rng[1])

def continent_for(lat: float, lon: float) -> str:
    # Greenland special-case to avoid double counting into NA
    if in_box(lat, lon, GREENLAND_LAT, GREENLAND_LON):
        return ""  # ignore
    for name, (LAT, LON) in CONTINENTS.items():
        if in_box(lat, lon, LAT, LON):
            return name
    return ""

def fetch_counts(map_key: str, days: int, timeout: int = 20) -> Dict[str, int]:
    url = f"https://firms.modaps.eosdis.nasa.gov/api/area/csv/{map_key}/VIIRS_NOAA20_NRT/world/{days}"
    r = requests.get(url, stream=True, timeout=timeout)
    r.raise_for_status()
    counts = {k: 0 for k in CONTINENTS.keys()}
    first = True
    for raw in r.iter_lines(decode_unicode=True):
        if not raw:
            continue
        if first:
            first = False
            continue  # header
        parts = raw.split(",")
        if len(parts) < 2:
            continue
        try:
            lat = float(parts[0]); lon = float(parts[1])
        except ValueError:
            continue
        c = continent_for(lat, lon)
        if c:
            counts[c] += 1
    return counts

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--map-key", required=True, help="NASA FIRMS API key")
    ap.add_argument("--broker", default="127.0.0.1", help="MQTT broker host/IP")
    ap.add_argument("--port", type=int, default=1883, help="MQTT broker port")
    ap.add_argument("--base-topic", default="fires", help="Base MQTT topic")
    ap.add_argument("--days", type=int, default=1, help="Lookback days (1..7)")
    ap.add_argument("--period", type=int, default=300, help="Publish period in seconds")
    ap.add_argument("--client-id", default="fires-publisher", help="MQTT client ID")
    ap.add_argument("--username", default=None, help="MQTT username (optional)")
    ap.add_argument("--password", default=None, help="MQTT password (optional)")
    args = ap.parse_args()

    args.days = max(1, min(args.days, 7))

    status_topic = f"{args.base_topic}/status/pi"
    all_topic    = f"{args.base_topic}/all"

    client = mqtt.Client(client_id=args.client_id, clean_session=True)
    client.will_set(status_topic, payload="offline", qos=1, retain=True)
    if args.username is not None:
        client.username_pw_set(args.username, args.password)

    stop = False
    def _sig(*_):
        nonlocal stop
        stop = True
    signal.signal(signal.SIGINT, _sig)
    signal.signal(signal.SIGTERM, _sig)

    backoff = 1
    while not stop:
        try:
            client.connect(args.broker, args.port, keepalive=30)
            client.loop_start()
            client.publish(status_topic, "online", qos=1, retain=True)
            backoff = 1  # reset backoff after a good connect

            while not stop:
                t0 = time.time()
                try:
                    counts = fetch_counts(args.map_key, args.days, timeout=25)
                    ts = int(time.time())
                    # Publish per-continent (retained)
                    for k, v in counts.items():
                        client.publish(f"{args.base_topic}/{k}", str(v), qos=1, retain=True)
                    # Publish combined JSON
                    payload = {"counts": counts, "ts": ts, "days": args.days}
                    client.publish(all_topic, json.dumps(payload), qos=1, retain=True)
                    # heartbeat
                    client.publish(f"{args.base_topic}/status/heartbeat", str(ts), qos=0, retain=False)
                    # sleep the remainder of the period
                    elapsed = time.time() - t0
                    sleep_s = max(1.0, args.period - elapsed)
                    for _ in range(int(sleep_s)):
                        if stop: break
                        time.sleep(1)
                    if stop: break
                except requests.RequestException as e:
                    sys.stderr.write(f"[fetch] network error: {e}\n")
                    time.sleep(5)
                except Exception as e:
                    sys.stderr.write(f"[fetch] unexpected error: {e}\n")
                    time.sleep(5)

        except Exception as e:
            sys.stderr.write(f"[mqtt] connect error: {e}\n")
            client.loop_stop()
            try:
                client.disconnect()
            except Exception:
                pass
            time.sleep(backoff)
            backoff = min(backoff * 2, 30)

    client.loop_stop()
    try:
        client.publish(status_topic, "offline", qos=1, retain=True)
        client.disconnect()
    except Exception:
        pass

if __name__ == "__main__":
    main()
