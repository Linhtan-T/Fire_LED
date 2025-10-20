from flask import Flask, jsonify, request
import requests, time

APP = Flask(__name__)

MAP_KEY = "0486f2822b77feeeddec7e83b1c3f73b"  # put your key here
FIRMS_URL = "https://firms.modaps.eosdis.nasa.gov/api/area/csv/{key}/VIIRS_NOAA20_NRT/world/{days}"

# Non-overlapping rough boxes
EUROPE_LAT, EUROPE_LON       = (34, 72), (-25, 60)
ASIA_LAT,   ASIA_LON         = (-11, 81), (60, 180)
AFRICA_LAT, AFRICA_LON       = (-36, 38), (-20, 52)
NA_LAT,     NA_LON           = (5, 83),   (-170, -50)
GREENLAND_LAT, GREENLAND_LON = (58, 83),  (-73, -12)
SA_LAT,     SA_LON           = (-56, 13), (-82, -35)
AU_LAT,     AU_LON           = (-45, -10),(110, 155)

def norm_lon(lon):
    if lon > 180: lon -= 360
    if lon < -180: lon += 360
    return lon

def inside(lat, lon, la, lo):
    x1, x2 = sorted(la); y1, y2 = sorted(lo)
    return x1 <= lat <= x2 and y1 <= lon <= y2

def detect(lat, lon):
    lon = norm_lon(lon)
    if inside(lat, lon, EUROPE_LAT, EUROPE_LON): return "europe"
    if inside(lat, lon, ASIA_LAT, ASIA_LON): return "asia"
    if inside(lat, lon, AFRICA_LAT, AFRICA_LON): return "africa"
    if inside(lat, lon, AU_LAT, AU_LON): return "australia"
    if inside(lat, lon, NA_LAT, NA_LON) or inside(lat, lon, GREENLAND_LAT, GREENLAND_LON): return "north_america"
    if inside(lat, lon, SA_LAT, SA_LON): return "south_america"
    return "unknown"

_cache = {"ts":0, "data":{}}

@APP.get("/fires")
def fires():
    # cache for 5 minutes
    if time.time() - _cache["ts"] < 300 and _cache["data"]:
        return jsonify(_cache["data"])
    days = max(1, min(int(request.args.get("days", "1")), 7))
    url = FIRMS_URL.format(key=MAP_KEY, days=days)

    r = requests.get(url, stream=True, timeout=20)
    r.raise_for_status()

    counts = {"asia":0,"africa":0,"north_america":0,"europe":0,"australia":0,"south_america":0}
    first = True
    for raw in r.iter_lines(decode_unicode=True):
        if not raw: continue
        if first: first = False; continue  # header
        parts = raw.split(",")
        if len(parts) < 2: continue
        try:
            lat = float(parts[0]); lon = float(parts[1])
        except: 
            continue
        c = detect(lat, lon)
        if c in counts: counts[c] += 1

    data = {"counts": counts, "ts": int(time.time()), "days": days}
    _cache["ts"] = data["ts"]; _cache["data"] = data
    return jsonify(data)

if __name__ == "__main__":
    APP.run(host="0.0.0.0", port=5055)
