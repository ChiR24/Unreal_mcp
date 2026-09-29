// The content-root allowlist: the static roots plus the mounts the connected
// editor reports. A plugin or game-feature mount (`/ShooterCore`) used to be
// refused by every path check unless it was listed by hand in
// MCP_ADDITIONAL_PATH_PREFIXES.

import { afterEach, describe, expect, it } from 'vitest';

import {
  clearEditorContentRoots,
  getContentRootsRevision,
  getEditorContentRoots,
  isUnderAllowedContentRoot,
  normalizeEditorContentRoots,
  sanitizePath,
  setEditorContentRoots,
} from './path-security.js';

const BACKSLASH = String.fromCharCode(92);

afterEach(() => {
  clearEditorContentRoots();
});

describe('normalizeEditorContentRoots', () => {
  it('keeps single-segment mount roots and strips a trailing slash', () => {
    expect(normalizeEditorContentRoots(['/Game', '/ShooterCore', '/Engine/'])).toEqual([
      '/Game',
      '/ShooterCore',
      '/Engine',
    ]);
  });

  it('drops values that are not a single-segment mount root', () => {
    expect(normalizeEditorContentRoots([
      42,
      null,
      { root: '/Game' },
      '',
      '/',
      '/A/B',
      '/../x',
      '//x',
      'C:/x',
      `C:${BACKSLASH}x`,
      'Game',
      '/Name With Space',
    ])).toEqual([]);
  });

  it('drops host directory roots', () => {
    expect(normalizeEditorContentRoots(['/tmp', '/Users', '/home', '/Game'])).toEqual(['/Game']);
  });

  it('dedupes case-insensitively, keeping the first spelling', () => {
    expect(normalizeEditorContentRoots(['/ShooterCore', '/shootercore', '/SHOOTERCORE/'])).toEqual([
      '/ShooterCore',
    ]);
  });

  it('keeps at most 1024 roots', () => {
    const many = Array.from({ length: 1100 }, (_, index) => `/Root${index}`);
    const roots = normalizeEditorContentRoots(many);
    expect(roots).toHaveLength(1024);
    expect(roots[0]).toBe('/Root0');
    expect(roots[1023]).toBe('/Root1023');
  });

  it('returns no roots for a value that is not an array', () => {
    expect(normalizeEditorContentRoots(undefined)).toEqual([]);
    expect(normalizeEditorContentRoots('/Game')).toEqual([]);
  });
});

describe('editor-reported content roots', () => {
  it('bumps the revision only when the set of roots changes', () => {
    const start = getContentRootsRevision();
    setEditorContentRoots(['/Game', '/ShooterCore']);
    expect(getContentRootsRevision()).toBe(start + 1);
    setEditorContentRoots(['/Game', '/ShooterCore']);
    expect(getContentRootsRevision()).toBe(start + 1);
    setEditorContentRoots(['/Game']);
    expect(getContentRootsRevision()).toBe(start + 2);
    expect(getEditorContentRoots()).toEqual(['/Game']);
  });

  it('lets sanitizePath accept a reported mount, and refuses it again once cleared', () => {
    // Called before the roots are set, so the default-roots cache is warm and
    // the test proves the update invalidates it.
    expect(() => sanitizePath('/ShooterCore/X')).toThrow(/must start with one of/);

    setEditorContentRoots(['/Game', '/ShooterCore']);
    expect(sanitizePath('/ShooterCore/X')).toBe('/ShooterCore/X');

    clearEditorContentRoots();
    expect(getEditorContentRoots()).toEqual([]);
    expect(() => sanitizePath('/ShooterCore/X')).toThrow(/must start with one of/);
  });

  it('still uses an explicit allowedRoots list as given', () => {
    setEditorContentRoots(['/ShooterCore']);
    expect(() => sanitizePath('/ShooterCore/X', ['/Game'])).toThrow(/must start with one of/);
  });

  it('matches a reported root case-insensitively only while it is reported', () => {
    expect(isUnderAllowedContentRoot('/shootercore/x')).toBe(false);
    setEditorContentRoots(['/ShooterCore']);
    expect(isUnderAllowedContentRoot('/shootercore/x')).toBe(true);
    expect(isUnderAllowedContentRoot('/ShooterCore')).toBe(true);
    expect(isUnderAllowedContentRoot('/ShooterCoreX/x')).toBe(false);
    clearEditorContentRoots();
    expect(isUnderAllowedContentRoot('/shootercore/x')).toBe(false);
    expect(isUnderAllowedContentRoot('/Game/x')).toBe(true);
  });
});
