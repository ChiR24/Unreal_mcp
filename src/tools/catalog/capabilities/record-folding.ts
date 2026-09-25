import { z } from 'zod';

import { readField } from './hashing.js';
import { hasOwn, isRecord } from '../../../utils/validation/type-guards.js';

/**
 * A folded family is one record standing for several handler actions. These
 * invariants keep that data self-consistent, so a fold can never dispatch an
 * action the handlers lack or accept a selector value the map cannot route:
 *   - legacyIds[0] is the advertised primary and is never folded;
 *   - a folded pair pins only declared input properties, and a pinned value
 *     honours the property's enum when it declares one;
 *   - a pair folded under a REQUIRED selector pins that selector, so a call by
 *     the old name can satisfy validation;
 *   - routing.dispatchBy names a declared input property whose enum values
 *     are exactly the map's keys, every mapped action is one of the record's
 *     legacy pairs, and a mapped pair's pin agrees with the value mapping to
 *     it — the two call forms of one family dispatch the same action;
 *   - routing.dispatchBy.declaredBy names declared input properties and maps
 *     each to selector values the map can route.
 */
export function verifyFolding(record: Record<string, unknown>, ctx: z.RefinementCtx): void {
  const legacyIds = readField(record, 'legacyIds');
  if (!Array.isArray(legacyIds)) return;
  const schemas = readField(record, 'schemas');
  const input = isRecord(schemas) ? readField(schemas, 'input') : undefined;
  const properties = isRecord(input) && isRecord(input.properties) ? input.properties : {};
  const required = isRecord(input) && Array.isArray(input.required) ? input.required : [];
  const enumOf = (name: string): readonly unknown[] | undefined => {
    const property = readField(properties, name);
    return isRecord(property) && Array.isArray(property.enum) ? property.enum : undefined;
  };
  const hasDefault = (name: string): boolean => {
    const property = readField(properties, name);
    return isRecord(property) && 'default' in property;
  };

  const pairActions = new Set<string>();
  for (const entry of legacyIds) {
    if (isRecord(entry) && typeof entry.action === 'string') pairActions.add(entry.action);
  }
  legacyIds.forEach((entry, position) => {
    if (!isRecord(entry) || !isRecord(entry.folded)) return;
    if (position === 0) {
      ctx.addIssue({
        code: 'custom',
        path: ['legacyIds', 0, 'folded'],
        message: 'legacyIds[0] is the advertised primary and cannot be folded'
      });
    }
    for (const [name, value] of Object.entries(entry.folded)) {
      if (!hasOwn(properties, name)) {
        ctx.addIssue({
          code: 'custom',
          path: ['legacyIds', position, 'folded', name],
          message: 'a folded pin must name a declared input property'
        });
        continue;
      }
      const allowed = enumOf(name);
      if (allowed !== undefined && !allowed.includes(value)) {
        ctx.addIssue({
          code: 'custom',
          path: ['legacyIds', position, 'folded', name],
          message: 'a folded pin value must be one of the property enum values'
        });
      }
    }
  });

  const routing = readField(record, 'routing');
  const dispatchBy = isRecord(routing) ? readField(routing, 'dispatchBy') : undefined;
  if (dispatchBy === undefined) return;
  // Shape errors are reported by routingSchema; only the cross-field rules live here.
  if (!isRecord(dispatchBy) || typeof dispatchBy.param !== 'string' || !isRecord(dispatchBy.actions)) return;
  const selector = dispatchBy.param;
  const selectorRequired = required.includes(selector) && !hasDefault(selector);
  const pinOf = (action: string): string | undefined => {
    for (const entry of legacyIds) {
      if (isRecord(entry) && entry.action === action && isRecord(entry.folded)
        && typeof entry.folded[selector] === 'string') {
        return entry.folded[selector];
      }
    }
    return undefined;
  };
  if (selectorRequired) {
    legacyIds.forEach((entry, position) => {
      if (position === 0 || !isRecord(entry) || !isRecord(entry.folded)) return;
      if (pinOf(String(entry.action)) === undefined) {
        ctx.addIssue({
          code: 'custom',
          path: ['legacyIds', position, 'folded', selector],
          message: `a pair folded under the required selector '${selector}' must pin that selector, or a call by its old name cannot satisfy validation`
        });
      }
    });
  }
  const allowed = enumOf(selector);
  if (allowed === undefined) {
    ctx.addIssue({
      code: 'custom',
      path: ['routing', 'dispatchBy', 'param'],
      message: 'dispatchBy.param must name a declared input property that declares an enum'
    });
    return;
  }
  const keys = Object.keys(dispatchBy.actions);
  const allowedValues = allowed.filter((value): value is string => typeof value === 'string');
  const missing = allowedValues.filter((value) => !keys.includes(value));
  const extra = keys.filter((key) => !allowedValues.includes(key));
  if (missing.length > 0 || extra.length > 0) {
    ctx.addIssue({
      code: 'custom',
      path: ['routing', 'dispatchBy', 'actions'],
      message: `dispatchBy.actions keys must equal the enum of '${selector}' `
        + `(missing: ${missing.join(', ') || 'none'}; extra: ${extra.join(', ') || 'none'})`
    });
  }
  if (isRecord(dispatchBy.declaredBy)) {
    for (const [name, values] of Object.entries(dispatchBy.declaredBy)) {
      if (!hasOwn(properties, name) || !Array.isArray(values) || !values.every((value) => keys.includes(String(value)))) {
        ctx.addIssue({
          code: 'custom',
          path: ['routing', 'dispatchBy', 'declaredBy', name],
          message: 'declaredBy must name a declared input property and map it to selector values dispatchBy.actions routes'
        });
      }
    }
  }
  for (const [value, action] of Object.entries(dispatchBy.actions)) {
    if (typeof action !== 'string') continue;
    if (!pairActions.has(action)) {
      ctx.addIssue({
        code: 'custom',
        path: ['routing', 'dispatchBy', 'actions', value],
        message: 'a dispatchBy action must be one of the record\'s legacy pairs: its folded legacy actions or its primary'
      });
      continue;
    }
    const pin = pinOf(action);
    if (pin !== undefined && pin !== value) {
      ctx.addIssue({
        code: 'custom',
        path: ['routing', 'dispatchBy', 'actions', value],
        message: `the pair for '${action}' pins selector '${selector}' to '${pin}', but dispatchBy maps '${value}' to it; the two call forms of one family must dispatch the same action`
      });
    }
  }
}
