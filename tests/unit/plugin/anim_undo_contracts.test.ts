// Every sequence and montage edit is one undo step: notifies, sync markers, root-motion and additive settings, montage
// sections, slots, links and blends, Aim Offset samples. They changed the asset with no transaction, so Ctrl+Z did
// nothing and the reply's undo block said NOTHING_CHANGED for a real change. Behaviour belongs to the integration cases
// in tests/mcp-tools/gameplay/animation-physics.test.mjs, which assert undo.undoable on each.

import { readFileSync } from 'node:fs';
import { join } from 'node:path';

import { describe, expect, it } from 'vitest';

const AUTHORING = join('plugins', 'EngineWire', 'Source', 'EngineWire', 'Private', 'Domains', 'AnimationAuthoring');

/** Block and line comments removed, so no assertion can be satisfied by prose. */
const read = (file: string): string =>
  readFileSync(join(AUTHORING, file), 'utf8').replace(/\/\*[\s\S]*?\*\//gu, ' ').replace(/\/\/[^\n]*/gu, ' ');

const EDITS: Record<string, readonly string[]> = {
  'McpAnimSequenceEvents.cpp': ['Add Notify'],
  'McpAnimSequenceNotifyStates.cpp': ['Add Notify State'],
  'McpAnimSequenceSettings.cpp': ['Add Sync Marker', 'Set Root Motion Settings', 'Set Additive Settings'],
  'McpAnimMontageAssets.cpp': ['Add Montage Section', 'Add Montage Slot', 'Set Section Timing'],
  'McpAnimMontageNotifyBlend.cpp': ['Add Montage Notify', 'Set Blend In', 'Set Blend Out', 'Link Sections'],
  'McpAnimAimOffsets.cpp': ['Add Aim Offset Sample'],
};

describe('animation edits are one undo step', () => {
  it('opens its transaction with the asset Modify\'d, and writes the undo block as it closes', () => {
    const support = read('McpAnimSupport.h');
    expect(support).toMatch(/struct FMcpAnimEdit[\s\S]*?Asset->Modify\(\);[\s\S]*?~FMcpAnimEdit\(\) \{ Transaction\.DescribeInto\(Response\); \}/u);
  });

  for (const [file, titles] of Object.entries(EDITS)) {
    it(`${file}: ${titles.join(', ')}`, () => {
      const source = read(file);
      for (const title of titles) {
        expect(source).toContain(`const FMcpAnimEdit Edit(TEXT("${title}")`);
      }
    });
  }

  it('makes a notify object before the edit opens, so a refusal changes nothing', () => {
    for (const file of ['McpAnimSequenceEvents.cpp', 'McpAnimSequenceNotifyStates.cpp', 'McpAnimMontageNotifyBlend.cpp']) {
      const source = read(file);
      expect(source.indexOf('INSTANTIATION_FAILED')).toBeLessThan(source.indexOf('const FMcpAnimEdit Edit(TEXT("Add'));
      expect(source).not.toContain('Notifies.Pop()');
    }
  });

  it('refuses an Aim Offset sample AddSample would refuse, before the edit opens', () => {
    const source = read('McpAnimAimOffsets.cpp');
    expect(source.indexOf('SAMPLE_REJECTED')).toBeLessThan(source.indexOf('const FMcpAnimEdit Edit(TEXT("Add Aim Offset Sample")'));
    expect(source).toMatch(/ValidateAnimationSequence\(Animation\) \|\| Target->IsTooCloseToExistingSamplePoint\(SampleValue, INDEX_NONE\)/u);
  });
});
