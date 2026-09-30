/**
 * Pin handles: `fromNodeId`+`fromPinName` and `toNodeId`+`toPinName` are the
 * correlation keys for connect_pins and break_pin_links. connect_pins uses
 * AllocateDefaultPins as a fallback when pins are not yet materialized.
 */
import type { CapabilityRecordSource } from '../../model.js';
import { BP_PLUGINS, buildRecord } from './helpers.js';
import { P } from './properties.js';

const FAMILY = 'graph';
const DOMAIN = 'blueprint';

export const GRAPH_PINS_RECORDS: readonly CapabilityRecordSource[] = [
  buildRecord({
    id: 'blueprint.connect_pins',
    action: 'connect_pins',
    family: FAMILY,
    domain: DOMAIN,
    topics: ['link pins', 'wire nodes', 'connect nodes', 'exec pin', 'connect output to input'],
    summary: 'Connect two graph pins (exec or data) by nodeGuid and pin name.',
    whenToUse: ['An exec or data link between two existing nodes must be created.'],
    whenNotToUse: ['Links must be broken (use break_pin_links).'],
    inputProps: { blueprintPath: P.blueprintPath, graphName: P.graphName, fromNodeId: P.fromNodeId, fromPinName: P.fromPinName, toNodeId: P.toNodeId, toPinName: P.toPinName, sourceNode: P.sourceNode, targetNode: P.targetNode, sourcePin: P.sourcePin, targetPin: P.targetPin },
    required: ['blueprintPath', 'fromNodeId', 'fromPinName', 'toNodeId', 'toPinName'],
    // The handler already reported which pins it resolved and whether the asset
    // was saved, but the default closed output schema stripped all of it, so a
    // successful link was indistinguishable from a no-op: "Pin connection
    // complete" and nothing else. `connected` is read back off the graph.
    outputProps: {
      connected: { type: 'boolean', description: 'Whether the two pins are linked after the call, read back from the graph rather than inferred from the schema call.' },
      sourcePinName: { type: 'string', description: 'Source pin that was actually used — the first output pin when fromPinName named none.' },
      targetPinName: { type: 'string', description: 'Target pin that was actually used.' },
      sourcePinType: { type: 'string', description: 'Pin category of the source pin (exec, object, real, ...).' },
      targetPinType: { type: 'string', description: 'Pin category of the target pin.' },
      blueprintPath: { type: 'string', description: 'Normalized blueprint path the link was written to.' },
      saved: { type: 'boolean', description: 'Whether the blueprint asset was saved after the link was made.' },
    },
    effect: 'write',
    behavior: { idempotency: 'idempotent' },
    latency: 'instant',
    resources: 'low',
    plugins: BP_PLUGINS,
    exampleInput: { action: 'connect_pins', blueprintPath: '/Game/Blueprints/BP_Test', graphName: 'EventGraph', fromNodeId: 'A1B2C3D4', fromPinName: 'OutExec', toNodeId: 'E5F6G7H8', toPinName: 'InExec' },
    exampleOutput: { success: true, message: 'Pin connection complete', connected: true, sourcePinName: 'then', targetPinName: 'execute', sourcePinType: 'exec', targetPinType: 'exec', saved: true },
  }),
  buildRecord({
    id: 'blueprint.break_pin_links',
    action: 'break_pin_links',
    family: FAMILY,
    domain: DOMAIN,
    summary: 'Break all links from one pin on a graph node.',
    whenToUse: ['All links from a specific pin must be removed.'],
    whenNotToUse: ['A single link should be redirected (use connect_pins after breaking).'],
    inputProps: { blueprintPath: P.blueprintPath, graphName: P.graphName, nodeId: P.nodeId, pinName: P.pinName, nodeGuid: P.nodeGuid },
    required: ['blueprintPath', 'pinName'],
    requiredOneOf: ['nodeId', 'nodeGuid'],
    effect: 'destructive',
    behavior: {  },
    latency: 'instant',
    resources: 'low',
    plugins: BP_PLUGINS,
    exampleInput: { action: 'break_pin_links', blueprintPath: '/Game/Blueprints/BP_Test', graphName: 'EventGraph', nodeId: 'A1B2C3D4', pinName: 'OutExec' },
  }),
  buildRecord({
    id: 'blueprint.set_node_property',
    action: 'set_node_property',
    family: FAMILY,
    domain: DOMAIN,
    summary: 'Set a property on a graph node by nodeGuid.',
    whenToUse: ['A node-level property (e.g. comment, position) must be updated.'],
    whenNotToUse: ['A pin default value is the target (use set_pin_default_value).'],
    inputProps: {
      blueprintPath: P.blueprintPath, graphName: P.graphName, nodeId: P.nodeId, propertyName: P.propertyName, nodeGuid: P.nodeGuid,
      propertyValue: { ...P.propertyValue, description: "set_node_property: text for NodeComment, a number for NodePosX or NodePosY, true or false for bCommentBubbleVisible or bDisabled, Enabled, Disabled or DevelopmentOnly for EnabledState, an asset path for a reflected field. set_pin_default_value: the pin's new default as text: a number, true or false, an enum value name, a vector as X,Y,Z (0,150,110), a rotator as P,Y,R, or an asset path." },
    },
    required: ['blueprintPath', 'propertyName', 'propertyValue'],
    requiredOneOf: ['nodeId', 'nodeGuid'],
    effect: 'write',
    behavior: { idempotency: 'idempotent', safeToRetry: true },
    latency: 'instant',
    resources: 'low',
    plugins: BP_PLUGINS,
    exampleInput: { action: 'set_node_property', blueprintPath: '/Game/Blueprints/BP_Test', graphName: 'EventGraph', nodeId: 'A1B2C3D4', propertyName: 'NodeComment', propertyValue: 'Entry point' },
  }),
  buildRecord({
    id: 'blueprint.set_pin_default_value',
    action: 'set_pin_default_value',
    family: FAMILY,
    domain: DOMAIN,
    summary: 'Set the default value of a data pin on a graph node; a read-only (const reference) pin gets it through a MakeLiteral node wired into it.',
    whenToUse: ['A pin\'s default literal value must be set when it has no incoming link.', 'A read-only text, string, name or number pin (TextRender Set Text\'s Value) needs a literal.'],
    whenNotToUse: ['The pin should receive its value from a linked node (use connect_pins).'],
    inputProps: { blueprintPath: P.blueprintPath, graphName: P.graphName, nodeId: P.nodeId, pinName: P.pinName, propertyValue: { ...P.propertyValue, description: "set_pin_default_value: the pin's new default as text: a number, true or false, an enum value name, a vector as X,Y,Z (0,150,110), a rotator as P,Y,R, or an asset path." }, nodeGuid: P.nodeGuid },
    required: ['blueprintPath', 'pinName'],
    requiredOneOf: ['nodeId', 'nodeGuid'],
    // appliedValue is read back off the pin after the schema has had its say, so
    // a caller can distinguish an accepted literal from one silently rejected —
    // the failure mode that let empty defaults pass as success.
    outputProps: {
      nodeId: P.nodeId, pinName: P.pinName,
      appliedValue: { type: 'string', description: 'Literal actually stored on the pin (or the resolved object path for object/class pins).' },
      literalNodeId: { type: 'string', description: 'Guid of the MakeLiteral node carrying the value when the pin itself takes no literal; setting the pin again updates that node.' },
    },
    effect: 'write',
    behavior: { idempotency: 'idempotent', safeToRetry: true },
    latency: 'instant',
    resources: 'low',
    plugins: BP_PLUGINS,
    exampleInput: { action: 'set_pin_default_value', blueprintPath: '/Game/Blueprints/BP_Test', graphName: 'EventGraph', nodeId: 'A1B2C3D4', pinName: 'InString', propertyValue: 'Hello' },
    exampleOutput: { success: true, message: 'Pin default value set', nodeId: 'A1B2C3D4', pinName: 'InString', appliedValue: 'Hello' },
  }),
  // A chain built without positions, or before the batch settle pass, can sit thousands of units from the
  // node it hangs off; this moves the nodes named beside what they are wired to (SettleAutoPlacedNodes).
  buildRecord({
    id: 'blueprint.arrange_nodes',
    action: 'arrange_nodes',
    family: FAMILY,
    domain: DOMAIN,
    topics: ['arrange nodes', 'tidy graph', 'clean up graph layout', 'move nodes next to each other', 'layout nodes'],
    summary: 'Tidy a graph: move the listed nodes beside the nodes they are wired to (right of what runs them, below-left of what reads them), each to the nearest free slot; the nodes left out stay put and anchor them.',
    whenToUse: ['Nodes of a chain sit far from each other or from the event they hang off, and should be laid out beside what they are wired to.'],
    whenNotToUse: ['One node must go to an exact position (use set_node_property with NodePosX and NodePosY).', 'Every listed node is wired only to other listed nodes: nothing anchors them, so none moves; leave the head of the chain (its event) out of nodeIds.'],
    inputProps: {
      blueprintPath: P.blueprintPath, graphName: P.graphName,
      nodeIds: { type: 'array', items: { type: 'string' }, minItems: 1, maxItems: 500, description: 'The nodes to move, by nodeId or nodeGuid. Nodes left out stay where they are and anchor the ones listed; a node wired to nothing left in place goes back where it was and is listed under unmoved.' },
    },
    required: ['blueprintPath', 'nodeIds'],
    outputProps: {
      moved: { type: 'array', items: { type: 'object', additionalProperties: true, 'x-unreal-reflection-boundary': true }, description: 'Each node that moved: nodeId, nodeTitle and its new x and y.' },
      unmoved: { type: 'array', items: { type: 'string' }, description: 'Ids of listed nodes that stayed where they were: wired to no node left in place, or no free slot near one.' },
    },
    outputRequired: [],
    effect: 'write',
    latency: 'interactive',
    resources: 'low',
    plugins: BP_PLUGINS,
    exampleInput: { action: 'arrange_nodes', blueprintPath: '/Game/Blueprints/BP_Test', graphName: 'EventGraph', nodeIds: ['A1B2C3D4', 'E5F6A7B8'] },
    exampleOutput: { success: true, message: 'Moved 2 of 2 nodes beside the nodes they are wired to.', moved: [{ nodeId: 'A1B2C3D4', nodeTitle: 'Print String', x: 480, y: 0 }], unmoved: [] },
  }),
  buildRecord({
    id: 'blueprint.add_construction_script',
    action: 'add_construction_script',
    family: FAMILY,
    domain: DOMAIN,
    summary: 'Add or open the Construction Script graph for a Blueprint.',
    whenToUse: ['The Construction Script graph must be created or opened for editing.'],
    whenNotToUse: ['An event graph is needed (use create_node in EventGraph).'],
    inputProps: { blueprintPath: P.blueprintPath },
    required: ['blueprintPath'],
    outputProps: { graphName: P.graphName },
    outputRequired: ['graphName'],
    effect: 'write',
    latency: 'interactive',
    resources: 'low',
    plugins: BP_PLUGINS,
    exampleInput: { action: 'add_construction_script', blueprintPath: '/Game/Blueprints/BP_Test' },
    exampleOutput: { success: true, graphName: 'ConstructionScript' },
  }),
];
