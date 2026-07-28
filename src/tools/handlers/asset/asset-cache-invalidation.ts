import { sanitizePath } from '../../../utils/paths/path-security.js';
import type { HandlerArgs } from '../../../types/handlers/handler-types.js';
import type { ITools } from '../../../types/tools/tool-interfaces.js';

/**
 * Asset actions that cannot change what a directory listing returns.
 *
 * The asset listing cache stores name/path/class rows per directory, so only a
 * mutation that adds, removes, moves or renames an asset can invalidate one.
 * Every action listed here is a pure read; anything else is treated as a
 * mutation, which keeps a newly added action fail-safe (it invalidates rather
 * than silently serving stale rows). Membership is asserted against the domain
 * action list in the colocated test so a typo cannot quietly widen this set.
 */
const LISTING_NEUTRAL_ASSET_ACTIONS: ReadonlySet<string> = new Set([
  'list',
  'exists',
  'validate',
  'search_assets',
  'find_by_tag',
  'get_dependencies',
  'get_metadata',
  'generate_report',
  'list_instances',
  'get_material_stats',
  'get_material_node_details',
  'get_source_control_state',
  'analyze_graph',
  'get_asset_graph',
  'get_struct',
  'read_struct',
  'list_struct_members',
  'list_structs',
  'compare_structs',
  'search_struct_usage',
  'export_struct',
  'get_row_struct',
  'get_data_table_row',
  'list_data_table_rows',
  'get_enum',
  'get_instanced_struct_property'
]);

/**
 * Argument fields that may name a content path a mutation touches.
 *
 * Values are accepted only when they resolve as Unreal content paths, so a
 * host filesystem argument such as `import.sourcePath` is filtered out rather
 * than evicting an unrelated cache key.
 */
const ASSET_PATH_FIELDS: readonly string[] = [
  'path',
  'directoryPath',
  'folderPath',
  'assetPath',
  'asset_path',
  'meshPath',
  'materialPath',
  'sourcePath',
  'destinationPath',
  'savePath',
  'packagePath',
  'paths',
  'assetPaths',
  'asset_paths'
];

export function isListingNeutralAssetAction(action: string): boolean {
  return LISTING_NEUTRAL_ASSET_ACTIONS.has(action);
}

export function listingNeutralAssetActions(): readonly string[] {
  return [...LISTING_NEUTRAL_ASSET_ACTIONS];
}

/**
 * Resolves a candidate value to an Unreal content path.
 *
 * `sanitizePath` is the single source of truth for which roots are content
 * roots, so this never introduces a second path-root policy. A value it
 * rejects (host path, traversal, illegal characters) is simply not a cache key
 * this mutation can affect.
 */
function toContentPath(value: unknown): string | undefined {
  if (typeof value !== 'string' || value.trim().length === 0) return undefined;
  try {
    return sanitizePath(value);
  } catch {
    return undefined;
  }
}

/**
 * Collects the content paths a mutation affects, from the declared path
 * fields only. Returns an empty list for a listing-neutral action.
 */
export function collectInvalidationPaths(action: string, args: HandlerArgs): string[] {
  if (isListingNeutralAssetAction(action)) return [];

  const record = args as Record<string, unknown>;
  const paths = new Set<string>();

  for (const field of ASSET_PATH_FIELDS) {
    const value = record[field];
    if (Array.isArray(value)) {
      for (const entry of value) {
        const resolved = toContentPath(entry);
        if (resolved !== undefined) paths.add(resolved);
      }
      continue;
    }

    const resolved = toContentPath(value);
    if (resolved !== undefined) paths.add(resolved);
  }

  return [...paths];
}

/**
 * Invalidates the asset listing cache entries a completed mutation affects.
 *
 * Called at the single asset-domain dispatch seam so no handler has to
 * remember to clear the cache. A mutation that reported failure changed
 * nothing, so its cache entries are left intact.
 */
export function invalidateAssetCacheForMutation(
  tools: ITools,
  action: string,
  args: HandlerArgs,
  result: Record<string, unknown>
): void {
  if (result.success === false) return;

  const paths = collectInvalidationPaths(action, args);
  if (paths.length === 0) return;

  tools.assetResources?.invalidateAssetPaths?.(paths);
}
