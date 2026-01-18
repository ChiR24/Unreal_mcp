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

## 2025-05-25 - [GraphQL Path Traversal via Direct Bridge Access]
**Vulnerability:** GraphQL resolvers (e.g., `duplicateAsset`, `moveAsset`) bypassed the `AssetTools` security layer and communicated directly with `AutomationBridge` without sanitizing input paths. This allowed path traversal (`../../`) via GraphQL mutations.
**Learning:** Layering security in "Tools" classes is insufficient if the API layer (GraphQL resolvers) bypasses them for performance or convenience. Security must be enforced at the entry point (Resolver) or the lowest common denominator (Bridge) if possible, but definitely at the API boundary.
**Prevention:** Enforce strict path sanitization (`sanitizePath`) in all GraphQL resolvers that accept file paths, before passing data to internal bridges.

## 2025-05-26 - [GraphQL List Filter Path Traversal]
**Vulnerability:** The `listAssets` and `blueprints` GraphQL resolvers accepted a `pathStartsWith` filter that was passed directly to the automation bridge without sanitization. This could potentially allow path traversal or listing of unauthorized directories if the backend implementation didn't strictly validate the root.
**Learning:** Argument filtering objects often bypass main resolver logic. If a filter property acts as a file path (even a prefix), it must be sanitized with the same rigor as direct path arguments.
**Prevention:** Explicitly sanitize `pathStartsWith` and similar filter properties using `sanitizePath` before passing them to the bridge. Ensure validation happens *before* any try/catch blocks that might suppress the error.
