export function sanitizePath(path: string, allowedRoots: string[] | null = null): string {
    if (!path || typeof path !== 'string') {
        throw new Error('Invalid path: must be a non-empty string');
    }

    const trimmed = path.trim();
    if (trimmed.length === 0) {
        throw new Error('Invalid path: cannot be empty');
    }

    // Normalize separators
    let normalized = trimmed.replace(/\\/g, '/');

    // Normalize double slashes (prevents engine crash from paths like /Game//Test)
    while (normalized.includes('//')) {
        normalized = normalized.replace(/\/\//g, '/');
    }

    // Prevent directory traversal
    if (normalized.includes('..')) {
        throw new Error('Invalid path: directory traversal (..) is not allowed');
    }

    // Must start with / and have a valid root segment
    // The C++ side validates against engine's registered mount points (FPackageName::IsValidLongPackageName),
    // so we only perform basic security checks here to avoid blocking plugin paths like /ShooterCore/, etc.
    if (!normalized.startsWith('/') || normalized.length < 2) {
        throw new Error('Invalid path: must start with /RootName');
    }

    // If explicit allowedRoots are provided, enforce them
    if (allowedRoots) {
        const isAllowed = allowedRoots.some(root =>
            normalized.toLowerCase() === root.toLowerCase() ||
            normalized.toLowerCase().startsWith(`${root.toLowerCase()}/`)
        );

        if (!isAllowed) {
            throw new Error(`Invalid path: must start with one of [${allowedRoots.join(', ')}]`);
        }
    }

    // Basic character validation (Unreal strictness)
    // Blocks: < > : " | ? * (Windows reserved) and control characters
    // eslint-disable-next-line no-control-regex
    const invalidChars = /[<>:"|?*\x00-\x1f]/;
    if (invalidChars.test(normalized)) {
        throw new Error('Invalid path: contains illegal characters');
    }

    return normalized;
}
