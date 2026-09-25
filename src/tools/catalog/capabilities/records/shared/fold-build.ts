// src/tools/catalog/capabilities/records/shared/fold-build.ts
//
// Assembles the single folded record a fold spec produces from its resolved
// members: union schema, legacy pairs with pins, aliases, discovery text and
// behaviour/cost facets.

import type {
  CapabilityRecordSource,
  Draft202012ObjectSchema,
  JsonObject,
  JsonValue,
  LegacyCapabilityId,
} from '../../model.js';
import {
  CapabilityAliasSchema,
  CapabilityIdSchema,
  LegacyActionNameSchema,
  LegacyToolNameSchema,
} from '../../identifiers.js';
import { hasOwn } from '../../../../../utils/validation/type-guards.js';
import type { FoldSpec, MemberEntry } from './fold-types.js';
import { sameJson, unique } from './fold-support.js';
import { intersectRequired, mergeProperties } from './fold-widen.js';

const LATENCY_RANK: Readonly<Record<string, number>> = { instant: 0, interactive: 1, 'long-running': 2 };
const RESOURCE_RANK: Readonly<Record<string, number>> = { low: 0, medium: 1, high: 2 };
const MAX_TOPICS = 8;
const MAX_WHEN = 6;

const namespaceOf = (id: string): string => id.slice(0, id.lastIndexOf('.'));
const actionProp = (): JsonObject => ({ type: 'string', description: 'The action to execute on the parent tool.' });

// Union of every member's group: each member's own group is a subset, so the
// union accepts what every member accepted and still refuses a bare call no
// member would have accepted. A member with no group accepts a parameterless
// call, so the obligation drops rather than imposing a sibling's group on it.
function unionRequiredOneOf(members: readonly CapabilityRecordSource[]): readonly string[] | undefined {
  if (members.length === 0) return undefined;
  if (!members.every((member) => member.schemas.input.requiredOneOf !== undefined)) return undefined;
  const union = unique(members.flatMap((member) => [...(member.schemas.input.requiredOneOf ?? [])]));
  return union.length === 0 ? undefined : union;
}

// Which variants declare each parameter the variants do not all share. A call
// that omits the selector but sends `operations` can only mean the batch
// variant; without this it ran the default variant, which ignored the steps.
function declaredBy(
  entries: readonly MemberEntry[],
  members: readonly CapabilityRecordSource[],
): Record<string, string[]> | undefined {
  const owners: Record<string, string[]> = {};
  let variants = 0;
  entries.forEach((entry, index) => {
    const properties = members[index]?.schemas.input.properties;
    if (entry.value === undefined || properties === undefined) return;
    variants += 1;
    for (const name of Object.keys(properties)) {
      if (name !== 'action') (owners[name] ??= []).push(entry.value);
    }
  });
  const distinct = Object.entries(owners).filter(([, values]) => values.length < variants);
  return distinct.length === 0 ? undefined : Object.fromEntries(distinct);
}

function objectSchema(
  template: Draft202012ObjectSchema,
  properties: JsonObject,
  required: readonly string[],
  requiredOneOf?: readonly string[],
): Draft202012ObjectSchema {
  return {
    $schema: template.$schema,
    type: 'object',
    properties,
    required: [...required],
    additionalProperties: false,
    ...(requiredOneOf === undefined ? {} : { requiredOneOf: [...requiredOneOf] }),
  };
}

function maxBy<T extends string>(values: readonly T[], rank: Readonly<Record<string, number>>): T {
  return values.reduce((best, value) => ((rank[value] ?? 0) > (rank[best] ?? 0) ? value : best));
}

/**
 * A fold that keeps one member as its primary keeps that member's inventory
 * classification (the audit adjudicated that name); a fold under a new name
 * is a retained same-verb family in its own right.
 */
function foldedNormalization(
  spec: FoldSpec,
  base: CapabilityRecordSource,
  members: readonly CapabilityRecordSource[],
  primaryIsMember: boolean,
  allPostMigration: boolean,
): CapabilityRecordSource['normalization'] {
  const family = `Folded family: ${spec.primary} stands for ${members.length} sibling actions`
    + `${spec.selector === undefined ? '' : ` selected by ${spec.selector}`}; each former name stays callable as a folded legacy pair.`;
  if (primaryIsMember) {
    return { ...base.normalization, rationale: `${base.normalization.rationale} ${family}` };
  }
  return {
    class: 'C_SAME_VERB_DIFFERENT_TARGET',
    disposition: 'retain',
    rationale: family,
    ...(allPostMigration ? { provenance: 'post-migration' as const } : {}),
  };
}

type Provenance = { readonly provenance: 'post-migration' } | Record<string, never>;
const postMigration = (member: CapabilityRecordSource | undefined): Provenance =>
  member?.normalization.provenance === 'post-migration' ? { provenance: 'post-migration' } : {};

export function buildFolded(
  parentTool: string,
  spec: FoldSpec,
  entries: readonly MemberEntry[],
  members: readonly CapabilityRecordSource[],
): CapabilityRecordSource {
  const tool = LegacyToolNameSchema.parse(parentTool);
  const first = members[0];
  if (first === undefined) throw new Error(`fold ${spec.primary}: no members`);
  const context = `fold ${spec.primary} (${parentTool})`;
  const namespace = namespaceOf(String(first.id));
  const primaryIndex = entries.findIndex((entry) => entry.action === spec.primary);
  const base = members[primaryIndex] ?? first;
  const primaryValue = entries[primaryIndex]?.value;
  const selector = spec.selector;
  const selected = entries.filter((entry): entry is MemberEntry & { value: string } => entry.value !== undefined);
  const memberActions = new Set(entries.map((entry) => entry.action));

  const inputProperties: Record<string, JsonValue> = {
    ...(hasOwn(base.schemas.input.properties, 'action') ? { action: actionProp() } : {}),
    ...mergeProperties(members, 'input', context),
  };
  if (selector !== undefined) {
    const label = spec.primary.replace(/_/g, ' ');
    inputProperties[selector] = {
      type: 'string',
      enum: selected.map((entry) => entry.value),
      description: primaryValue === undefined
        ? `Which ${label} variant to run.`
        : `Which ${label} variant to run; omit for '${primaryValue}'.`,
      ...(primaryValue === undefined ? {} : { default: primaryValue }),
    };
  }
  const selectorRequired = selector !== undefined && primaryValue === undefined;
  // Whether `action` is listed as required follows the base record: the
  // gateway always supplies it, so the listing is contract style, not a gate.
  const inputRequired = unique([
    ...(base.schemas.input.required.includes('action') ? ['action'] : []),
    ...(selectorRequired ? [selector] : []),
    ...intersectRequired(members, 'input'),
  ]);
  const input = objectSchema(first.schemas.input, inputProperties, inputRequired, unionRequiredOneOf(members));
  const outputProperties = mergeProperties(members, 'output', context);
  const output = objectSchema(first.schemas.output, outputProperties, unique(['success', ...intersectRequired(members, 'output')]));

  const legacyIds: LegacyCapabilityId[] = [];
  const primaryPair = members[primaryIndex]?.legacyIds[0];
  legacyIds.push(primaryPair !== undefined
    ? { ...primaryPair, ...postMigration(members[primaryIndex]) }
    : { tool, action: LegacyActionNameSchema.parse(spec.primary), provenance: 'post-migration' });
  entries.forEach((entry, index) => {
    if (index === primaryIndex) return;
    legacyIds.push({
      tool,
      action: LegacyActionNameSchema.parse(entry.action),
      folded: selector === undefined || entry.pin === undefined ? {} : { [selector]: entry.pin },
      ...postMigration(members[index]),
    });
  });

  const id = CapabilityIdSchema.parse(`${namespace}.${spec.primary}`);
  const aliases = unique(
    members.flatMap((member, index) => [
      ...(index === primaryIndex ? [] : [`${namespace}.${entries[index]?.action ?? ''}`]),
      ...member.aliases.map(String),
    ]),
  ).filter((alias) => alias !== String(id)).map((alias) => CapabilityAliasSchema.parse(alias));

  const derivedTopics = unique(members.flatMap((member) =>
    member.discovery.topics.filter((topic) => !memberActions.has(topic) && topic !== spec.primary)));
  const topics = unique([spec.primary, ...(spec.topics ?? derivedTopics.slice(0, MAX_TOPICS))]);
  const whenToUse = spec.whenToUse ?? unique(members.flatMap((member) => member.discovery.whenToUse)).slice(0, MAX_WHEN);
  const whenNotToUse = spec.whenNotToUse ?? unique(members.flatMap((member) => member.discovery.whenNotToUse)).slice(0, MAX_WHEN);

  const idempotency = members.every((member) => member.behavior.idempotency === first.behavior.idempotency)
    ? first.behavior.idempotency
    : 'non-idempotent';
  const semantics = first.behavior.semantics !== undefined
    && members.every((member) => sameJson(member.behavior.semantics, first.behavior.semantics))
    ? first.behavior.semantics
    : undefined;

  const example = base.examples[0];
  const exampleValue = primaryValue ?? selected[0]?.value;
  const exampleInput: JsonObject = {
    ...(example?.input ?? {}),
    action: spec.primary,
    ...(selector !== undefined && exampleValue !== undefined ? { [selector]: exampleValue } : {}),
  };

  const allPostMigration = members.every((member) => member.normalization.provenance === 'post-migration');
  const owners = selector === undefined ? undefined : declaredBy(entries, members);

  return {
    id,
    aliases,
    legacyIds,
    discovery: {
      domain: first.discovery.domain,
      family: first.discovery.family,
      topics,
      summary: spec.summary,
      whenToUse,
      whenNotToUse,
    },
    schemas: { input, output },
    examples: [{ title: spec.summary, input: exampleInput, output: example?.output ?? { success: true } }],
    availability: first.availability,
    behavior: {
      effect: first.behavior.effect,
      idempotency,
      longRunning: members.some((member) => member.behavior.longRunning),
      safeToRetry: members.every((member) => member.behavior.safeToRetry),
      supportsPreview: members.every((member) => member.behavior.supportsPreview),
      supportsUndo: members.every((member) => member.behavior.supportsUndo),
      ...(semantics === undefined ? {} : { semantics }),
    },
    policy: first.policy,
    cost: {
      latency: maxBy(members.map((member) => member.cost.latency), LATENCY_RANK) as CapabilityRecordSource['cost']['latency'],
      resources: maxBy(members.map((member) => member.cost.resources), RESOURCE_RANK) as CapabilityRecordSource['cost']['resources'],
    },
    routing: {
      parentTool: tool,
      dispatchAction: base.routing.dispatchAction,
      dispatchMode: first.routing.dispatchMode,
      ...(selector === undefined
        ? {}
        : {
          dispatchBy: {
            param: selector,
            actions: Object.fromEntries(selected.map((entry) => [entry.value, LegacyActionNameSchema.parse(entry.action)])),
            ...(owners === undefined ? {} : { declaredBy: owners }),
          },
        }),
    },
    normalization: foldedNormalization(spec, base, members, primaryIndex >= 0, allPostMigration),
    deprecation: { status: 'active' },
    parent: first.parent,
  };
}
