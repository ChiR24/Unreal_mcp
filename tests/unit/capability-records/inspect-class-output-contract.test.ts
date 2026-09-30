// inspect_class (kind=class) sends `properties` as an array of descriptors, inspect_cdo as an
// object of values. The class record declared neither that nor its other fields, so they were
// projected away; once both were folded into inspect.inspect_class, the class variant's array
// met the cdo variant's object type and every class inspect answered OUTPUT_SCHEMA_VIOLATION.
// Ground truth is the handler source, re-read on every run.

import { readFileSync } from 'node:fs';
import { join } from 'node:path';
import { describe, expect, it } from 'vitest';

import { ALL_CAPABILITY_RECORDS } from '../../../src/tools/catalog/capabilities/records/aggregate.js';
import { validateAgainstCapabilitySchema } from '../../../src/server/gateway/gateway-schema-validate.js';

const HANDLER = join('plugins', 'McpAutomationBridge', 'Source', 'McpAutomationBridge', 'Private',
  'Domains', 'Environment', 'Inspection', 'McpAutomationBridge_EnvironmentHandlersInspectClass.cpp');
// Deliberately left undeclared: a hex dump of the flags (flags carries them as booleans) and the
// package path (module or generatedBy name it).
const UNDECLARED = new Set(['classFlags', 'package']);

const record = ALL_CAPABILITY_RECORDS.find((candidate) => String(candidate.id) === 'inspect.inspect_class');
const output = record?.schemas.output;

describe('inspect_class output contract', () => {
  it('declares every top-level field the class handler sends', () => {
    const sent = [...readFileSync(HANDLER, 'utf8').matchAll(/Resp->Set\w+Field\(TEXT\("(\w+)"\)/gu)].map((match) => match[1]);
    expect(sent.length).toBeGreaterThan(10);
    const declared = Object.keys(output?.properties ?? {});
    expect(sent.filter((name) => name !== undefined && !UNDECLARED.has(name) && !declared.includes(name))).toEqual([]);
  });

  it('accepts a class reply and a cdo reply', () => {
    const classReply = {
      success: true, className: 'LegacyCameraShake', parentClass: 'CameraShakeBase', ancestors: ['CameraShakeBase', 'Object'],
      properties: [{ name: 'OscillationDuration', type: 'Float', declaredIn: 'LegacyCameraShake', editable: true }],
      propertyCount: 1, propertiesTruncated: false, defaultProperties: { OscillationDuration: '0.000000' }
    };
    expect(validateAgainstCapabilitySchema(classReply, output)).toBeUndefined();
    expect(validateAgainstCapabilitySchema({ success: true, properties: { bBig: false } }, output)).toBeUndefined();
  });
});
