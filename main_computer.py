import json
import time
from datetime import datetime, timezone
from sgp4.api import Satrec, jday

from config import TARGET_SATELLITE, OBSERVER_LAT, OBSERVER_LON, OBSERVER_ALT, UPDATE_PERIOD
from tle_fetcher import fetch_tles
from tle_parser import parse_tle
from coord_transforms import get_az_el_from_teme

def main():
    tle_text = fetch_tles()
    if not tle_text:
        print("[ERROR] No TLE data available.")
        return

    name, l1, l2 = parse_tle(TARGET_SATELLITE, tle_text)
    sat = Satrec.twoline2rv(l1, l2)

    print(f"[INFO] Tracking {TARGET_SATELLITE} using SGP4.\n")

    while True:
        now = datetime.now(timezone.utc)
        jd, fr = jday(now.year, now.month, now.day, now.hour, now.minute, now.second + now.microsecond * 1e-6)

        # Propagate
        e, r, v = sat.sgp4(jd, fr)
        if e != 0:
            print(f"[WARN] Propagation error {e}")
            continue

        # Satellite ECI position (km)
        x, y, z = r

        az, el = get_az_el_from_teme(r, now, OBSERVER_LAT, OBSERVER_LON, OBSERVER_ALT)

        # Prepare JSON payload
        data = {
            "timestamp_utc": now.isoformat(),
            "satellite": TARGET_SATELLITE,
            "azimuth_deg": round(az, 3),
            "elevation_deg": round(el, 3),
        }

        print(json.dumps(data))

        time.sleep(UPDATE_PERIOD)


if __name__ == "__main__":
    main()



