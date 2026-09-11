"""Deterministic simulated-weather feature generation.

The SRS requests simulated temperature and precipitation features. These
values are deliberately deterministic so rebuilding the training set produces
identical data from the same source database.
"""

from __future__ import annotations

import hashlib
import math
from datetime import datetime


def _stable_noise(key: str) -> float:
    """Map a stable string to approximately [-1, 1]."""
    digest = hashlib.sha256(key.encode("utf-8")).digest()
    value = int.from_bytes(digest[:8], "big") / float(2**64 - 1)
    return value * 2.0 - 1.0


def simulated_weather(station_id: int, when: datetime) -> tuple[float, int]:
    """Return deterministic (temperature_c, precipitation_flag).

    The model is intentionally simple: annual seasonality + daily cycle + a
    station-specific offset, with a deterministic perturbation. Precipitation
    is a binary simulated event so the feature matches "是否降水" directly.
    """
    day_of_year = when.timetuple().tm_yday
    hour = when.hour

    seasonal = 10.5 * math.sin(2.0 * math.pi * (day_of_year - 80) / 365.25)
    daily = 3.0 * math.sin(2.0 * math.pi * (hour - 6) / 24.0)
    station_offset = ((station_id * 17) % 9) - 4
    noise = 1.5 * _stable_noise(f"temp:{station_id}:{when:%Y-%m-%d:%H}")
    temperature_c = 14.0 + seasonal + daily + station_offset * 0.15 + noise

    rain_score = _stable_noise(f"rain:{station_id}:{when:%Y-%m-%d:%H}")
    precipitation = int(rain_score > 0.62)
    return round(temperature_c, 2), precipitation
