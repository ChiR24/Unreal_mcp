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

## 2026-01-20 - [Command Injection via String Interpolation in UITools]
**Vulnerability:** `UITools` methods (e.g., `createMenu`, `createTooltip`) constructed console commands using string interpolation with user-provided text (like button labels). This allowed attackers to break out of quoted strings and potentially inject additional commands or arguments (e.g., `"; Quit; "`).
**Learning:** Relying on basic string quoting for console commands is unsafe if the input itself can contain quotes. `CommandValidator` only checks for known dangerous commands but doesn't prevent argument injection or syntax breaking within valid commands.
**Prevention:** Implement and use a dedicated `sanitizeConsoleString` utility that escapes or replaces quotes (`"`) and removes newlines before interpolating user input into command strings. Always treat user-facing text as untrusted when building command lines.

## 2025-03-07 - Path Traversal Bypass via `startsWith()`
**Vulnerability:** A directory traversal check in `validateSnapshotPath` used `resolvedPath.startsWith(cwd)` to enforce that user paths stayed inside the project root. This allows an attacker to access sibling directories that share the same prefix (e.g., if `cwd` is `/projects/app`, accessing `/projects/app-secrets` bypasses the check).
**Learning:** Checking string prefixes for directory boundaries is insecure if the string lacks a trailing path separator, because string matching does not respect directory boundaries.
**Prevention:** Always append a trailing path separator (e.g., `cwd + path.sep`) before using `startsWith()` for path boundary enforcement, or use strict directory checks using `path.relative` ensuring it doesn't start with `..`.

## 2025-03-13 - Path Traversal in Export Asset Command
**Vulnerability:** The `exportPath` argument in `McpAutomationBridge_SystemControlHandlers.cpp` was used directly to create export directories and save assets without sanitization, leading to an arbitrary file write/path traversal vulnerability.
**Learning:** Native Unreal Engine functions like `FPaths::GetPath` and `UExporter::ExportToFile` do not perform bounding checks. Passing user-controlled paths directly to them allows escaping the project directory.
**Prevention:** Always use `SanitizeProjectFilePath` for user-provided paths that interact with the host filesystem. Then, concatenate it safely using `FPaths::ProjectDir() / SafePath`, and always check that the resulting absolute path resides within the project directory using `StartsWith(NormalizedProjectDir, ESearchCase::IgnoreCase)`.

## 2025-03-14 - Absolute Path Bypass in File Output Handlers
**Vulnerability:** Several handlers (`generate_thumbnail`, `generate_report`) used `FPaths::IsRelative()` to check paths before converting to absolute. However, providing an absolute path like `C:/Windows/System32/exploit.dll` would bypass this check entirely, allowing arbitrary file writes.
**Learning:** Checking `IsRelative()` alone is insufficient for security - it only handles relative paths. Absolute paths bypass the check and are used directly without bounds validation.
**Prevention:** Always use `SanitizeProjectFilePath` to reject absolute paths (it checks for `:` character), then construct the absolute path from `FPaths::ProjectDir() / SafePath` and verify bounds. Never trust user-provided paths directly, regardless of whether they appear relative or absolute.

## 2024-05-24 - Unsanitized AssetPath in SystemControlHandlers
**Vulnerability:** In `McpAutomationBridge_SystemControlHandlers.cpp`, the `export_asset` sub-action accepted a raw `AssetPath` from JSON payload and passed it directly to `UEditorAssetLibrary::DoesAssetExist` and `UEditorAssetLibrary::LoadAsset`.
**Learning:** This could allow directory traversal or invalid character injection into the engine's asset loading system, potentially causing crashes or unauthorized file access.
**Prevention:** Always use `SanitizeProjectRelativePath()` for any user-provided asset path before using it in engine functions. Ensure the sanitized path is checked for emptiness, and always use the sanitized variable (e.g., `SafeAssetPath`) for all subsequent logic, including string manipulation like `GetBaseFilename` and in response JSONs.

## 2024-03-24 - Missing Level Export Path Sanitization
**Vulnerability:** The `export_level` handler in `McpAutomationBridge_LevelHandlers.cpp` directly used the user-provided `exportPath` in directory creation (`IFileManager::Get().MakeDirectory`) and level saving operations without sanitizing it first.
**Learning:** File paths passed to Unreal Engine's save and export functions must be sanitized to prevent directory traversal and arbitrary file write vulnerabilities. The handler failed to use the type-specific `SanitizeProjectRelativePath` validator for `ExportPath` (which acts as a package/asset path in this context) before operating on it.
**Prevention:** Always validate and sanitize user-provided file and asset paths at the final boundary before performing operations. Use `SanitizeProjectRelativePath` for package/asset paths. Also, remember that `IFileManager::Get().MakeDirectory` expects a filesystem path, not an asset path, so always convert the sanitized asset path to an absolute filesystem path (using `FPackageName::TryConvertLongPackageNameToFilename`) before creating directories based on it.
