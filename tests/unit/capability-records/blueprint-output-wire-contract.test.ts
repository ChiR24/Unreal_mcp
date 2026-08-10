/**
 * tests/unit/capability-records/blueprint-output-wire-contract.test.ts
 *
 * TASK 29 GATE - zero tolerance. The canonical output contract of the two
 * property-writing blueprint capabilities must be HONEST about the field the
 * shipping native handler actually puts on the wire.
 *
 * `schema-compilation.test.ts` and `record-example-honesty.test.ts`
 * prove each record is consistent with ITSELF (its example validates against its
 * own schema). That is necessary but not sufficient: a record can REQUIRE a
 * `verifiedValue` output, illustrate it in its example, validate perfectly, and
 * still name a field the plugin either emits only conditionally or never emits
 * at all. A client validating a real success response against that closed,
 * required contract would then reject a correct native response.
 *
 * Ground truth, re-read from the shipping plugin sources on every run:
 *  - blueprint.set_default -> BlueprintHandlersSetDefaultLiteral.cpp emits
 *    `value` CONDITIONALLY (guarded by `CurrentValue.IsValid()`) and
 *    BlueprintHandlersSetDefaultObject.cpp emits NEITHER `value` nor
 *    `verifiedValue`. So `verifiedValue` is a phantom for this action and even
 *    `value` is conditional: the record must declare `value` WITHOUT requiring
 *    it, and must not declare `verifiedValue`.
 *  - blueprint.set_scs_property -> SCSHandlersSetProperty.cpp emits
 *    `verifiedValue` CONDITIONALLY (guarded by `VerifiedValue.IsValid()`), so
 *    the record must declare `verifiedValue` WITHOUT requiring it.
 */
import { readFileSync } from 'node:fs';
import { dirname, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';
import { describe, expect, it } from 'vitest';
import { validateAgainstCapabilitySchema } from '../../../src/server/gateway/gateway-execute-validate.js';
import { MANAGE_BLUEPRINT_RECORDS } from '../../../src/tools/catalog/capabilities/records/manage-blueprint/index.js';

const REPO_ROOT = resolve(dirname(fileURLToPath(import.meta.url)), '../../..');
const PLUGIN = 'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private';
const SET_DEFAULT_LITERAL = `${PLUGIN}/Domains/Blueprint/Graph/McpAutomationBridge_BlueprintHandlersSetDefaultLiteral.cpp`;
const SET_DEFAULT_OBJECT = `${PLUGIN}/Domains/Blueprint/Graph/McpAutomationBridge_BlueprintHandlersSetDefaultObject.cpp`;
const SCS_SET_PROPERTY = `${PLUGIN}/Domains/SCS/McpAutomationBridge_SCSHandlersSetProperty.cpp`;

/** `success`/`message` are the response envelope, not a domain output field. */
const ENVELOPE_FIELDS: ReadonlySet<string> = new Set(['success', 'message']);

const plain = (value: unknown): unknown => JSON.parse(JSON.stringify(value));

const readPluginSource = (relativePath: string): string =>
  readFileSync(resolve(REPO_ROOT, relativePath), 'utf8');

function recordFor(id: string) {
  const found = MANAGE_BLUEPRINT_RECORDS.find((r) => String(r.id) === id);
  if (found === undefined) throw new Error(`No manage_blueprint record with id ${id}`);
  return found;
}

/** Declared output property names minus the envelope, i.e. the domain payload. */
function declaredDomainOutputs(id: string): readonly string[] {
  return Object.keys(recordFor(id).schemas.output.properties).filter((name) => !ENVELOPE_FIELDS.has(name));
}

describe('Task 29 blueprint output wire contract - the ground truth is real', () => {
  it('set_default: the literal handler emits `value` conditionally and neither handler emits `verifiedValue`', () => {
    const literal = readPluginSource(SET_DEFAULT_LITERAL);
    const object = readPluginSource(SET_DEFAULT_OBJECT);
    // `value` is written, and gated on the JSON export succeeding.
    expect(literal).toContain('SetField(TEXT("value")');
    expect(literal).toContain('if (CurrentValue.IsValid())');
    // `verifiedValue` is never emitted for set_default on either path.
    expect(literal).not.toContain('verifiedValue');
    expect(object).not.toContain('verifiedValue');
    expect(object).not.toContain('SetField(TEXT("value")');
  });

  it('set_scs_property: the SCS handler emits `verifiedValue` only when the re-read exports to JSON', () => {
    const scs = readPluginSource(SCS_SET_PROPERTY);
    expect(scs).toContain('SetField(TEXT("verifiedValue")');
    expect(scs).toContain('if (VerifiedValue.IsValid())');
  });
});

describe('Task 29 blueprint output wire contract - the record matches the wire', () => {
  it('set_default declares the conditional `value` it emits and never the phantom `verifiedValue`', () => {
    const record = recordFor('blueprint.set_default');
    const domain = declaredDomainOutputs('blueprint.set_default');
    expect(domain).toContain('value');
    expect(domain).not.toContain('verifiedValue');
    expect(Object.keys(record.schemas.output.properties)).not.toContain('verifiedValue');
  });

  it('set_default never requires the conditional `value` (nor the phantom `verifiedValue`)', () => {
    const required = recordFor('blueprint.set_default').schemas.output.required;
    expect(required).not.toContain('value');
    expect(required).not.toContain('verifiedValue');
  });

  it('set_scs_property declares `verifiedValue` but never requires the conditional field', () => {
    const record = recordFor('blueprint.set_scs_property');
    expect(Object.keys(record.schemas.output.properties)).toContain('verifiedValue');
    expect(record.schemas.output.required).not.toContain('verifiedValue');
  });

  it('both repaired records still carry an example output that satisfies their own schema', () => {
    for (const id of ['blueprint.set_default', 'blueprint.set_scs_property'] as const) {
      const record = recordFor(id);
      const violation = validateAgainstCapabilitySchema(
        plain(record.examples[0]?.output ?? {}),
        plain(record.schemas.output),
      );
      expect(violation, `${id} example output violates its own schema`).toBeUndefined();
    }
  });
});
