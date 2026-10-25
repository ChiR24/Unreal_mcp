// Source contracts for remove_bone and set_bone_parent, which need an editor to run. FReferenceSkeletonModifier::Remove
// and ::SetParent exist only from UE 5.3, and both actions used to answer NOT_SUPPORTED on 5.0-5.2. Behaviour belongs
// to the integration cases.

import { readFileSync } from 'node:fs';
import { join } from 'node:path';

import { describe, expect, it } from 'vitest';

const BONES = join('plugins', 'EngineWire', 'Source', 'EngineWire', 'Private', 'Domains', 'Skeleton', 'Bones');
const read = (file: string): string => readFileSync(join(BONES, file), 'utf8');

describe('remove_bone and set_bone_parent on every engine version', () => {
  const editing = read('McpSkeletonStructureEditing.cpp');
  const edits = read('McpSkeletonBoneEdits.cpp');

  it('never answer NOT_SUPPORTED, and the handlers carry no engine-version branch', () => {
    expect(editing).not.toContain('NOT_SUPPORTED');
    expect(edits).not.toContain('NOT_SUPPORTED');
    expect(editing).not.toContain('ENGINE_MINOR_VERSION');
  });

  it('rebuild the bones on 5.0-5.2 without telling loaded animations a kept bone is gone', () => {
    expect(edits).toContain('Ref.RemoveBonesByName(nullptr, Dropped);');
    expect(edits).not.toContain('RemoveBonesFromSkeleton(');
  });

  it('put the BoneTree retargeting modes back in step after both paths, and after create_skeleton and add_bone', () => {
    expect(edits.match(/SyncBoneTree\(Skeleton, Modes\);/gu)).toHaveLength(2);
    const creation = read('McpSkeletonStructureCreation.cpp');
    expect(creation).toContain('McpSkeletonBoneEdits::FitBoneTree(NewSkeleton);');
    expect(creation.indexOf('McpSkeletonBoneEdits::FitBoneTree(Skeleton);')).toBeLessThan(creation.indexOf('Transaction.DescribeInto(Result);'));
  });

  it('look bones up among the raw bones, so a virtual bone is not found instead of answered as edited', () => {
    expect(editing).not.toContain('RefSkeleton.FindBoneIndex(');
    expect(editing.match(/RefSkeleton\.FindRawBoneIndex\(/gu)).toHaveLength(3);
  });
});
