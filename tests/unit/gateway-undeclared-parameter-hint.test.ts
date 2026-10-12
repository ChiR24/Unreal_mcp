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
    const message = describeUndeclaredParameter('bogus', { mode: { type: 'string' } });

    expect(message).toBe("Undeclared parameter 'bogus' (allowed: mode)");
  });

  // get_transform with motion: true answered "allowed: ..., readMode, ..." and never said motion was a readMode value.
  it('names the parameter a key is a value of, ahead of any name match', () => {
    const transform = { actorName: { type: 'string' }, readMode: { type: 'string', enum: ['now', 'motion'] } };

    expect(describeUndeclaredParameter('motion', transform)).toBe(
      "Undeclared parameter 'motion' (did you mean readMode: 'motion'; allowed: actorName, readMode)"
    );
    expect(describeUndeclaredParameter('Motion', transform)).toContain("did you mean readMode: 'motion'");
  });

  it('never offers the envelope fields, which execute refuses inside params', () => {
    const play = { action: { type: 'string' }, actorName: { type: 'string' }, control: { type: 'string' } };

    expect(describeUndeclaredParameter('playAction', play)).toBe(
      "Undeclared parameter 'playAction' (allowed: actorName, control)"
    );
    expect(describeUndeclaredParameter('bogus', { action: { type: 'string' } })).toBe(
      "Undeclared parameter 'bogus' (this action declares no parameters)"
    );
  });
});
