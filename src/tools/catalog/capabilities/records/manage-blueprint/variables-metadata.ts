/**
 * set_default targets the Class Default Object (CDO) via
 * GeneratedClass->GetDefaultObject(); if the property denotes a component,
 * the handler dereferences the component CDO. This is the only true CDO
 * fallback path; SCS template writes go through set_scs_property instead.
 */
import type { CapabilityRecordSource } from '../../model.js';
import { BP_PLUGINS, buildRecord } from './helpers.js';
import { P } from './properties.js';

const FAMILY = 'variables';
const DOMAIN = 'blueprint';

export const VARIABLES_METADATA_RECORDS: readonly CapabilityRecordSource[] = [
  buildRecord({
    id: 'blueprint.add_variable',
    action: 'add_variable',
    family: FAMILY,
    domain: DOMAIN,
    topics: ['new variable', 'member variable', 'add property', 'float variable', 'bool variable', 'integer variable', 'declare variable'],
    summary: 'Add a new member variable to a Blueprint.',
    whenToUse: ['A Blueprint needs a new member variable with a type and default.'],
    whenNotToUse: ['A graph-local variable is needed (use create_node with a local variable).'],
    inputProps: { blueprintPath: P.blueprintPath, variableName: P.variableName, variableType: P.variableType, defaultValue: P.defaultValue, category: P.category, isReplicated: P.isReplicated, isPublic: P.isPublic },
    required: ['blueprintPath', 'variableName', 'variableType'],
    outputProps: { variableName: P.variableName },
    outputRequired: ['variableName'],
    effect: 'write',
    latency: 'interactive',
    resources: 'low',
    plugins: BP_PLUGINS,
    exampleInput: { action: 'add_variable', blueprintPath: '/Game/Blueprints/BP_Test', variableName: 'Health', variableType: 'Float', defaultValue: 100, category: 'Stats', isReplicated: false, isPublic: true },
    exampleOutput: { success: true, variableName: 'Health' },
  }),
  buildRecord({
    id: 'blueprint.remove_variable',
    action: 'remove_variable',
    family: FAMILY,
    domain: DOMAIN,
    summary: 'Permanently remove a member variable from a Blueprint.',
    whenToUse: ['A member variable must be permanently deleted.'],
    whenNotToUse: ['The variable should be renamed rather than removed.'],
    inputProps: { blueprintPath: P.blueprintPath, variableName: P.variableName },
    required: ['blueprintPath', 'variableName'],
    effect: 'destructive',
    behavior: { safeToRetry: false },
    latency: 'interactive',
    resources: 'low',
    plugins: BP_PLUGINS,
    exampleInput: { action: 'remove_variable', blueprintPath: '/Game/Blueprints/BP_Test', variableName: 'OldStat' },
  }),
  buildRecord({
    id: 'blueprint.rename_variable',
    action: 'rename_variable',
    family: FAMILY,
    domain: DOMAIN,
    summary: 'Rename a member variable or a component in a Blueprint; its graph nodes follow.',
    whenToUse: ['A member variable needs a new name.', 'A component (Simple Construction Script node) needs a new name.'],
    whenNotToUse: ['The variable should be removed rather than renamed.'],
    inputProps: {
      blueprintPath: P.blueprintPath,
      oldName: { type: 'string', description: 'Current name of the variable or component.' },
      newName: P.newName,
    },
    required: ['blueprintPath', 'oldName', 'newName'],
    effect: 'write',
    behavior: { idempotency: 'idempotent' },
    latency: 'interactive',
    resources: 'low',
    plugins: BP_PLUGINS,
    exampleInput: { action: 'rename_variable', blueprintPath: '/Game/Blueprints/BP_Test', oldName: 'HP', newName: 'Health' },
  }),
  buildRecord({
    id: 'blueprint.set_variable_metadata',
    action: 'set_variable_metadata',
    family: FAMILY,
    domain: DOMAIN,
    summary: 'Set metadata (tooltip, category, replication) on a Blueprint member variable.',
    whenToUse: ['Variable metadata such as tooltip or category must be updated.'],
    whenNotToUse: ['Only the default value is needed (use set_default).'],
    inputProps: {
      blueprintPath: P.blueprintPath, variableName: P.variableName, metadata: P.metadata,
      variableNames: {
        type: 'array', items: { type: 'string' },
        description: 'Several variables to give the same metadata in one call (one compile and save), in place of or besides variableName.',
      },
    },
    required: ['blueprintPath'],
    requiredOneOf: ['variableName', 'variableNames'],
    outputProps: {
      variableName: P.variableName,
      variableNames: { type: 'array', items: { type: 'string' }, description: 'Every variable that took the metadata.' },
    },
    effect: 'write',
    behavior: { idempotency: 'idempotent', safeToRetry: true },
    latency: 'instant',
    resources: 'low',
    plugins: BP_PLUGINS,
    exampleInput: { action: 'set_variable_metadata', blueprintPath: '/Game/Blueprints/BP_Test', variableName: 'Health', metadata: { tooltip: 'Current health points', category: 'Stats' } },
  }),
  buildRecord({
    id: 'blueprint.set_metadata',
    action: 'set_metadata',
    family: FAMILY,
    domain: DOMAIN,
    summary: 'Set arbitrary metadata key-value pairs on a Blueprint class, or on one member variable.',
    whenToUse: ['Freeform metadata must be attached to a Blueprint or its members.'],
    whenNotToUse: ['Several variables need the same metadata (set_variable_metadata takes variableNames).'],
    inputProps: {
      blueprintPath: P.blueprintPath, metadata: P.metadata,
      propertyName: { ...P.propertyName, description: 'A member variable to put the metadata on (as set_variable_metadata does); omitted, the keys go on the Blueprint class.' },
    },
    required: ['blueprintPath', 'metadata'],
    effect: 'write',
    behavior: { idempotency: 'idempotent', safeToRetry: true },
    latency: 'instant',
    resources: 'low',
    plugins: BP_PLUGINS,
    exampleInput: { action: 'set_metadata', blueprintPath: '/Game/Blueprints/BP_Test', metadata: { author: 'MCP', version: '2' } },
  }),
  buildRecord({
    id: 'blueprint.set_default',
    action: 'set_default',
    family: FAMILY,
    domain: DOMAIN,
    topics: ['default value', 'cdo default', 'variable default', 'set property default'],
    aliases: ['blueprint.set_default_value'],
    summary: 'Set a default property value on the Blueprint CDO (Class Default Object).',
    whenToUse: ['A default property must be set on the CDO; if the property denotes a component, the component CDO is targeted.'],
    whenNotToUse: ['An SCS component template property is the target (use set_scs_property).'],
    inputProps: { blueprintPath: P.blueprintPath, propertyName: P.propertyName, propertyValue: P.propertyValue },
    required: ['blueprintPath', 'propertyName'],
    // The literal set_default path re-reads the CDO property after the write and
    // returns it as `value`, but only when it exports to JSON; the object path
    // returns neither value. `value` is therefore an optional output, and
    // `verifiedValue` (which no set_default path emits) is not declared.
    outputProps: { value: { description: 'Property value re-read from the Class Default Object after the write. Emitted on the literal path only and omitted when the value cannot be exported to JSON.' } },
    effect: 'write',
    behavior: { idempotency: 'idempotent', safeToRetry: true },
    latency: 'instant',
    resources: 'low',
    plugins: BP_PLUGINS,
    exampleInput: { action: 'set_default', blueprintPath: '/Game/Blueprints/BP_Test', propertyName: 'InitialHealth', propertyValue: 100 },
    exampleOutput: { success: true, value: 100 },
  }),
];
