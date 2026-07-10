import { cleanObject } from '../../../utils/serialization/safe-json.js';

const VALID_ASSET_ACTIONS = new Set([
  'list', 'import', 'duplicate', 'rename', 'move', 'delete',
  'create_folder', 'search_assets', 'get_dependencies', 'validate',
  'fixup_redirectors', 'find_by_tag', 'exists', 'bulk_rename', 'bulk_delete',
  'duplicate_asset', 'rename_asset', 'move_asset', 'delete_asset', 'delete_assets',
  'create_thumbnail', 'set_tags', 'get_metadata', 'set_metadata', 'generate_report',
  'create_material', 'create_material_instance', 'create_render_target',
  'generate_lods', 'add_material_parameter', 'list_instances',
  'reset_instance_parameters', 'get_material_stats', 'nanite_rebuild_mesh',
  'add_material_node', 'remove_material_node', 'rebuild_material',
  'connect_material_pins', 'break_material_connections', 'get_material_node_details',
  'source_control_checkout', 'source_control_submit', 'source_control_enable', 'get_source_control_state',
  'analyze_graph', 'get_asset_graph',
  // Struct authoring (first-class Blueprint Struct support, issue #510)
  'create_struct', 'get_struct', 'read_struct', 'list_struct_members',
  'add_struct_member', 'remove_struct_member', 'rename_struct_member',
  'set_struct_member_type', 'reorder_struct_members', 'set_struct_member_default',
  'set_struct_member_metadata', 'compare_structs', 'search_struct_usage', 'recompile_struct',
  'rename_struct', 'duplicate_struct', 'delete_struct', 'refresh_struct_dependencies',
  'list_structs', 'export_struct', 'import_struct',
  // Struct ecosystem — DataTable (issue #struct-ecosystem)
  'create_data_table', 'set_data_table_row_struct', 'create_row_struct', 'get_row_struct',
  'set_struct_as_row_struct', 'add_data_table_row', 'get_data_table_row',
  'update_data_table_row', 'delete_data_table_row', 'list_data_table_rows',
  'import_data_table_rows', 'clear_data_table_rows',
  // Struct ecosystem — Enum
  'create_enum', 'delete_enum', 'get_enum', 'add_enum_value', 'remove_enum_value',
  'rename_enum_value', 'reorder_enum_values', 'set_enum_value_metadata', 'split_enum',
  // Struct ecosystem — FInstancedStruct
  'get_instanced_struct_property', 'set_instanced_struct_property'
]);

const TRAVERSAL_PATTERNS = [
  '../', '..\\',
  '/etc/', '/proc/', '/sys/',
  'c:\\', 'c:/',
  '\\\\', '//',
  '%2e%2e', '%252e',
  '....//', '....\\'
];

export function isValidAssetAction(action: string): boolean {
  return VALID_ASSET_ACTIONS.has(action);
}

export function validAssetActionMessage(): string {
  return Array.from(VALID_ASSET_ACTIONS).join(', ');
}

function isPathTraversalAttempt(path: string): boolean {
  if (!path || typeof path !== 'string') return false;
  const normalized = path.toLowerCase();
  return TRAVERSAL_PATTERNS.some(pattern => normalized.includes(pattern));
}

export function validatePathSecurity(pathValue: string | undefined, paramName: string): Record<string, unknown> | null {
  if (!pathValue) return null;
  if (!isPathTraversalAttempt(pathValue)) return null;

  return cleanObject({
    success: false,
    error: 'SECURITY_VIOLATION',
    message: `Path traversal attempt detected in ${paramName}. Access denied.`,
    [paramName]: pathValue
  });
}

export function validatePathsSecurity(paths: string[] | undefined, paramName: string): Record<string, unknown> | null {
  if (!paths || !Array.isArray(paths)) return null;
  for (const path of paths) {
    if (isPathTraversalAttempt(path)) {
      return cleanObject({
        success: false,
        error: 'SECURITY_VIOLATION',
        message: `Path traversal attempt detected in ${paramName}. Access denied.`,
        [paramName]: paths
      });
    }
  }
  return null;
}
