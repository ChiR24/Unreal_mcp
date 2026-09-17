import { describe, expect, it } from 'vitest';

import { describeUndeclaredParameter } from '../../src/server/gateway/gateway-schema-validate.js';

// blueprint.edit_graph declares 33 parameters. Sending `defaultValue` for a pin
// default used to answer with no "did you mean" (no substring overlap with
// `propertyValue`) and an alphabetical list truncated at 24 that spent every
// slot on names before 'p' — so the one parameter that works was inside "and 9
// more". Two more round trips to learn a name the validator was holding.
const EDIT_GRAPH_PARAMETERS = Object.fromEntries(
  [
    'action', 'actionPath', 'blueprintPath', 'customEventName', 'edit', 'eventName',
    'fromNodeId', 'fromPinName', 'functionName', 'graphName', 'inputActionAssetPath',
    'inputActionPath', 'inputAxisName', 'linkedTo', 'memberClass', 'memberName',
    'nodeGuid', 'nodeId', 'nodeName', 'nodeType', 'pinName', 'posX', 'posY',
    'propertyName', 'propertyValue', 'sourceNode', 'sourcePin', 'structPath',
    'targetClass', 'targetNode', 'targetPin', 'toNodeId', 'toPinName'
  ].map((name) => [name, { type: 'string' }])
);

describe('describeUndeclaredParameter', () => {
  it('suggests and lists the synonym a substring match would miss', () => {
    const message = describeUndeclaredParameter('defaultValue', EDIT_GRAPH_PARAMETERS);

    expect(message).toContain("did you mean 'propertyValue'");
    expect(message.slice(0, message.indexOf(' and '))).toContain('propertyValue');
  });

  it('still ranks an exact substring match above a shared token', () => {
    const message = describeUndeclaredParameter('pinDefaultValue', EDIT_GRAPH_PARAMETERS);

    expect(message).toContain("did you mean 'pinName'");
  });

  it('leaves an unrelated name with the plain alphabetical list', () => {
    const message = describeUndeclaredParameter('bogus', { action: { type: 'string' } });

    expect(message).toBe("Undeclared parameter 'bogus' (allowed: action)");
  });
});
