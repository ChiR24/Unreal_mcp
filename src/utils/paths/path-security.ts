import { getAdditionalPathPrefixes } from '../../config.js';
import { HOST_PATH_PATTERN, UE_CONTENT_ROOTS } from './content-path-policy.js';

/**
 * A mount root as the editor reports it: one identifier segment, no trailing slash.
 * QueryRootContentPaths reports single-segment roots (`/Game`, `/ShooterCore`); anything
 * else is dropped rather than guessed at.
 */
const EDITOR_ROOT_SHAPE = /^\/[A-Za-z0-9_][A-Za-z0-9_-]*$/u;

/** Upper bound on editor-reported roots, so a malformed report cannot grow every check. */
const MAX_EDITOR_ROOTS = 1024;

let editorContentRoots: readonly string[] = [];
let contentRootsRevision = 0;
let cachedDefaultRoots: { revision: number; roots: string[] } | undefined;

function normalizeRoots(sourceRoots: readonly string[]): string[] {
    return [...new Set(
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
}

/**
 * The editor-reported roots from a `bridge_ack` or `content_roots_changed` payload,
 * validated: strings only, single-segment mount shape, not a host directory
 * (HOST_PATH_PATTERN), deduplicated case-insensitively, at most MAX_EDITOR_ROOTS.
 */
export function normalizeEditorContentRoots(raw: unknown): string[] {
    if (!Array.isArray(raw)) return [];
    const seen = new Set<string>();
    const roots: string[] = [];
    for (const entry of raw) {
        if (roots.length >= MAX_EDITOR_ROOTS) break;
        if (typeof entry !== 'string') continue;
        const root = entry.trim().replace(/\/+$/u, '');
        if (!EDITOR_ROOT_SHAPE.test(root) || HOST_PATH_PATTERN.test(root)) continue;
        const key = root.toLowerCase();
        if (seen.has(key)) continue;
        seen.add(key);
        roots.push(root);
    }
    return roots;
}

/**
 * Replace the content roots the connected editor reports. Called with the
 * `contentRoots` of `bridge_ack` and of each `content_roots_changed` event; an
 * absent or invalid value clears them, which falls back to the static roots.
 * Every root cache is invalidated when the set actually changes.
 */
export function setEditorContentRoots(raw: unknown): void {
    const next = normalizeEditorContentRoots(raw);
    const same = next.length === editorContentRoots.length
        && next.every((root, index) => root === editorContentRoots[index]);
    if (same) return;
    editorContentRoots = next;
    contentRootsRevision += 1;
    cachedDefaultRoots = undefined;
}

/** Forget the editor-reported roots (the bridge socket closed). */
export function clearEditorContentRoots(): void {
    setEditorContentRoots([]);
}

/** The roots the connected editor reported, validated; empty when none is connected. */
export function getEditorContentRoots(): readonly string[] {
    return editorContentRoots;
}

/** Increments whenever the editor-reported roots change; caches built from the roots compare against it. */
export function getContentRootsRevision(): number {
    return contentRootsRevision;
}

/** The roots that do not depend on a connected editor: UE_CONTENT_ROOTS plus MCP_ADDITIONAL_PATH_PREFIXES. */
export function getConfiguredContentRoots(): string[] {
    return normalizeRoots([
        ...UE_CONTENT_ROOTS,
        ...getAdditionalPathPrefixes().map(p => p.replace(/\/$/, '')),
    ]);
}

/** The one content-root allowlist: the configured roots plus the roots the connected editor reports. */
export function getContentRoots(): readonly string[] {
    return getDefaultRoots();
}

/** True when `value` is a content root, or a path under one, compared case-insensitively. */
export function isUnderAllowedContentRoot(value: string): boolean {
    const lower = value.toLowerCase();
    return getDefaultRoots().some(root => {
        const rootLower = root.toLowerCase();
        return lower === rootLower || lower.startsWith(`${rootLower}/`);
    });
}

function getDefaultRoots(): string[] {
    if (!cachedDefaultRoots || cachedDefaultRoots.revision !== contentRootsRevision) {
        cachedDefaultRoots = {
            revision: contentRootsRevision,
            roots: normalizeRoots([...getConfiguredContentRoots(), ...editorContentRoots]),
        };
    }
    return cachedDefaultRoots.roots;
}

/**
 * Normalize a UE content path and check it against `allowedRoots`, or, when none
 * are given, against the content-root allowlist (getContentRoots). Throws on
 * traversal, an unknown root or illegal characters; returns the normalized path.
 */
export function sanitizePath(path: string, allowedRoots?: string[]): string {
    const normalizedRoots = allowedRoots ? normalizeRoots(allowedRoots) : getDefaultRoots();
    if (normalizedRoots.length === 0) {
        throw new Error('Invalid allowedRoots: no valid roots configured');
    }
    if (!path || typeof path !== 'string') {
        throw new Error('Invalid path: must be a non-empty string');
    }

    const trimmed = path.trim();
    if (trimmed.length === 0) {
        throw new Error('Invalid path: cannot be empty');
    }

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
    const normalizedLower = normalized.toLowerCase();
    const isAllowed = normalizedRoots.some(root =>
        normalizedLower === root.toLowerCase() ||
        normalizedLower.startsWith(`${root.toLowerCase()}/`)
    );

    if (!isAllowed) {
        throw new Error(`Invalid path: must start with one of [${normalizedRoots.join(', ')}]`);
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
