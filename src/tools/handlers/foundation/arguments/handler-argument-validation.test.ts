// The TypeScript path gate every automation request passes (validateArgsSecurity).
// A content key may name a mount the connected editor reports; a key the plugin
// opens as a file on disk never may, because a file never lives under a mount.

import { afterEach, describe, expect, it } from 'vitest';

import { clearEditorContentRoots, setEditorContentRoots } from '../../../../utils/paths/path-security.js';
import { validateArgsSecurity } from './handler-argument-validation.js';

const BACKSLASH = String.fromCharCode(92);

afterEach(() => {
  clearEditorContentRoots();
});

describe('validateArgsSecurity content keys and editor-reported roots', () => {
  it('allows a reported mount only while the editor reports it', () => {
    const args = { action: 'list', path: '/ShooterCore/Items' };
    expect(() => validateArgsSecurity(args)).toThrow(/unauthorized absolute path/);

    setEditorContentRoots(['/Game', '/ShooterCore']);
    expect(() => validateArgsSecurity(args)).not.toThrow();

    clearEditorContentRoots();
    expect(() => validateArgsSecurity(args)).toThrow(/unauthorized absolute path/);
  });

  it('treats sourcePath as a package path for asset actions', () => {
    setEditorContentRoots(['/ShooterCore']);
    expect(() => validateArgsSecurity({ action: 'duplicate', sourcePath: '/ShooterCore/A' })).not.toThrow();
  });
});

describe('validateArgsSecurity file keys', () => {
  it.each([
    ['import sourcePath', { action: 'import', sourcePath: '/ShooterCore/x.fbx' }],
    ['the legacy subAction shape', { subAction: 'import', sourcePath: '/ShooterCore/x.fbx' }],
    ['import_struct sourcePath', { action: 'import_struct', sourcePath: '/ShooterCore/s.json' }],
    ['the screenshot folder', { action: 'screenshot', path: '/ShooterCore/shots' }],
    ['the take_screenshot folder', { action: 'take_screenshot', path: '/ShooterCore/shots' }],
    ['the export_heightmap output', { action: 'export_heightmap', path: '/ShooterCore/h.raw' }],
    ['the import_heightmap input', { action: 'import_heightmap', heightmapPath: '/ShooterCore/h.raw' }],
    ['the export_metahuman folder', { action: 'export_metahuman', externalPath: '/ShooterCore/d' }],
    ['an inspect export file', { action: 'export', exportPath: '/ShooterCore/o.t3d' }],
    ['an export_level .t3d file', { action: 'export_level', exportPath: '/ShooterCore/L.t3d' }],
    ['the action the plugin runs (subAction)', { action: 'duplicate', subAction: 'import', sourcePath: '/ShooterCore/x.fbx' }],
  ])('refuses a reported mount for %s', (_label, args) => {
    setEditorContentRoots(['/ShooterCore']);
    expect(() => validateArgsSecurity(args)).toThrow(/unauthorized absolute path/);
  });

  it('allows a reported mount for an export_level package copy', () => {
    setEditorContentRoots(['/ShooterCore']);
    expect(() => validateArgsSecurity({ action: 'export_level', exportPath: '/ShooterCore/Maps/L' })).not.toThrow();
  });

  it('keeps /tmp and /Saved for a file key', () => {
    setEditorContentRoots(['/ShooterCore']);
    expect(() => validateArgsSecurity({ action: 'import', sourcePath: '/tmp/x.fbx' })).not.toThrow();
    expect(() => validateArgsSecurity({ action: 'import', sourcePath: '/Saved/x.fbx' })).not.toThrow();
  });
});

describe('validateArgsSecurity path guards', () => {
  it.each([
    ['a UNC share over a reported mount', '//ShooterCore/x'],
    ['a mixed-separator UNC share', `/${BACKSLASH}ShooterCore${BACKSLASH}x`],
    ['a UNC share over a static root', '//Game/x'],
  ])('refuses %s', (_label, value) => {
    setEditorContentRoots(['/ShooterCore']);
    expect(() => validateArgsSecurity({ action: 'list', path: value })).toThrow(/unauthorized absolute path/);
  });

  it('matches a reported mount only against a canonical value', () => {
    setEditorContentRoots(['/ShooterCore']);
    for (const value of ['/./ShooterCore/x', '/ShooterCore/./x', '/ShooterCore//x']) {
      expect(() => validateArgsSecurity({ action: 'list', path: value }), value).toThrow(/unauthorized absolute path/);
    }
    expect(() => validateArgsSecurity({ action: 'list', path: '/ShooterCore/x' })).not.toThrow();
    expect(() => validateArgsSecurity({ action: 'list', path: '/Game/./x' })).not.toThrow();
  });

  it('keeps the static roots with and without reported mounts', () => {
    for (const value of ['/Game/Foo', '/Engine/Foo']) {
      expect(() => validateArgsSecurity({ action: 'list', path: value }), value).not.toThrow();
    }
    setEditorContentRoots(['/ShooterCore']);
    for (const value of ['/Game/Foo', '/Engine/Foo']) {
      expect(() => validateArgsSecurity({ action: 'list', path: value }), value).not.toThrow();
    }
  });
});
