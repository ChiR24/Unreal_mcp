// get_components on a Blueprint used to list only names and classes, so laying
// anything out against a Blueprint cost one get_property call per field per
// component. Each entry now carries the template's layout.
import { existsSync, readFileSync } from 'node:fs';
import { resolve } from 'node:path';
import { describe, expect, it } from 'vitest';

const FILE = resolve(process.cwd(),
  'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/Domains/Environment/Inspection/McpAutomationBridge_EnvironmentHandlersInspectBlueprint.cpp');
function code(s: string): string { return s.replace(/\/\*[\s\S]*?\*\//g, '').replace(/\/\/[^\n]*/g, ''); }

describe('Blueprint component listing', () => {
  it('carries each component template transform, visibility, mesh and materials', () => {
    expect(existsSync(FILE)).toBe(true);
    const s = code(readFileSync(FILE, 'utf8'));
    for (const field of ['location', 'rotation', 'scale', 'visible', 'staticMesh', 'materials']) {
      expect(s, field).toContain(`TEXT("${field}")`);
    }
    expect(s).toMatch(/McpMakeComponentEntry\([\s\S]*?Node->ComponentTemplate\)\)\);/);
    expect(s).toMatch(/TEXT\("Native"\), Component\)/);
  });
});
