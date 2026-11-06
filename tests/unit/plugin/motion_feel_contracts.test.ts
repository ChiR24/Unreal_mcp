// control_actor sample_motion feel (plugin Domains/ControlActor/Motion/McpMotionFeel.cpp): the run samples every frame
// unless intervalSeconds is given, records each sample's frame, and sums the frames up in the result. The summary's
// arithmetic is the plugin's EngineWire.ControlActor.MotionFeel automation test.

import { readFileSync } from 'node:fs';
import { join } from 'node:path';

import { describe, expect, it } from 'vitest';

const ACTOR = join('plugins', 'EngineWire', 'Source', 'EngineWire', 'Private', 'Domains', 'ControlActor');
/** Block and line comments removed, so no assertion can be satisfied by prose. */
const code = (file: string): string =>
  readFileSync(join(ACTOR, file), 'utf8').replace(/\r\n/gu, '\n').replace(/\/\*[\s\S]*?\*\//gu, ' ').replace(/\/\/[^\n]*/gu, ' ');

describe('sample_motion feel', () => {
  const sample = code('McpActorMotionSample.cpp');
  const feel = code(join('Motion', 'McpMotionFeel.cpp'));

  // A key's latency is a frame or two: at the 0.05 s default interval it read as three frames at best.
  it('samples every frame unless intervalSeconds is given, decided before the run keeps its interval', () => {
    expect(feel).toMatch(/if \(Feel\.bOn && !Payload->HasField\(TEXT\("intervalSeconds"\)\)\)\s*\{\s*Interval = 0\.0;/u);
    expect(sample.indexOf('McpParseMotionFeel(Payload, Run->Feel, Interval);')).toBeGreaterThan(0);
    expect(sample.indexOf('McpParseMotionFeel(Payload, Run->Feel, Interval);')).toBeLessThan(sample.indexOf('Run->Interval = FMath::Clamp(Interval, 0.0, 5.0);'));
  });

  it('records each sample\'s frame on the inputs\' clock and sums them up in the result', () => {
    expect(sample).toContain('McpSampleMotionFeel(Run.Feel, Actor, GameTime - Run.StartGame);');
    expect(sample).toContain('McpAddMotionFeel(Run.Feel, Run.Inputs, *Data);');
  });

  // Camera frames that skip one would pair a camera with the wrong actor frame in the jerk.
  it('keeps camera frames only while every frame has one', () => {
    expect(feel).toContain('Feel.Cameras.Num() == Feel.Times.Num() - 1');
    expect(feel).toMatch(/if \(C\.Num\(\) != T\.Num\(\) \|\| C\.Num\(\) < 5\)/u);
  });
});
