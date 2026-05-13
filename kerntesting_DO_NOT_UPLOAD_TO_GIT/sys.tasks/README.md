# sys.tasks

Task utility stdlib for retries, idempotency keys, and schedule planning.

## API

- `backoff_ms(attempt, baseMs, maxMs)`
- `run_retry_plan(taskName, maxAttempts, baseMs, maxMs)`
- `idempotency_key(namespace, payload)`
- `next_run_after(secondsFromNow)`
- `schedule_window(startTs, everySeconds, count)`
- `log_plan(logger, planResult)`

## Quick use

```kn
let tasks = import("sys.tasks")
let plan = tasks["run_retry_plan"]("charge", 5, 200, 5000)
print(json_stringify(plan, 2))
```
