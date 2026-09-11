# ML Subsystem — UC-M-01

This stage prepares the historical charging-load dataset required by the SRS.
It is intentionally independent of `client_user` and `client_admin`; it reads
SQLite directly and writes CSV/JSON artifacts for the later Random Forest stage.

## What UC-M-01 does

1. Reads `charging_order` joined with `charger` to obtain the charging station.
2. Uses completed orders (`status = 2`) with a valid `start_time`.
3. Aggregates `energy` by **station + date + hour** (equivalent to a station-hour bucket).
4. Fills missing station-hours with `0` kWh so lag features are truly hourly.
5. Adds the required features:
   - `hour`
   - `weekday` (Monday=0 ... Sunday=6)
   - `is_weekend`
   - `is_holiday`
   - `lag_1h`, `lag_24h`, `lag_168h`
   - `rolling_mean_24h` (previous 24 hours, current target excluded)
   - `station_encoding`
   - `temperature_c`
   - `is_precipitation`
6. Uses hourly energy (`charge_load_kwh`) as the prediction label.
7. Produces reproducible simulated-weather features and a station encoder mapping.

## Run on the existing database

From the project root:

```bash
python3 -m ml.prepare_data
```

Or specify the database explicitly:

```bash
python3 ml/prepare_data.py \
  --db ~/.local/share/NCS_Charging_Platform/charge_platform.db
```

## Generated artifacts

The command creates these files under `ml/data/`:

```text
charging_load_training.csv   # model-ready feature matrix + target
hourly_aggregated.csv        # audit-friendly hourly aggregation
station_encoder.json         # station_id -> integer encoding
dataset_metadata.json       # provenance and row/feature counts
```

No Python package beyond the standard library is required for UC-M-01.
The next stage, UC-M-02, can consume `charging_load_training.csv` directly.
