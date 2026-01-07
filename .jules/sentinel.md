## 2024-05-22 - [Arbitrary File Read in LogTools]
**Vulnerability:** The `LogTools.readOutputLog` function allowed reading any file on the system if its path was provided in `logPath`. The validation only checked if the file existed and was a file, but did not enforce the `.log` extension or the `Saved/Logs` directory restriction, despite the documentation/memory claiming otherwise.
**Learning:** Comments or external documentation (memory) are not reliable sources of truth for security guarantees. Always verify implementation details. `path.resolve` alone does not prevent access to sensitive files if the initial input is not restricted.
**Prevention:** Explicitly validate file extensions and ensure the resolved path starts with an allowed root directory. Use `path.normalize` and check prefix matching to prevent traversal attacks.

## 2026-01-01 - [Insecure GraphQL CORS Configuration]
**Vulnerability:** The GraphQL server allowed `origin: '*'` combined with `credentials: true`. This configuration allows any website to make authenticated requests to the server if the user is logged in (though typically blocked by modern browsers, it's a dangerous default).
**Learning:** Defaulting to permissive CORS (`*`) for development convenience can lead to security risks if credentials are also enabled via environment variables or configuration without validation.
**Prevention:** Enforce strict validation in the server configuration logic to mutually exclude `origin: '*'` and `credentials: true`. Fail securely by disabling credentials when wildcard origin is present.
