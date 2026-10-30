// Every animation and skeleton edit is one undo step: notifies, sync markers, root-motion and additive settings, montage
// sections, slots, links and blends, Aim Offset and blend space samples, blend space axes and smoothing, sockets and
// virtual bones. They changed the asset with no transaction (a Modify with none open records nothing), so Ctrl+Z undid
// the edit before them and the reply said nothing about undo. Behaviour belongs to the integration cases in
// tests/mcp-tools/gameplay/animation-physics*.test.mjs, which assert undo.undoable on each.

import { readFileSync } from 'node:fs';
import { join } from 'node:path';

import { describe, expect, it } from 'vitest';

const PRIVATE = join('plugins', 'EngineWire', 'Source', 'EngineWire', 'Private');
const AUTHORING = join(PRIVATE, 'Domains', 'AnimationAuthoring');
const BONES = join(PRIVATE, 'Domains', 'Skeleton', 'Bones');

/** Block and line comments removed, so no assertion can be satisfied by prose. */
const read = (file: string): string =>
  readFileSync(file, 'utf8').replace(/\/\*[\s\S]*?\*\//gu, ' ').replace(/\/\/[^\n]*/gu, ' ');

const EDITS: Record<string, readonly string[]> = {
  [join(AUTHORING, 'McpAnimSequenceEvents.cpp')]: ['Add Notify'],
  [join(AUTHORING, 'McpAnimSequenceNotifyStates.cpp')]: ['Add Notify State'],
  [join(AUTHORING, 'McpAnimSequenceSettings.cpp')]: ['Add Sync Marker', 'Set Root Motion Settings', 'Set Additive Settings'],
  [join(AUTHORING, 'McpAnimMontageAssets.cpp')]: ['Add Montage Section', 'Add Montage Slot', 'Set Section Timing'],
  [join(AUTHORING, 'McpAnimMontageNotifyBlend.cpp')]: ['Add Montage Notify', 'Set Blend In', 'Set Blend Out', 'Link Sections'],
  [join(AUTHORING, 'McpAnimAimOffsets.cpp')]: ['Add Aim Offset Sample'],
  [join(AUTHORING, 'McpAnimBlendSpaceSamples.cpp')]: ['Add Blend Sample', 'Set Axis Settings', 'Set Interpolation Settings'],
  [join(BONES, 'McpSkeletonSockets.cpp')]: ['Add Socket', 'Configure Socket'],
  [join(BONES, 'McpSkeletonSocketRemoval.cpp')]: ['Delete Socket'],
  [join(BONES, 'McpSkeletonVirtualBones.cpp')]: ['Add Virtual Bone', 'Rename Virtual Bone', 'Delete Virtual Bone'],
};

describe('animation and skeleton edits are one undo step', () => {
  it('opens its transaction with the asset Modify\'d, and writes the undo block as it closes', () => {
    expect(readFileSync(join(PRIVATE, 'Foundation', 'McpScopedEditorTransaction.h'), 'utf8')).toMatch(/~FMcpAssetEdit\(\) \{ Transaction\.DescribeInto\(Response\); \}/u);
    expect(readFileSync(join(PRIVATE, 'Foundation', 'McpScopedEditorTransaction.cpp'), 'utf8'))
      .toMatch(/FMcpAssetEdit::FMcpAssetEdit[\s\S]*?for \(UObject\* Object : Objects\)\s*\{\s*Object->Modify\(\);/u);
  });

  for (const [file, titles] of Object.entries(EDITS)) {
    it(`${file.split(/[\\/]/u).pop()}: ${titles.join(', ')}`, () => {
      const source = read(file);
      for (const title of titles) {
        expect(source).toContain(`const FMcpAssetEdit Edit(TEXT("${title}")`);
      }
      // A Modify with no transaction open records nothing: every one belongs to an edit.
      expect(source).not.toMatch(/(Skeleton|BlendSpace|Sequence|Montage|AnimAsset)->Modify\(\);/u);
    });
  }

  it('makes a notify object before the edit opens, so a refusal changes nothing', () => {
    for (const file of ['McpAnimSequenceEvents.cpp', 'McpAnimSequenceNotifyStates.cpp', 'McpAnimMontageNotifyBlend.cpp']) {
      const source = read(join(AUTHORING, file));
      expect(source.indexOf('INSTANTIATION_FAILED')).toBeLessThan(source.indexOf('const FMcpAssetEdit Edit(TEXT("Add'));
      expect(source).not.toContain('Notifies.Pop()');
    }
  });

  it('refuses a sample AddSample would refuse, before the edit opens', () => {
    for (const [file, title] of [['McpAnimAimOffsets.cpp', 'Add Aim Offset Sample'], ['McpAnimBlendSpaceSamples.cpp', 'Add Blend Sample']]) {
      const source = read(join(AUTHORING, file));
      expect(source.indexOf('SAMPLE_REJECTED')).toBeLessThan(source.indexOf(`const FMcpAssetEdit Edit(TEXT("${title}")`));
      expect(source).toMatch(/ValidateAnimationSequence\(Animation\) \|\| \w+->IsTooCloseToExistingSamplePoint\(SampleValue, INDEX_NONE\)/u);
    }
  });

  it('records a socket\'s own fields, and refuses a virtual bone pair already there before the edit opens', () => {
    expect(read(join(BONES, 'McpSkeletonSockets.cpp'))).toMatch(/Socket->SetFlags\(RF_Transactional\);\s*const FMcpAssetEdit Edit\(TEXT\("Configure Socket"\), \{Skeleton, Socket\}/u);
    const bones = read(join(BONES, 'McpSkeletonVirtualBones.cpp'));
    expect(bones.indexOf('VIRTUAL_BONE_FAILED')).toBeLessThan(bones.indexOf('const FMcpAssetEdit Edit(TEXT("Add Virtual Bone")'));
  });
});
