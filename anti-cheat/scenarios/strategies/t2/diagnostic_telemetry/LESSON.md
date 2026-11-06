# diagnostic_telemetry — Message 159 Diagnostic Telemetry

Family: Detection. Tiers: T2. Area: DllStatusResponse.

## Battlefield
Red: Produces simulated module, PE timestamp, and thread-origin anomalies.
Blue: Runs the CS2 diagnostic orchestrator to collect module inventory, PE metadata,
thread captures, and the response state for server-side correlation.

## Takeaway
Message 159-style telemetry is valuable because independent traces can be correlated
instead of making a decision from any single field.
