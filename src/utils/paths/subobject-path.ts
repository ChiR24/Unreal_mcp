import { normalizeContentToGame, sanitizePath } from './path-security.js';

// Lexical counterpart of Foundation/BridgeHelpers/Security/McpSubobjectPath.h.
// This is syntax/root validation, never a copy of a principal's private grants.
export function normalizeSubobjectPath(input: string): string {
    // eslint-disable-next-line no-control-regex
    if (/[\x00-\x1f\x7f]/u.test(input)) throw new Error('Invalid object path: control character');
    let value = input.trim().replace(/\\/gu, '/');
    if (!value.startsWith('/') || value.includes('//') || value.includes('..')) {
        throw new Error('Invalid object path: noncanonical separators or traversal');
    }
    value = normalizeContentToGame(value);
    const dot = value.indexOf('.');
    const colon = value.indexOf(':');
    if (dot < 0 || colon <= dot + 1 || colon >= value.length - 1) {
        throw new Error('Invalid object path: package, object and subobject are required');
    }
    const packagePath = value.slice(0, dot);
    if (packagePath.indexOf('/', 1) < 0 || packagePath.endsWith('/') ||
        /[\s\\:*?"<>|' ,.&!~@#]/u.test(packagePath)) {
        throw new Error('Invalid object path: invalid package');
    }
    sanitizePath(packagePath);
    const names = [value.slice(dot + 1, colon), ...value.slice(colon + 1).split('.')];
    if (names.some(name => name.length === 0 || name.length >= 1024 ||
        /[\s"' ,/.:|&!~@#(){}[\]=;^%$`\\<>?*]/u.test(name))) {
        throw new Error('Invalid object path: invalid object name');
    }
    return value;
}
