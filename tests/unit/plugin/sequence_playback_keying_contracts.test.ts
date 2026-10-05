/**
 * Source contracts for previewing and keying a level sequence over MCP.
 *
 * A seek on a playing Sequencer was lost while the reply named the frame asked for; a held frame showed the previous
 * pose in a background editor; re-keying a frame added a second key beside the first; a skeletal animation track on a
 * character built from several meshes played on a follower and showed nothing.
 */

import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';
import { describe, expect, it } from 'vitest';

const read = (relative: string): string =>
  readFileSync(
    resolve(process.cwd(), 'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/Domains/Sequence', relative),
    'utf8',
  );

const playback = read('McpAutomationBridge_SequenceHandlersPlayback.cpp');

describe('sequence playback', () => {
  it('pauses before seeking, poses the animated meshes, and reports the frame it reached', () => {
    const seek = playback.slice(playback.indexOf('void McpSeekPaused'));
    expect(seek).toMatch(/Pause\(\);[\s\S]*SetLocalTime\([\s\S]*TickAnimation\(0\.f, false\)[\s\S]*RefreshBoneTransforms\(\)/u);
    expect(seek, 'a mesh copying another pose needs the second pass').toMatch(/for \(int32 Pass = 0; Pass < 2; \+\+Pass\)[\s\S]*TickAnimation/u);
    expect(playback).toContain('McpPlayheadFrame(Sequencer, MovieScene)');
  });

  it('refuses a play or pause time past the end of the sequence', () => {
    expect(playback.match(/McpSequenceFrameMath::CheckPlaybackTime\(/gu)?.length).toBe(2);
    const math = read('Validation/McpAutomationBridge_SequenceFrameMath.cpp');
    expect(math).toMatch(/bool CheckPlaybackTime[\s\S]*if \(Seconds <= EndSeconds\) \{\s*return true;/u);
  });

  it('holds a frame on pause, opening a closed sequence first', () => {
    const pause = playback.slice(playback.indexOf('HandleSequencePause'));
    expect(pause).toMatch(/TryGetNumberField\(TEXT\("startTime"\), HoldAt\)/u);
    expect(pause).toMatch(/bHold && [\s\S]*OpenLevelSequence\(LevelSeq\)/u);
    expect(pause).toContain('McpSeekPaused(Sequencer, LevelSeq->GetMovieScene(), HoldAt)');
  });
});

describe('sequence keying and binding', () => {
  it('replaces a transform key on a frame that already has one, and smooths the curve through it', () => {
    const keys = read('McpAutomationBridge_SequenceHandlersTransformKeyframes.cpp');
    expect(keys).toMatch(/Key\.InterpMode = Interpolation;[\s\S]*UpdateOrAddKey\(TickFrame, Key\);[\s\S]*Channels\[Index\]->AutoSetTangents\(\);/u);
    expect(keys).not.toMatch(/GetData\(\)\.AddKey\(/u);
    const helpers = read('McpAutomationBridge_SequenceHandlersKeyframeHelpers.cpp');
    expect(helpers).toMatch(/UpdateOrAddKey\(TickFrame, Key\);[\s\S]*Channel->AutoSetTangents\(\);/u);
  });

  it('starts a new transform section from what the bound actor holds, so unkeyed channels keep their values', () => {
    const keys = read('McpAutomationBridge_SequenceHandlersTransformKeyframes.cpp');
    expect(keys).toMatch(/if \(bSectionAdded && Bound && Channels\.Num\(\) >= 9\) \{[\s\S]*Channels\[Index\]->SetDefault\(Current\[Index\]\);/u);
    expect(read('McpAutomationBridge_SequenceHandlersKeyframes.cpp')).toContain('McpSequenceCinematics::LocateBindingObjects(LevelSeq, BindingGuid');
  });

  it('aims a lookAt key from its location and winds the yaw the short way from the key before', () => {
    const keys = read('McpAutomationBridge_SequenceHandlersTransformKeyframes.cpp');
    expect(keys).toContain('const FRotator Aim = (To - From).Rotation();');
    expect(keys).toContain('Yaw = Yaws[Index].Value + FMath::UnwindDegrees(Yaw - Yaws[Index].Value);');
    expect(keys).toContain('AimAtLookTarget(*ValueObj, *Channels[5], TickFrame)');
  });

  it('keys Visibility on a Visibility track, in a section that spans the playback range', () => {
    const helpers = read('McpAutomationBridge_SequenceHandlersKeyframeHelpers.cpp');
    expect(helpers).toMatch(/bVisibility \? static_cast<UMovieScenePropertyTrack \*>\(MovieScene->FindTrack<UMovieSceneVisibilityTrack>\(BindingGuid\)\)/u);
    const section = helpers.slice(helpers.indexOf('UMovieSceneSection *FindOrAddKeySection'));
    expect(section).toMatch(/bSectionAdded \|\| Section->GetRange\(\)\.IsEmpty\(\)\) \{\s*Section->SetRange\(MovieScene->GetPlaybackRange\(\)\)/u);
    expect(helpers.match(/FindOrAddKeySection\(MovieScene, Track, TickFrame\)/gu)?.length).toBe(2);
    expect(read('McpAutomationBridge_SequenceHandlersTransformKeyframes.cpp')).toContain('FindOrAddKeySection(MovieScene, Track, TickFrame, &bSectionAdded)');
  });

  it('writes a keys array in one call, checking every frame before writing any', () => {
    const handler = read('McpAutomationBridge_SequenceHandlersKeyframes.cpp');
    expect(handler).toMatch(/TryGetArrayField\(TEXT\("keys"\), Entries\)/u);
    expect(handler).toMatch(/if \(!ReadKeys\(LocalPayload, Keys, KeyError\)\)[\s\S]*McpLoadAsset\(SeqPath\)/u);
    expect(handler).toMatch(/for \(const TSharedPtr<FJsonObject> &Key : Keys\) \{\s*WrapTransformPart\(Key, Part\);/u);
    expect(handler).toMatch(/for \(int32 Index = 0; Index < Keys\.Num\(\); \+\+Index\)[\s\S]*AddTransformKeyframe\(MovieScene, BindingGuid, TickFrame, Keys\[Index\], BoundRoot\)/u);
  });

  it('reads bool keys back, so a Visibility track can be checked', () => {
    const keys = read('Metadata/McpAutomationBridge_SequenceTrackKeys.cpp');
    expect(keys).toContain('DescribeChannels<FMovieSceneBoolChannel>(MovieScene, Proxy, TEXT("bool")');
    expect(keys).toContain('double KeyNumber(bool bValue) { return bValue ? 1.0 : 0.0; }');
  });

  it('eases a skeletal animation section in and out, in ticks, so overlapping clips crossfade', () => {
    const tracks = read('Cinematics/McpAutomationBridge_SequenceCinematicsBindingTracks.cpp');
    expect(tracks).toMatch(/TryGetNumberField\(TEXT\("easeInFrames"\), EaseFrames\)[\s\S]*bManualEaseIn = true;[\s\S]*ManualEaseInDuration = Ticks\(EaseFrames\);/u);
    expect(tracks).toMatch(/TryGetNumberField\(TEXT\("easeOutFrames"\), EaseFrames\)[\s\S]*bManualEaseOut = true;[\s\S]*ManualEaseOutDuration = Ticks\(EaseFrames\);/u);
  });

  it('moves, trims and eases a section already on a track', () => {
    const state = read('McpAutomationBridge_SequenceHandlersTrackState.cpp');
    const setSection = state.slice(state.indexOf('HandleSequenceSetSection('), state.indexOf('HandleSequenceRemoveTrack('));
    expect(setSection).toMatch(/Range\.SetLowerBound\(TRangeBound<FFrameNumber>::Inclusive\(Ticks\(Start\)\)\)/u);
    expect(setSection).toMatch(/Range\.SetUpperBound\(TRangeBound<FFrameNumber>::Exclusive\(Ticks\(End\)\)\)/u);
    expect(setSection).toMatch(/Section->Easing\.bManualEaseIn = EaseIn > 0\.0;[\s\S]*Section->Easing\.bManualEaseOut = EaseOut > 0\.0;/u);
    expect(setSection).toContain('Sequence->MarkPackageDirty();');
    expect(read('McpAutomationBridge_SequenceHandlers.cpp')).toContain('if (EffectiveAction == TEXT("sequence_set_section"))');
  });

  it('removes keys of every type, on one frame or several', () => {
    const removal = read('Metadata/McpAutomationBridge_SequenceKeyRemoval.cpp');
    for (const family of ['Double', 'Float', 'Bool', 'Byte', 'Integer', 'Particle']) {
      expect(removal).toContain(`RemoveChannelKeys<FMovieScene${family}Channel>(Proxy, !bHasFrame, TargetTicks)`);
    }
    expect(removal).toMatch(/TryGetArrayField\(TEXT\("frames"\), FrameList\)/u);
    expect(removal, 'an empty frames list must not clear the track').toContain('frames must list at least one frame number');
  });

  it('reads back whose track it is, and each section row, easing and clip', () => {
    const keys = read('Metadata/McpAutomationBridge_SequenceTrackKeys.cpp');
    expect(keys).toContain('GetBindingLabel(MovieScene, BindingGuid)');
    expect(keys).toContain('DescribeChannels<FMovieSceneParticleChannel>(MovieScene, Proxy, TEXT("particle")');
    expect(read('McpAutomationBridge_SequenceHandlersEditorSupport.h')).toMatch(/inline FString GetBindingLabel[\s\S]*Parent \+ TEXT\("\/"\) \+ Name/u);
    expect(keys).toContain('MovieScene->FindTrackBinding(*Track, BindingGuid)');
    expect(keys).toContain('Obj->SetNumberField(TEXT("rowIndex"), Section->GetRowIndex());');
    expect(keys).toMatch(/Section->Easing\.GetEaseInDuration\(\) > 0[\s\S]*Section->Easing\.GetEaseOutDuration\(\) > 0/u);
    expect(keys).toContain('Obj->SetStringField(TEXT("animation"), Clip->Params.Animation->GetPathName());');
  });

  it('binds the mesh that plays the clip and refuses a skeleton it cannot play', () => {
    const tracks = read('Cinematics/McpAutomationBridge_SequenceCinematicsBindingTracks.cpp');
    expect(tracks).toMatch(/Cast<USkeletalMeshComponent>\(Actor->GetRootComponent\(\)\)/u);
    expect(tracks).toMatch(/Possessable->SetParent\(ActorGuid/u);
    expect(tracks).toContain('TEXT("SKELETON_MISMATCH")');
  });
});
