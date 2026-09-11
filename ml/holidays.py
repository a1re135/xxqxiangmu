"""Chinese public-holiday date helpers used by the ML data-preparation stage.

The project specification requires a holiday flag but does not provide an
external holiday calendar dependency. The configured 2026 dates below come
from the State Council's published 2026 holiday arrangement. Additional years
can be added here without changing the feature pipeline.
"""

from __future__ import annotations

from datetime import date


# 2026 statutory public-holiday dates published by the State Council.
# Make-up working days are intentionally not marked as holidays.
CHINA_PUBLIC_HOLIDAYS: frozenset[date] = frozenset(
    {
        # New Year's Day
        date(2026, 1, 1), date(2026, 1, 2), date(2026, 1, 3),
        # Spring Festival
        date(2026, 2, 15), date(2026, 2, 16), date(2026, 2, 17),
        date(2026, 2, 18), date(2026, 2, 19), date(2026, 2, 20),
        date(2026, 2, 21), date(2026, 2, 22), date(2026, 2, 23),
        # Qingming Festival
        date(2026, 4, 4), date(2026, 4, 5), date(2026, 4, 6),
        # Labor Day
        date(2026, 5, 1), date(2026, 5, 2), date(2026, 5, 3),
        date(2026, 5, 4), date(2026, 5, 5),
        # Dragon Boat Festival
        date(2026, 6, 19), date(2026, 6, 20), date(2026, 6, 21),
        # Mid-Autumn Festival
        date(2026, 9, 25), date(2026, 9, 26), date(2026, 9, 27),
        # National Day
        date(2026, 10, 1), date(2026, 10, 2), date(2026, 10, 3),
        date(2026, 10, 4), date(2026, 10, 5), date(2026, 10, 6),
        date(2026, 10, 7),
    }
)


def is_public_holiday(day: date) -> bool:
    """Return whether *day* is in the configured statutory holiday set."""
    return day in CHINA_PUBLIC_HOLIDAYS
