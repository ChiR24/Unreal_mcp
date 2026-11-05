// system_control fit_frame_budget (plugin Domains/Performance/Budget/): a knob measured is switched in render state only,
// the pick is kept as one undo step inside the latent reply, and knobs and view go back before any answer.
// Behaviour belongs to the integration cases in tests/mcp-tools/core/system-control.test.mjs.

import { readFileSync } from 'node:fs';
import { join } from 'node:path';

import { describe, expect, it } from 'vitest';

const BUDGET = join('plugins', 'EngineWire', 'Source', 'EngineWire', 'Private', 'Domains', 'Performance', 'Budget');
/** Block and line comments removed, so no assertion can be satisfied by prose. */
const code = (file: string): string =>
  readFileSync(join(BUDGET, file), 'utf8').replace(/\r\n/gu, '\n').replace(/\/\*[\s\S]*?\*\//gu, ' ').replace(/\/\/[^\n]*/gu, ' ');
const between = (text: string, from: string, to: string): string => text.slice(text.indexOf(from), text.indexOf(to, text.indexOf(from)));

describe('fit_frame_budget', () => {
  const knobs = code('McpFrameBudgetKnobs.cpp');
  const run = code('McpFrameBudgetRun.cpp');

  // A measure that wrote the saved Desired Max Draw Distance left meshes culled after a run that kept nothing.
  it('switches a knob for a measure in render state only, and writes the saved distance only when keeping', () => {
    const setKnob = between(knobs, 'void SetKnob(', 'int32 KeepKnobs(');
    expect(setKnob).toContain('Mesh->SetCachedMaxDrawDistance(');
    expect(setKnob).not.toContain('SetCullDistance(');
    expect(setKnob).not.toContain('Modify(');
    const keep = knobs.slice(knobs.indexOf('int32 KeepKnobs('));
    expect(keep).toContain('FMcpScopedEditorTransaction Transaction(');
    expect(keep).toContain('if (!Transaction.IsRecordingUndo())');
    expect(keep.indexOf('if (!Transaction.IsRecordingUndo())')).toBeLessThan(keep.indexOf('Mesh->SetCullDistance(Knob.Distance);'));
  });

  // SetCastShadows does nothing on a registered static or stationary light, whose shadows are baked anyway.
  it('takes only movable lights', () => {
    expect(knobs).toMatch(/Light->Mobility != EComponentMobility::Movable\)\s*\{\s*return false;/u);
  });

  it('puts knobs and view back before it answers, and keeps the pick inside the latent reply', () => {
    for (const name of ['void Fail(', 'void Finish(']) {
      const body = between(run, name, '\n}\n');
      expect(body.indexOf('RestoreView(Run);')).toBeGreaterThan(0);
      expect(body.indexOf('RestoreView(Run);')).toBeLessThan(body.indexOf('Answer(Run,'));
    }
    const finish = between(run, 'void Finish(', 'void Begin(');
    expect(finish.indexOf('Answer(Run,')).toBeLessThan(finish.indexOf('KeepKnobs(Run, Code, Detail)'));
    const restore = between(code('McpFrameBudget.cpp'), 'void RestoreView(', 'bool Handle(');
    for (const step of ['SetKnob(Knob, false);', 'Run.Client->SetGameView(Run.bWasGameView);', 'EndEditorRunForMcp(Run.Hold);']) {
      expect(restore).toContain(step);
    }
  });

  // A capture of another size read past the baseline's pixels in the brightness sum.
  it('refuses a picture whose size changed before comparing it', () => {
    const capture = between(run, 'bool Capture(', 'bool NextKnob(');
    expect(capture).toContain('OutFrame.Size != Run.Baseline.Frame.Size');
    expect(capture.indexOf('OutFrame.Size != Run.Baseline.Frame.Size')).toBeLessThan(capture.indexOf('return true;\n}'));
  });

  // Judged against the opening picture, every knob after the first read as changing 93-98% of the frame: the exposure
  // had adapted and the new scene settled since. Each knob is judged against the picture taken just before it.
  it('judges each knob against the picture taken just before it went on', () => {
    expect(between(run, 'TSharedPtr<FJsonObject> Changed(', 'bool Capture(')).toContain('CompareFrames(TEXT("baseline"), Run.Pair, Frame)');
    // The knob-off measure that ends one knob holds the picture the next knob is judged against.
    const off = between(run, 'case EStep::KnobOff:', 'case EStep::Chosen:');
    expect(off.indexOf('Run.Pair.Frame = MoveTemp(Frame);')).toBeGreaterThan(0);
    expect(off.indexOf('Run.Pair.Frame = MoveTemp(Frame);')).toBeLessThan(off.indexOf('return NextKnob(Run);'));
  });

  // Gains read against one baseline swung 0.4 ms between two runs: each knob is measured off again straight after.
  // Noise taken as the spread of all knob-off measures counted drift (0.79 ms) and hid 2 ms gains: it is the change
  // between consecutive ones.
  it('takes a knob\'s gain as off less on, measured a moment apart, against the noise of consecutive measures', () => {
    const reply = code('McpFrameBudgetReply.cpp');
    expect(reply).toContain('Knob.GainMs = Knob.OffMs - Knob.Ms;');
    expect(reply).toContain('Steps.Add(FMath::Abs(Run.Bases[Index] - Run.Bases[Index - 1]));');
    const knob = between(run, 'case EStep::Knob:', 'case EStep::KnobOff:');
    expect(knob.indexOf('SetKnob(Knob, false);')).toBeLessThan(knob.indexOf('Begin(Run, EStep::KnobOff);'));
  });
});
