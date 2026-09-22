// Reusable outcome metadata extraction for the success receipt. A handler result
// that truthfully reports a created/changed entity, a typed handle, or a task
// state is surfaced as independently reusable receipt metadata, using strict,
// bounded parsing against the existing semantic schemas. Nothing is fabricated:
// a field the handler did not report stays absent rather than empty-by-default
// overclaiming, and every derived handle is proven by its own schema before it
// is emitted (a malformed identifier is dropped, never coerced).

import { isRecord } from '../../../../utils/validation/type-guards.js';
import { TaskStatusSchema, type TaskStatus } from './errors.js';
import { TypedHandleSchema, type TypedHandle } from './handles.js';

// A handler result carries its payload either at the root or nested under `data`
// (the WebSocket frame the gateway projects embeds the verdict separately from
// the payload), then `details` (the gateway folds undeclared handler fields
// into it). Field order below (root, then data, then details) matches the
// native ReadField order, so both transports read the same winner.
function makeReader(result: Record<string, unknown>): (key: string) => unknown {
  const data = isRecord(result.data) ? result.data : undefined;
  const details = isRecord(result.details) ? result.details : undefined;
  const dataDetails = isRecord(data?.details) ? data.details : undefined;
  return (key) => {
    const rooted = result[key];
    if (rooted !== undefined) return rooted;
    const nested = data?.[key];
    if (nested !== undefined) return nested;
    const inDetails = details?.[key];
    if (inDetails !== undefined) return inDetails;
    return dataDetails?.[key];
  };
}

// widgetPath is the canonical asset path of a Widget Blueprint and is what the
// WidgetAuthoring handlers already emit, so without it every widget mutation
// produced a receipt with no asset handle at all.
const ASSET_FIELDS = ['assetPath', 'createdAssetPath', 'savedAssetPath', 'destinationPath', 'widgetPath', 'deletedPath'] as const;
const ACTOR_FIELDS = ['actorName', 'actorLabel', 'actorPath'] as const;
const CHANGE_ARRAY_FIELDS = ['changes', 'changedEntities', 'changedAssets', 'affectedActors', 'modifiedPaths', 'deleted'] as const;
// widgetPath was listed for handles but not here, so create_game_screen and
// create_widget_template published a handle to a brand-new asset while leaving
// changes[] empty — a caller diffing changes[] missed every widget it authored.
const CHANGE_SINGLE_FIELDS = ['assetPath', 'createdAssetPath', 'savedAssetPath', 'destinationPath', 'deletedPath', 'widgetPath', 'actorName', 'actorPath'] as const;

export function extractHandles(result: unknown): TypedHandle[] {
  if (!isRecord(result)) return [];
  const read = makeReader(result);
  const handles: TypedHandle[] = [];
  const seen = new Set<string>();
  const add = (handle: TypedHandle): void => {
    const key = JSON.stringify(handle);
    if (!seen.has(key)) {
      seen.add(key);
      handles.push(handle);
    }
  };

  const explicit = read('handles');
  if (Array.isArray(explicit)) {
    for (const raw of explicit) {
      const parsed = TypedHandleSchema.safeParse(raw);
      if (parsed.success) add(parsed.data);
    }
  }

  for (const field of ASSET_FIELDS) {
    const value = read(field);
    if (typeof value === 'string') {
      const parsed = TypedHandleSchema.safeParse({ kind: 'asset', path: value });
      if (parsed.success) {
        add(parsed.data);
        break;
      }
    }
  }

  for (const field of ACTOR_FIELDS) {
    const value = read(field);
    if (typeof value === 'string' && value.length > 0) {
      const parsed = TypedHandleSchema.safeParse({ kind: 'actor', ref: value });
      if (parsed.success) {
        add(parsed.data);
        break;
      }
    }
  }

  return handles;
}

export function extractChanges(result: unknown): string[] {
  if (!isRecord(result)) return [];
  const read = makeReader(result);
  const collected: string[] = [];
  for (const field of CHANGE_ARRAY_FIELDS) {
    const value = read(field);
    if (Array.isArray(value)) {
      for (const entry of value) {
        if (typeof entry === 'string' && entry.length > 0) collected.push(entry);
      }
    }
  }
  for (const field of CHANGE_SINGLE_FIELDS) {
    const value = read(field);
    if (typeof value === 'string' && value.length > 0) collected.push(value);
  }
  return [...new Set(collected)];
}

export function extractTask(result: unknown): TaskStatus | undefined {
  if (!isRecord(result)) return undefined;
  const read = makeReader(result);
  const raw = read('task') ?? read('taskStatus');
  if (!isRecord(raw)) return undefined;
  const parsed = TaskStatusSchema.safeParse(raw);
  return parsed.success ? parsed.data : undefined;
}
