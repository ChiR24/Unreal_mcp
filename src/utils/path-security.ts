import { getAdditionalPathPrefixes } from '../config.js';

export function sanitizePath(path: string, allowedRoots?: string[]): string {
    // Default roots: all standard UE paths plus configured additional prefixes
    const defaultRoots = ['/Game', '/Engine', '/Script', '/Temp'];
    const sourceRoots = allowedRoots ?? [
        ...defaultRoots,
        ...getAdditionalPathPrefixes().map(p => p.replace(/\/$/, '')),
    ];
    // Normalize and validate all roots (including caller-supplied)
    const normalizedRoots = [...new Set(
        sourceRoots
            .map(r => r.trim().replace(/\/+$/, ''))
            .filter(r =>
                r.length > 0 &&
                r.startsWith('/') &&
                r !== '/' &&
                !r.includes('..') &&
                !r.includes('//')
            )
    )];
    if (normalizedRoots.length === 0) {
        throw new Error('Invalid allowedRoots: no valid roots configured');
    }
    allowedRoots = normalizedRoots;
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
