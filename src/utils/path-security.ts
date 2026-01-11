/**
 * Strict path validation - throws on invalid input.
 * Use for internal paths where invalid values indicate bugs.
 * @param path - The path to validate
 * @param allowedRoots - Allowed path prefixes (default: ['/Game', '/Engine'])
 * @throws {Error} If path is invalid or outside allowed roots
 * @see sanitizePathSafe - Safe version for user input (returns result object)
 */
export function sanitizePath(path: string, allowedRoots: string[] = ['/Game', '/Engine']): string {
    if (!path || typeof path !== 'string') {
        throw new Error('Invalid path: must be a non-empty string');
    }

    const trimmed = path.trim();
    if (trimmed.length === 0) {
        throw new Error('Invalid path: cannot be empty');
    }

    // Normalize separators
    const normalized = trimmed.replace(/\\/g, '/');

    // Prevent directory traversal
    if (normalized.includes('..')) {
        throw new Error('Invalid path: directory traversal (..) is not allowed');
    }

    // Ensure path starts with a valid root
    // We check case-insensitive for the root prefix to be user-friendly, 
    // but Unreal paths are typically case-insensitive anyway.
    const isAllowed = allowedRoots.some(root =>
        normalized.toLowerCase() === root.toLowerCase() ||
        normalized.toLowerCase().startsWith(`${root.toLowerCase()}/`)
    );

    if (!isAllowed) {
        throw new Error(`Invalid path: must start with one of [${allowedRoots.join(', ')}]`);
    }

    // Basic character validation (Unreal strictness)
    // Blocks: < > : " | ? * (Windows reserved) and control characters
    // allowing spaces, dots, underscores, dashes, slashes
    // Note: Unreal allows spaces in some contexts but it's often safer to restrict them if strict mode is desired.
    // For now, we block the definitely invalid ones.
    // eslint-disable-next-line no-control-regex
    const invalidChars = /[<>:"|?*\x00-\x1f]/;
    if (invalidChars.test(normalized)) {
        throw new Error('Invalid path: contains illegal characters');
    }

    return normalized;
}
