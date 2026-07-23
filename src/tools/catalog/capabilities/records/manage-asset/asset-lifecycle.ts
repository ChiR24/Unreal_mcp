// Asset lifecycle records: list, import, duplicate/rename/move/delete alias
// families, create_folder, search_assets. List and search model bounded
// continuation with max page size 500 and opaque cursor contract.

import type { RecordSpec } from './builder.js';
import { aliasCanonical, aliasOf, arr, arrObj, bool, boundedLimit, boundedPagination, DESTRUCTIVE, DESTRUCTIVE_POLICY, ex, HIGH, LOW, MEDIUM, NON_IDEMPOTENT, num, READ, READ_POLICY, r, schema, str, WRITE, WRITE_POLICY } from './builder.js';

const SOURCE_PATH = str('Source filesystem path for import.');
const DEST_PATH = str('Destination /Game asset path.');

const PAGINATED_OUTPUT = schema({
  success: bool('Operation succeeded.'),
  assets: arrObj('Matched assets on this page.'),
  folders: arr('Subfolder paths.'),
  totalCount: num('Total matched count before pagination.'),
  count: num('Asset count on this page.'),
  limit: num('Applied page size.'),
  offset: num('Applied zero-based offset.'),
  hasMore: bool('True when more results exist beyond the current page.'),
  nextOffset: num('Next-page offset.'),
  // handleListAssets resolves both to `... : (cursor ?? null)` / `... : null`, so
  // null is a produced value on the last page, not merely an absent field.
  cursor: { type: ['string', 'null'], description: 'Opaque cursor for the current page, or null when the page was requested without one.' },
  nextCursor: { type: ['string', 'null'], description: 'Opaque cursor for the next page, or null on the last page.' }
}, ['success']);

const OK_OUTPUT = schema({ success: bool('Operation succeeded.'), details: { type: 'object', 'x-unreal-reflection-boundary': true, description: 'Operation details.' } }, ['success']);

export const ASSET_LIFECYCLE_RECORDS: readonly RecordSpec[] = [
  r('list', 'asset', 'List assets under a /Game path with bounded pagination and opaque cursor continuation.',
    schema({
      path: { type: 'string', default: '/Game', description: 'Canonical /Game path to list.' },
      limit: boundedLimit(500, 50),
      offset: num('Zero-based offset into the full result set.'),
      pagination: boundedPagination(500, 50),
      cursor: str('Opaque pagination cursor returned by a previous list response. Forward verbatim to resume.'),
      recursive: bool('Recurse into subdirectories.'),
      depth: num('Maximum recursion depth.'),
      includeTags: bool('When true, include asset tags in the listing response.')
    }, ['path']),
    PAGINATED_OUTPUT, READ, READ_POLICY, MEDIUM,
    { dispatchAction: 'list', dispatchMode: 'action', examples: [ex('List first page', { path: '/Game' }, { success: true, assets: [], hasMore: false, nextCursor: null })] }
  ),

  r('import', 'asset', 'Import an asset from a filesystem source into the project content hierarchy.',
    schema({ sourcePath: SOURCE_PATH, destinationPath: DEST_PATH, overwrite: bool('Overwrite existing asset at destination.'), save: bool('Save package after import.') }, ['sourcePath', 'destinationPath']),
    OK_OUTPUT, WRITE, WRITE_POLICY, MEDIUM,
    { examples: [ex('Import FBX', { sourcePath: '/tmp/mesh.fbx', destinationPath: '/Game/Imports/Mesh' }, { success: true })] }
  ),

  r('duplicate', 'asset', 'Duplicate an existing asset to a new path.',
    schema({ sourcePath: str('Source /Game asset path.'), destinationPath: DEST_PATH, newName: str('New asset name.') }, ['sourcePath']),
    OK_OUTPUT, WRITE, WRITE_POLICY, MEDIUM,
    { normalization: aliasCanonical('duplicate_asset'),
      examples: [ex('Duplicate a material', { sourcePath: '/Game/Materials/M_Base', destinationPath: '/Game/Materials', newName: 'M_Base_Variant' }, { success: true })] }
  ),
  r('duplicate_asset', 'asset', 'Long-form alias for duplicate.',
    schema({ sourcePath: str('Source /Game asset path.'), destinationPath: DEST_PATH, newName: str('New asset name.') }, ['sourcePath']),
    OK_OUTPUT, WRITE, WRITE_POLICY, MEDIUM,
    { normalization: aliasOf('asset.duplicate'),
      examples: [ex('Duplicate via the long-form alias', { sourcePath: '/Game/Materials/M_Base', destinationPath: '/Game/Materials', newName: 'M_Base_Variant' }, { success: true })] }
  ),

  r('rename', 'asset', 'Rename an existing asset in place.',
    schema({ sourcePath: str('Source /Game asset path.'), destinationPath: DEST_PATH, newName: str('New asset name.') }, ['sourcePath']),
    OK_OUTPUT, NON_IDEMPOTENT, WRITE_POLICY, MEDIUM,
    { normalization: aliasCanonical('rename_asset'),
      examples: [ex('Rename a mesh in place', { sourcePath: '/Game/Meshes/SM_Crate', newName: 'SM_Crate_Large' }, { success: true })] }
  ),
  r('rename_asset', 'asset', 'Long-form alias for rename.',
    schema({ sourcePath: str('Source /Game asset path.'), destinationPath: DEST_PATH, newName: str('New asset name.') }, ['sourcePath']),
    OK_OUTPUT, NON_IDEMPOTENT, WRITE_POLICY, MEDIUM,
    { normalization: aliasOf('asset.rename'),
      examples: [ex('Rename via the long-form alias', { sourcePath: '/Game/Meshes/SM_Crate', newName: 'SM_Crate_Large' }, { success: true })] }
  ),

  r('move', 'asset', 'Move an asset to a new package path.',
    schema({ sourcePath: str('Source /Game asset path.'), destinationPath: DEST_PATH }, ['sourcePath']),
    OK_OUTPUT, NON_IDEMPOTENT, WRITE_POLICY, MEDIUM,
    { normalization: aliasCanonical('move_asset'),
      examples: [ex('Move a texture into a subfolder', { sourcePath: '/Game/Textures/T_Rock', destinationPath: '/Game/Textures/Terrain/T_Rock' }, { success: true })] }
  ),
  r('move_asset', 'asset', 'Long-form alias for move.',
    schema({ sourcePath: str('Source /Game asset path.'), destinationPath: DEST_PATH }, ['sourcePath']),
    OK_OUTPUT, NON_IDEMPOTENT, WRITE_POLICY, MEDIUM,
    { normalization: aliasOf('asset.move'),
      examples: [ex('Move via the long-form alias', { sourcePath: '/Game/Textures/T_Rock', destinationPath: '/Game/Textures/Terrain/T_Rock' }, { success: true })] }
  ),

  r('delete', 'asset', 'Permanently delete one or more assets after explicit confirmation.',
    schema({ paths: arr('Asset paths to delete.'), assetPath: str('Single asset path (alternative to paths).'), force: bool('Force deletion even when the asset is still referenced (bridge delete path).') }, []),
    OK_OUTPUT, DESTRUCTIVE, DESTRUCTIVE_POLICY, HIGH,
    { normalization: aliasCanonical('delete_asset/delete_assets'),
      examples: [ex('Delete one asset', { paths: ['/Game/MCPTest/Disposable'] }, { success: true })] }
  ),
  r('delete_asset', 'asset', 'Long-form alias for delete.',
    schema({ paths: arr('Asset paths to delete.'), assetPath: str('Single asset path.'), force: bool('Force deletion even when the asset is still referenced (bridge delete path).') }, []),
    OK_OUTPUT, DESTRUCTIVE, DESTRUCTIVE_POLICY, HIGH,
    { normalization: aliasOf('asset.delete'),
      examples: [ex('Delete a single asset by path', { assetPath: '/Game/MCPTest/Disposable' }, { success: true })] }
  ),
  r('delete_assets', 'asset', 'Plural-form alias for delete.',
    schema({ paths: arr('Asset paths to delete.'), assetPath: str('Single asset path.'), force: bool('Force deletion even when the asset is still referenced (bridge delete path).') }, []),
    OK_OUTPUT, DESTRUCTIVE, DESTRUCTIVE_POLICY, HIGH,
    { normalization: aliasOf('asset.delete'),
      examples: [ex('Delete several assets in one call', { paths: ['/Game/MCPTest/DisposableA', '/Game/MCPTest/DisposableB'] }, { success: true })] }
  ),

  r('create_folder', 'asset', 'Create a new content-browser folder under a /Game path.',
    schema({ path: str('Folder path (must start with /).') }, ['path']),
    OK_OUTPUT, WRITE, WRITE_POLICY, LOW,
    { examples: [ex('Create folder', { path: '/Game/NewFolder' }, { success: true })] }
  ),

  r('search_assets', 'asset', 'Search assets by text, class, or package path with bounded pagination.',
    schema({
      searchText: str('Text to search for.'),
      classNames: arr('Asset class names to filter.'),
      packagePaths: arr('Package paths to search within.'),
      recursivePaths: bool('Recurse into subdirectories.'),
      recursiveClasses: bool('Recurse into child classes.'),
      limit: boundedLimit(500, 50),
      offset: num('Zero-based offset into the full result set.')
    }, []),
    PAGINATED_OUTPUT, READ, READ_POLICY, MEDIUM,
    { dispatchAction: 'asset_query', dispatchMode: 'action',
      examples: [ex('Search materials by name',
        { searchText: 'M_Rock', classNames: ['Material'], packagePaths: ['/Game/Materials'], recursivePaths: true, limit: 25 },
        { success: true, assets: [{ name: 'M_Rock', path: '/Game/Materials/M_Rock.M_Rock', class: 'Material', packagePath: '/Game/Materials' }], folders: [], totalCount: 1, count: 1, limit: 25, offset: 0, hasMore: false, nextOffset: 1, cursor: null, nextCursor: null })] }
  )
];
