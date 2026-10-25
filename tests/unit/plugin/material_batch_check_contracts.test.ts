// build_material_graph checks every edit and "$id" before a step runs: a misspelled id at step 10 used to leave steps
// 0-9 applied, since the material batch does not roll back. dryRun answers with the check alone. Wiring contracts only:
// the behaviour is covered by the manage-asset integration cases.

import { readFileSync } from 'node:fs';
import { join } from 'node:path';

import { describe, expect, it } from 'vitest';

const MATERIAL = join('plugins', 'EngineWire', 'Source', 'EngineWire', 'Private', 'Domains', 'MaterialAuthoring');

/** Line comments removed, CRLF normalized, so no assertion can be satisfied by prose. */
const code = (file: string): string =>
  readFileSync(join(MATERIAL, file), 'utf8').replace(/\r\n/gu, '\n').replace(/\/\/[^\n]*/gu, ' ');

describe('build_material_graph checks the batch before a step runs', () => {
  const batch = code('McpMatGraphBatch.cpp');
  const handler = batch.slice(batch.indexOf('bool HandleBuildMaterialGraph('));

  it('runs the check after the material loads and before the first step, answering dryRun there', () => {
    const check = handler.indexOf('CheckMaterialBatch(Payload, *Steps)');
    expect(check).toBeGreaterThan(handler.indexOf('LOAD_MATERIAL_OR_FUNCTION_OR_RETURN();'));
    expect(check).toBeLessThan(handler.indexOf('RunStep(StepId, StepPayload);'));
    expect(handler).toMatch(/if \(Problems\.Num\(\) > 0 \|\| GetJsonBoolField\(Payload, TEXT\("dryRun"\), false\)\) \{\s*return AnswerMaterialBatchCheck\(/u);
  });

  it('resolves "$id" with the run\'s own payload and alias helpers, and runs nothing itself', () => {
    const source = code('McpMatGraphBatchCheck.cpp');
    const checker = source.slice(source.indexOf('TArray<TSharedPtr<FJsonValue>> CheckMaterialBatch('), source.indexOf('bool AnswerMaterialBatchCheck('));
    expect(checker).toContain('ResolveAliases(Ids, BuildStepPayload(Payload, *StepPtr, Edit), Error)');
    expect(checker).toContain('DescribeUnbatchableMaterialEdit(Edit)');
    expect(code('McpMatGraphBatchCheck.cpp')).not.toMatch(/RunStep\(|Capture\.Begin|McpFinishMaterialEdit/u);
  });
});
