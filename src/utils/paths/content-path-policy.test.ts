// The rejection policy four surfaces share (resource, prompt, completion and
// the asset handlers). It is consolidated precisely because it had drifted:
// the same value was refused on one surface and accepted on another. These
// cases pin the union, and every one of them is a value some copy used to let
// through.

import { describe, expect, it } from 'vitest';

import {
  ENCODED_TRAVERSAL_PATTERN,
  HOST_PATH_PATTERN,
  isContentMountShapedPath,
  isTraversalPath,
  isUnderContentRoot,
  UE_CONTENT_ROOTS,
} from './content-path-policy.js';

const BACKSLASH = String.fromCharCode(92);

describe('content path policy', () => {
  it.each([
    ['any drive letter, not just C', `D:${BACKSLASH}payload`],
    ['a forward-slash drive path', 'd:/payload'],
    ['a bare backslash anywhere', '/Game/Sub' + BACKSLASH + 'Thing'],
    ['a home shorthand', '~/secret'],
    ['the POSIX home root', '/home/me/x'],
    ['the POSIX var root', '/var/log/x'],
    ['the proc root', '/proc/self/environ'],
    ['the sys root', '/sys/class/x'],
  ])('HOST_PATH_PATTERN rejects %s', (_label, value) => {
    expect(HOST_PATH_PATTERN.test(value)).toBe(true);
  });

  it.each([
    ['a content path', '/Game/Props/SM_Rock'],
    ['a name that merely contains dots', '/Game/My..Thing'],
    // The `\b` in the pattern is what keeps these two out: /bin must not
    // swallow /binaries, and a root name is only a root at the start.
    ['a folder whose name extends a root', '/binaries/x'],
    ['a root name deeper in the path', '/Game/bin/Thing'],
  ])('HOST_PATH_PATTERN accepts %s', (_label, value) => {
    expect(HOST_PATH_PATTERN.test(value)).toBe(false);
  });

  it('treats .. as a segment, not a substring', () => {
    expect(isTraversalPath('/Game/../../etc')).toBe(true);
    expect(isTraversalPath('/Game/Sub' + BACKSLASH + '..' + BACKSLASH + 'x')).toBe(true);
    expect(isTraversalPath('/Game/My..Thing')).toBe(false);
  });

  it('catches percent-encoded traversal, single and double encoded', () => {
    expect(ENCODED_TRAVERSAL_PATTERN.test('%2e%2e/x')).toBe(true);
    expect(ENCODED_TRAVERSAL_PATTERN.test('%252e/x')).toBe(true);
    expect(ENCODED_TRAVERSAL_PATTERN.test('%2E%2E/x')).toBe(true);
    expect(ENCODED_TRAVERSAL_PATTERN.test('/Game/Ok')).toBe(false);
  });

  it('counts percent-encoded traversal as traversal for every caller', () => {
    // The prompt and completion surfaces never decode, so for them `%2e%2e`
    // used to read as an ordinary folder name. The resource reader decodes
    // once, which turns `%252e%252e` into the text `%2e%2e` -- still not a
    // `..` segment. All three now reject through this one predicate.
    expect(isTraversalPath('/Game/%2e%2e/Secret')).toBe(true);
    expect(isTraversalPath('/Game/%252e%252e/Secret')).toBe(true);
    expect(isTraversalPath('/Game/%2E%2E/Secret')).toBe(true);
    expect(isTraversalPath('/Game/Props/SM_Rock')).toBe(false);
  });

  it.each([
    ['a dot encoded on one side only', '/Game/%2e./Secret'],
    ['the other side', '/Game/.%2e/Secret'],
    ['an encoded separator after a literal ..', '/Game/..%2fSecret'],
    ['mixed case across a mixed encoding', '/Game/%2E./Secret'],
  ])('decodes rather than matching spellings, so it catches %s', (_label, value) => {
    // These are why the predicate decodes to a fixed point instead of testing
    // ENCODED_TRAVERSAL_PATTERN: every one of them decodes to `/Game/../Secret`
    // and none of them contains `%2e%2e` or `%252e`, so a spelling list let all
    // four through on the two surfaces that never decode.
    expect(ENCODED_TRAVERSAL_PATTERN.test(value), 'spelling list misses it').toBe(false);
    expect(isTraversalPath(value)).toBe(true);
  });

  it('refuses malformed encoding and admits legitimate escapes', () => {
    // decodeURIComponent throws on '%Co'. A value the next decoder downstream
    // would resolve differently is refused here rather than guessed at.
    expect(isTraversalPath('/Game/100%Cotton')).toBe(true);
    // An encoded space is ordinary; decoding settles on '/Game/A B', no '..'.
    expect(isTraversalPath('/Game/A%20B')).toBe(false);
  });

  it('admits every declared content root and nothing beside them', () => {
    for (const root of UE_CONTENT_ROOTS) {
      expect(isUnderContentRoot(root), root).toBe(true);
      expect(isUnderContentRoot(`${root}/Sub/Thing`), root).toBe(true);
      // A prefix match is not a root match: /GameOther is a different mount.
      expect(isUnderContentRoot(`${root}Other/Thing`), root).toBe(false);
    }
    expect(isUnderContentRoot('/Content/Props')).toBe(false);
  });
});

describe('isContentMountShapedPath', () => {
  // Plugin and game-feature mounts differ per project, so a static allowlist cannot know them. These must
  // pass the SHAPE check and reach the engine's mount table, which is the real authority.
  it('accepts paths shaped like a plugin or game-feature content mount', () => {
    for (const value of ['/myplugin/blueprints/bp_foo', '/shootercore/maps/l_arena', '/pedestrian_system/ai', '/paper2d']) {
      expect(isContentMountShapedPath(value)).toBe(true);
    }
  });

  it('rejects host roots that HOST_PATH_PATTERN already names', () => {
    for (const value of ['/home/user/x', '/usr/lib/x', '/etc/passwd', '/tmp/x']) {
      expect(isContentMountShapedPath(value)).toBe(false);
    }
  });

  // The regression this guards: HOST_PATH_PATTERN names only a handful of POSIX roots. Narrowing the mount
  // check to it alone would let every one of these through as a "content mount" -- Windows system dirs,
  // per-user data, the WSL /mnt/c/... shape, and the project's own on-disk layout.
  it('still rejects the host and project-layout roots HOST_PATH_PATTERN does not name', () => {
    for (const value of [
      '/windows/system32/x', '/programdata/x', '/appdata/roaming/x', '/documents/x',
      '/mnt/c/windows/x', '/volumes/x', '/library/x',
      '/saved/x', '/config/defaultengine', '/binaries/win64/x', '/source/x', '/plugins/x',
    ]) {
      expect(isContentMountShapedPath(value)).toBe(false);
    }
  });

  it('rejects values that are not a single-segment mount shape', () => {
    for (const value of ['/my.plugin/x', '/my plugin/x', 'relative/path', '/']) {
      expect(isContentMountShapedPath(value)).toBe(false);
    }
  });
});
