## 2024-05-22 - [Arbitrary File Read in LogTools]
**Vulnerability:** The `LogTools.readOutputLog` function allowed reading any file on the system if its path was provided in `logPath`. The validation only checked if the file existed and was a file, but did not enforce the `.log` extension or the `Saved/Logs` directory restriction, despite the documentation/memory claiming otherwise.
**Learning:** Comments or external documentation (memory) are not reliable sources of truth for security guarantees. Always verify implementation details. `path.resolve` alone does not prevent access to sensitive files if the initial input is not restricted.
**Prevention:** Explicitly validate file extensions and ensure the resolved path starts with an allowed root directory. Use `path.normalize` and check prefix matching to prevent traversal attacks.

## 2026-01-01 - [Insecure GraphQL CORS Configuration]
**Vulnerability:** The GraphQL server allowed `origin: '*'` combined with `credentials: true`. This configuration allows any website to make authenticated requests to the server if the user is logged in (though typically blocked by modern browsers, it's a dangerous default).
**Learning:** Defaulting to permissive CORS (`*`) for development convenience can lead to security risks if credentials are also enabled via environment variables or configuration without validation.
**Prevention:** Enforce strict validation in the server configuration logic to mutually exclude `origin: '*'` and `credentials: true`. Fail securely by disabling credentials when wildcard origin is present.

## 2025-05-20 - [Path Traversal in Screenshot Filenames]
**Vulnerability:** The `takeScreenshot` function sanitized filenames by replacing invalid Windows characters but allowed path separators (`/` and `\`). This enabled path traversal attacks, allowing screenshots to be written to arbitrary locations.
**Learning:** Simply replacing "invalid" characters is insufficient for security. Specifically, allowing path separators enables traversal. Sanitization must address the specific vulnerability (directory traversal) by stripping directory components or strictly enforcing an allowlist.
**Prevention:** Use `path.basename()` to strip all directory information from user-supplied filenames. Combine this with strict character sanitization to ensure the resulting filename is safe for the filesystem.

## 2025-05-24 - [Command Injection Bypass via Whitespace]
**Vulnerability:** The `CommandValidator` used simple string inclusion (`includes('rm ')`) to block dangerous commands. This could be bypassed using tabs (`rm\t`) or other separators, allowing execution of forbidden commands if the underlying system normalized the whitespace.
**Learning:** Simple string matching with hardcoded spaces is insufficient for blocking commands in systems that accept flexible whitespace.
**Prevention:** Use Regular Expressions with word boundaries (`\b`) or explicit whitespace classes (`\s+`) to match tokens robustly.

## 2025-05-30 - [Path Traversal in GraphQL Resolvers]
**Vulnerability:** The GraphQL resolvers accepted file paths and asset names directly from user input and passed them to the `AutomationBridge` without sanitization. This could allow path traversal attacks or injection of invalid characters if the underlying C++ or automation layer did not strictly validate inputs.
**Learning:** Middleware (like GraphQL resolvers) must validate and sanitize inputs at the boundary before passing them to internal services, even if those services (like `AssetTools`) have their own validation, because the GraphQL layer might bypass those specific service classes and call the bridge directly.
**Prevention:** Apply strict sanitization helpers (`sanitizePath`, `sanitizeAssetName`) to all user-controlled path and name arguments in GraphQL resolvers.
