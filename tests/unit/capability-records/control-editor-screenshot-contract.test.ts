// A screenshot of an editor window other than the main frame is picked with `window`, and the
// reply lists the windows by index. The record typed it string only, so the natural call
// {"window": 1} (the index straight from windows[]) answered INVALID_PARAMETER_TYPE before the
// plugin saw it; the handler already read a numeric window. Both records now take either.

import { describe, expect, it } from 'vitest';

import { ALL_CAPABILITY_RECORDS } from '../../../src/tools/catalog/capabilities/records/aggregate.js';
import { validateAgainstCapabilitySchema } from '../../../src/server/gateway/gateway-schema-validate.js';

const inputOf = (id: string) => ALL_CAPABILITY_RECORDS.find((candidate) => String(candidate.id) === id)?.schemas.input;
const call = (window: unknown) => ({ action: 'screenshot', mode: 'full_editor_window', window });

describe('screenshot window: a list index or a title', () => {
  it.each(['control_editor.screenshot', 'system_control.screenshot'])('%s takes the window as an integer or a string', (id) => {
    const input = inputOf(id);

    expect(input, id).toBeDefined();
    expect(validateAgainstCapabilitySchema(call(1), input)).toBeUndefined();
    expect(validateAgainstCapabilitySchema(call('1'), input)).toBeUndefined();
    expect(validateAgainstCapabilitySchema(call('WBP_HubUI'), input)).toBeUndefined();
  });

  it.each(['control_editor.screenshot', 'system_control.screenshot'])('%s still refuses a window that is neither', (id) => {
    const input = inputOf(id);

    expect(validateAgainstCapabilitySchema(call(1.5), input)).toMatchObject({ reason: 'type' });
    expect(validateAgainstCapabilitySchema(call(true), input)).toMatchObject({ reason: 'type' });
    expect(validateAgainstCapabilitySchema(call(['1']), input)).toMatchObject({ reason: 'type' });
  });
});
