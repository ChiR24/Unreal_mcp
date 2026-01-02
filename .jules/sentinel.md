## 2025-12-28 - Arbitrary File Read Vulnerability in LogTools

**Vulnerability:** The `read_log` action (via `LogTools.readOutputLog`) accepted an `override` (or `logPath`) parameter that was directly passed to `fs.stat` and `fs.open` without validation. This allowed reading any file on the system (e.g., `package.json`, `.env`) by providing its absolute or relative path, provided the process had read permissions.

**Learning:** When exposing file reading capabilities, relying on "default" paths is not enough if the user can override them. Path validation must be enforced on *all* user-supplied paths. The `path.resolve` function resolves the path but does not restrict it.

**Prevention:**
1.  Enforce file extension checks (e.g., must end in `.log`).
2.  Restrict paths to a specific allowed directory whitelist (e.g., `Saved/Logs`).
3.  Use `path.resolve` and check if the result starts with the allowed root directory.
