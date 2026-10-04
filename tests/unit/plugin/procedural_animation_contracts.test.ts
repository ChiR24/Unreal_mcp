/**
 * Source contracts for procedural animation authoring (create_animation_asset kind=procedural).
 *
 * A clip authored with bone keys drew the reference pose: every track carried one key fewer than the
 * sequence (N frames hold N + 1 keys), frames between two authored keys snapped back to rest, and a
 * 24 fps clip was refused by the engine and silently stayed at 30.
 */

import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';
import { describe, expect, it } from 'vitest';

const source = readFileSync(
  resolve(
    process.cwd(),
    'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/Domains/Animation/Assets/McpAutomationBridge_AnimationHandlersProceduralTracks.cpp',
  ),
  'utf8',
);

/** Comment bodies explain the rules, so rule checks ignore them. */
const code = source.replace(/\/\*[\s\S]*?\*\//gu, '').replace(/^[ \t]*\/\/.*$/gmu, '');

describe('procedural bone tracks', () => {
  it('writes one key per frame boundary, frames 0..numFrames', () => {
    expect(code).toContain('const int32 NumKeys = NumFrames + 1;');
    expect(code).toContain('RotationKeys.Init(RefLocal.GetRotation(), NumKeys);');
    expect(code).toMatch(/FrameIndex < 0 \|\| FrameIndex >= NumKeys/u);
    expect(code).not.toMatch(/Init\([^)]*,\s*NumFrames\)/u);
  });

  it('blends the frames between authored keys instead of leaving them at rest', () => {
    expect(code).toMatch(/FillBetweenKeys\(PositionKeys, PositionKeyed,/u);
    expect(code).toMatch(/FillBetweenKeys\(RotationKeys, RotationKeyed,[^;]*FQuat::Slerp/u);
    expect(code).toMatch(/FillBetweenKeys\(ScaleKeys, ScaleKeyed,/u);
    const fill = code.indexOf('FillBetweenKeys(RotationKeys');
    const write = code.indexOf('Controller.SetBoneTrackKeys(BoneFName');
    expect(fill, 'the gaps must be filled before the keys are written').toBeGreaterThan(-1);
    expect(write).toBeGreaterThan(fill);
  });

  it('reaches a frame rate the engine would refuse through a common multiple', () => {
    expect(code).toContain('FMath::LeastCommonMultiplier(Current.Numerator, FrameRate)');
    expect(code.match(/SetFrameRateVia\(Sequence, FrameRate\);/gu)?.length).toBe(2);
  });

  it('shows a component animation edit in the editor without a level reload', () => {
    const helper = readFileSync(
      resolve(
        process.cwd(),
        'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/Foundation/BridgeHelpers/Properties/McpAutomationBridgeHelpersComponentLookup.h',
      ),
      'utf8',
    );
    const refresh = helper.slice(helper.indexOf('static inline void McpRefreshComponentAfterEdit'));
    expect(refresh).toMatch(/Cast<USkeletalMeshComponent>\(Component\)[\s\S]*WorldType == EWorldType::Editor[\s\S]*InitAnim\(true\)/u);
  });

  it('re-keys an existing clip from empty instead of answering success with nothing written', () => {
    const create = readFileSync(
      resolve(
        process.cwd(),
        'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/Domains/Animation/Assets/McpAutomationBridge_AnimationHandlersCreateProceduralAnim.cpp',
      ),
      'utf8',
    );
    expect(create).not.toContain('reusing existing asset');
    expect(create).toMatch(/if \(bExisting\) \{[\s\S]*RemoveAllBoneTracks\(\)[\s\S]*ApplyProceduralBoneTracks/u);
  });

  it('rebuilds a clip as additive when its additive settings change, and refuses unknown types', () => {
    const settings = readFileSync(
      resolve(
        process.cwd(),
        'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/Domains/AnimationAuthoring/McpAutomationBridge_AnimationAuthoringHandlersSequenceSettings.cpp',
      ),
      'utf8',
    );
    expect(settings).toMatch(/GET_MEMBER_NAME_CHECKED\(UAnimSequence, AdditiveAnimType\)[\s\S]*PostEditChangeProperty\(AdditiveChanged\)/u);
    expect(settings).toContain('additiveAnimType must be NoAdditive, LocalSpaceAdditive or MeshSpaceAdditive');
  });

  it('makes the length one whole second first, so the conversion is exact and does not warn', () => {
    const second = code.indexOf('SetNumberOfFrames(FFrameNumber(Current.Numerator))');
    const viaMultiple = code.indexOf('SetFrameRate(FFrameRate(FMath::LeastCommonMultiplier');
    expect(second).toBeGreaterThan(-1);
    expect(viaMultiple).toBeGreaterThan(second);
  });
});
