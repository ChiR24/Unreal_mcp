import { cleanObject } from '../../utils/safe-json.js';
import { ITools } from '../../types/tool-interfaces.js';
import type { HandlerArgs, AssetArgs } from '../../types/handler-types.js';
import { executeAutomationRequest } from './common-handlers.js';
import { normalizeArgs, extractString, extractOptionalString, extractOptionalNumber, extractOptionalBoolean, extractOptionalArray } from './argument-helper.js';
import { ResponseFactory } from '../../utils/response-factory.js';
import { sanitizePath } from '../../utils/validation.js';

/** Asset info from list response */
interface AssetListItem {
  path?: string;
  package?: string;
  name?: string;
}

/** Response from list/search operations */
interface AssetListResponse {
  success?: boolean;
  assets?: AssetListItem[];
  result?: { assets?: AssetListItem[]; folders?: string[] };
  folders?: string[];
  [key: string]: unknown;
}

/** Response from asset operations */
interface AssetOperationResponse {
  success?: boolean;
  message?: string;
  error?: string;
  tags?: Record<string, unknown>;
  metadata?: Record<string, unknown>;
  [key: string]: unknown;
}

export async function handleAssetTools(action: string, args: HandlerArgs, tools: ITools): Promise<Record<string, unknown>> {
  try {
    switch (action) {
      case 'list': {
        // Route through C++ HandleListAssets for proper asset enumeration
        const params = normalizeArgs(args, [
          { key: 'path', aliases: ['directory', 'assetPath'], default: '/Game' },
          { key: 'limit', default: 50 },
          { key: 'recursive', default: false },
          { key: 'depth', default: undefined }
        ]);

        let path = extractOptionalString(params, 'path') ?? '/Game';
        path = sanitizePath(path);

        const limit = extractOptionalNumber(params, 'limit') ?? 50;
        const recursive = extractOptionalBoolean(params, 'recursive') ?? false;
        const depth = extractOptionalNumber(params, 'depth');

        const effectiveRecursive = recursive === true || (depth !== undefined && depth > 0);

        const res = await executeAutomationRequest(tools, 'list', {
          path,
          recursive: effectiveRecursive,
          depth
        }) as AssetListResponse;

        const assets: AssetListItem[] = (Array.isArray(res.assets) ? res.assets :
          (Array.isArray(res.result) ? res.result : (res.result?.assets || [])));

        // New: Handle folders
        const folders: string[] = Array.isArray(res.folders) ? res.folders : (res.result?.folders || []);

        const totalCount = assets.length;
        const limitedAssets = assets.slice(0, limit);
        const remaining = Math.max(0, totalCount - limit);

        let message = `Found ${totalCount} assets`;
        if (folders.length > 0) {
          message += ` and ${folders.length} folders`;
        }
        message += `: ${limitedAssets.map((a) => a.path || a.package || a.name || 'unknown').join(', ')}`;

        if (folders.length > 0 && limitedAssets.length < limit) {
          const remainingLimit = limit - limitedAssets.length;
          if (remainingLimit > 0) {
            const limitedFolders = folders.slice(0, remainingLimit);
            if (limitedAssets.length > 0) message += ', ';
            message += `Folders: [${limitedFolders.join(', ')}]`;
            if (folders.length > remainingLimit) message += '...';
          }
        }

        if (remaining > 0) {
          message += `... and ${remaining} others`;
        }

        return ResponseFactory.success({
          assets: limitedAssets,
          folders: folders,
          totalCount: totalCount,
          count: limitedAssets.length
        }, message);
      }
      case 'create_folder': {
        const params = normalizeArgs(args, [
          { key: 'path', aliases: ['directoryPath'], required: true }
        ]);
        // Validate path format
        const folderPath = extractString(params, 'path').trim();
        if (!folderPath.startsWith('/')) {
          return ResponseFactory.error('VALIDATION_ERROR', `Invalid folder path: '${folderPath}'. Path must start with '/'`);
        }
        const res = await tools.assetTools.createFolder(folderPath);
        return ResponseFactory.success(res, 'Folder created successfully');
      }
      case 'import': {
        const params = normalizeArgs(args, [
          { key: 'sourcePath', required: true },
          { key: 'destinationPath', required: true },
          { key: 'overwrite', default: false },
          { key: 'save', default: true }
        ]);

        const sourcePath = extractString(params, 'sourcePath');
        const destinationPath = extractString(params, 'destinationPath');
        const overwrite = extractOptionalBoolean(params, 'overwrite') ?? false;
        const save = extractOptionalBoolean(params, 'save') ?? true;

        const res = await tools.assetTools.importAsset({
          sourcePath,
          destinationPath,
          overwrite,
          save
        });
        return ResponseFactory.success(res, 'Asset imported successfully');
      }
      case 'duplicate_asset':
      case 'duplicate': {
        const params = normalizeArgs(args, [
          { key: 'sourcePath', aliases: ['assetPath'], required: true },
          { key: 'destinationPath' },
          { key: 'newName' }
        ]);

        const sourcePath = extractString(params, 'sourcePath');
        let destinationPath = extractOptionalString(params, 'destinationPath');
        const newName = extractOptionalString(params, 'newName');

        if (newName) {
          if (!destinationPath) {
            const lastSlash = sourcePath.lastIndexOf('/');
            const parentDir = lastSlash > 0 ? sourcePath.substring(0, lastSlash) : '/Game';
            destinationPath = `${parentDir}/${newName}`;
          } else if (!destinationPath.endsWith(newName)) {
            if (destinationPath.endsWith('/')) {
              destinationPath = `${destinationPath}${newName}`;
            }
          }
        }

        if (!destinationPath) {
          throw new Error('destinationPath or newName is required for duplicate action');
        }

        const res = await tools.assetTools.duplicateAsset({
          sourcePath,
          destinationPath
        });
        return ResponseFactory.success(res, 'Asset duplicated successfully');
      }
      case 'rename_asset':
      case 'rename': {
        const params = normalizeArgs(args, [
          { key: 'sourcePath', aliases: ['assetPath'], required: true },
          { key: 'destinationPath' },
          { key: 'newName' }
        ]);

        const sourcePath = extractString(params, 'sourcePath');
        let destinationPath = extractOptionalString(params, 'destinationPath');
        const newName = extractOptionalString(params, 'newName');

        if (!destinationPath && newName) {
          const lastSlash = sourcePath.lastIndexOf('/');
          const parentDir = lastSlash > 0 ? sourcePath.substring(0, lastSlash) : '/Game';
          destinationPath = `${parentDir}/${newName}`;
        }

        if (!destinationPath) throw new Error('Missing destinationPath or newName');

        const res = await tools.assetTools.renameAsset({
          sourcePath,
          destinationPath
        }) as AssetOperationResponse;

        if (res && res.success === false) {
          const msg = (res.message || '').toLowerCase();
          if (msg.includes('already exists') || msg.includes('exists')) {
            return cleanObject({
              success: false,
              error: 'ASSET_ALREADY_EXISTS',
              message: res.message || 'Asset already exists at destination',
              sourcePath,
              destinationPath
            });
          }
        }
        return cleanObject(res);
      }
      case 'move_asset':
      case 'move': {
        const params = normalizeArgs(args, [
          { key: 'sourcePath', aliases: ['assetPath'], required: true },
          { key: 'destinationPath' }
        ]);

        const sourcePath = extractString(params, 'sourcePath');
        let destinationPath = extractOptionalString(params, 'destinationPath');
        const assetName = sourcePath.split('/').pop();
        if (assetName && destinationPath && !destinationPath.endsWith(assetName)) {
          destinationPath = `${destinationPath.replace(/\/$/, '')}/${assetName}`;
        }

        const res = await tools.assetTools.moveAsset({
          sourcePath,
          destinationPath: destinationPath ?? ''
        });
        return ResponseFactory.success(res, 'Asset moved successfully');
      }
      case 'delete_assets':
      case 'delete_asset':
      case 'delete': {
        let paths: string[] = [];
        const argsTyped = args as AssetArgs;
        if (Array.isArray(argsTyped.assetPaths) || Array.isArray(argsTyped.asset_paths)) {
          paths = (argsTyped.assetPaths || argsTyped.asset_paths) as string[];
        } else {
          // Support both camelCase and snake_case parameter names
          const single = argsTyped.assetPath || argsTyped.asset_path || argsTyped.path;
          if (typeof single === 'string' && single.trim()) {
            paths = [single.trim()];
          }
        }

        if (paths.length === 0) {
          // Return graceful error response instead of throwing
          // This handles cleanup scenarios where paths may be empty
          return ResponseFactory.error('INVALID_ARGUMENT', 'No paths provided for delete action. Provide assetPath (string) or assetPaths (array).');
        }

        // Normalize paths: strip object sub-path suffix (e.g., /Game/Folder/Asset.Asset -> /Game/Folder/Asset)
        // This handles the common pattern where full object paths are provided instead of package paths
        const normalizedPaths = paths.map(p => {
          let normalized = p.replace(/\\/g, '/').trim();
          // If the path contains a dot after the last slash, it's likely an object path (e.g., /Game/Folder/Asset.Asset)
          const lastSlash = normalized.lastIndexOf('/');
          if (lastSlash >= 0) {
            const afterSlash = normalized.substring(lastSlash + 1);
            const dotIndex = afterSlash.indexOf('.');
            if (dotIndex > 0) {
              // Strip the .ObjectName suffix
              normalized = normalized.substring(0, lastSlash + 1 + dotIndex);
            }
          }
          return normalized;
        });

        const res = await tools.assetTools.deleteAssets({ paths: normalizedPaths });
        return ResponseFactory.success(res, 'Assets deleted successfully');
      }

      case 'generate_lods': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', required: true },
          { key: 'lodCount', required: true }
        ]);
        const assetPath = extractString(params, 'assetPath');
        const lodCount = typeof params.lodCount === 'number' ? params.lodCount : Number(params.lodCount);
        const res = await tools.assetTools.generateLODs({
          assetPath,
          lodCount
        });
        return ResponseFactory.success(res, 'LODs generated successfully');
      }
      case 'create_thumbnail': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', required: true },
          { key: 'width' },
          { key: 'height' }
        ]);
        const assetPath = extractString(params, 'assetPath');
        const width = extractOptionalNumber(params, 'width');
        const height = extractOptionalNumber(params, 'height');
        const res = await tools.assetTools.createThumbnail({
          assetPath,
          width,
          height
        });
        return ResponseFactory.success(res, 'Thumbnail created successfully');
      }
      case 'set_tags': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', required: true },
          { key: 'tags', required: true }
        ]);
        const assetPath = extractString(params, 'assetPath');
        const tags = extractOptionalArray<string>(params, 'tags') ?? [];

        if (!assetPath) {
          return ResponseFactory.error('INVALID_ARGUMENT', 'assetPath is required');
        }

        // Note: Array.isArray check is unnecessary - extractOptionalArray always returns an array

        // Forward to C++ automation bridge which uses UEditorAssetLibrary::SetMetadataTag
        const res = await executeAutomationRequest(tools, 'set_tags', {
          assetPath,
          tags
        });
        return ResponseFactory.success(res, 'Tags set successfully');
      }
      case 'get_metadata': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', required: true }
        ]);
        const assetPath = extractString(params, 'assetPath');
        const res = await tools.assetTools.getMetadata({ assetPath }) as AssetOperationResponse;
        const tags = res.tags || {};
        const metadata = res.metadata || {};
        const merged = { ...tags, ...metadata };
        const tagCount = Object.keys(merged).length;

        const cleanRes = cleanObject(res);
        cleanRes.message = `Metadata retrieved (${tagCount} items)`;
        cleanRes.tags = tags;
        if (Object.keys(metadata).length > 0) {
          cleanRes.metadata = metadata;
        }

        return ResponseFactory.success(cleanRes, cleanRes.message as string);
      }
      case 'set_metadata': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', required: true },
          { key: 'metadata', required: true }
        ]);
        const assetPath = extractString(params, 'assetPath');
        const metadata = params.metadata as Record<string, unknown>;
        const res = await executeAutomationRequest(tools, 'set_metadata', { ...args, assetPath, metadata });
        return ResponseFactory.success(res, 'Metadata set successfully');
      }
      case 'validate':
      case 'validate_asset': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', required: true }
        ]);
        const assetPath = extractString(params, 'assetPath');
        const res = await tools.assetTools.validate({ assetPath });
        return ResponseFactory.success(res, 'Asset validation complete');
      }
      case 'generate_report': {
        const params = normalizeArgs(args, [
          { key: 'directory' },
          { key: 'reportType' },
          { key: 'outputPath' }
        ]);
        const directory = extractOptionalString(params, 'directory') ?? '';
        const reportType = extractOptionalString(params, 'reportType');
        const outputPath = extractOptionalString(params, 'outputPath');
        const res = await tools.assetTools.generateReport({
          directory,
          reportType,
          outputPath
        });
        return ResponseFactory.success(res, 'Report generated successfully');
      }
      case 'create_material_instance': {
        const params = normalizeArgs(args, [
          { key: 'name', required: true },
          { key: 'parentMaterial', required: true },
          { key: 'savePath', aliases: ['path'] }
        ]);
        const name = extractString(params, 'name');
        const parentMaterial = extractString(params, 'parentMaterial');
        const savePath = extractOptionalString(params, 'savePath');
        
        const res = await executeAutomationRequest(
          tools,
          'create_material_instance',
          { ...args, name, parentMaterial, savePath },
          'Automation bridge not available for create_material_instance'
        ) as AssetOperationResponse;

        const result = res ?? {};
        const errorCode = typeof result.error === 'string' ? result.error.toUpperCase() : '';
        const message = typeof result.message === 'string' ? result.message : '';
        const argsTyped = args as AssetArgs;

        if (errorCode === 'PARENT_NOT_FOUND' || message.toLowerCase().includes('parent material not found')) {
          // Keep specific error structure for this business logic case
          return cleanObject({
            success: false,
            error: 'PARENT_NOT_FOUND',
            message: message || 'Parent material not found',
            path: (result as Record<string, unknown>).path,
            parentMaterial: argsTyped.parentMaterial
          });
        }

        return ResponseFactory.success(res, 'Material instance created successfully');
      }
      case 'search_assets': {
        const params = normalizeArgs(args, [
          { key: 'classNames' },
          { key: 'packagePaths' },
          { key: 'recursivePaths' },
          { key: 'recursiveClasses' },
          { key: 'limit' }
        ]);
        const classNames = extractOptionalArray<string>(params, 'classNames');
        const packagePaths = extractOptionalArray<string>(params, 'packagePaths');
        const recursivePaths = extractOptionalBoolean(params, 'recursivePaths');
        const recursiveClasses = extractOptionalBoolean(params, 'recursiveClasses');
        const limit = extractOptionalNumber(params, 'limit');
        const res = await tools.assetTools.searchAssets({
          classNames,
          packagePaths,
          recursivePaths,
          recursiveClasses,
          limit
        });
        return ResponseFactory.success(res, 'Assets found');
      }
      case 'find_by_tag': {
        const params = normalizeArgs(args, [
          { key: 'tag', required: true },
          { key: 'value' }
        ]);
        const tag = extractString(params, 'tag');
        const value = extractOptionalString(params, 'value');
        const res = await tools.assetTools.findByTag({ tag, value });
        return ResponseFactory.success(res, 'Assets found by tag');
      }
      case 'get_dependencies': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', required: true },
          { key: 'recursive' }
        ]);
        const assetPath = extractString(params, 'assetPath');
        const recursive = extractOptionalBoolean(params, 'recursive');
        const res = await tools.assetTools.getDependencies({ assetPath, recursive });
        return ResponseFactory.success(res, 'Dependencies retrieved');
      }
      case 'get_source_control_state': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', required: true }
        ]);
        const assetPath = extractString(params, 'assetPath');
        const res = await tools.assetTools.getSourceControlState({ assetPath });
        return ResponseFactory.success(res, 'Source control state retrieved');
      }
      case 'analyze_graph': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', required: true },
          { key: 'maxDepth' }
        ]);
        const assetPath = extractString(params, 'assetPath');
        const maxDepth = extractOptionalNumber(params, 'maxDepth');
        const res = await executeAutomationRequest(tools, 'get_asset_graph', {
          assetPath,
          maxDepth
        });
        return ResponseFactory.success(res, 'Graph analysis complete');
      }
      case 'create_render_target': {
        const params = normalizeArgs(args, [
          { key: 'name', required: true },
          { key: 'packagePath', aliases: ['path'], default: '/Game' },
          { key: 'width' },
          { key: 'height' },
          { key: 'format' }
        ]);
        const name = extractString(params, 'name');
        const packagePath = extractOptionalString(params, 'packagePath') ?? '/Game';
        const width = extractOptionalNumber(params, 'width');
        const height = extractOptionalNumber(params, 'height');
        const format = extractOptionalString(params, 'format');
        const res = await executeAutomationRequest(tools, 'manage_render', {
          subAction: 'create_render_target',
          name,
          packagePath,
          width,
          height,
          format,
          save: true
        });
        return ResponseFactory.success(res, 'Render target created successfully');
      }
      case 'nanite_rebuild_mesh': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', aliases: ['meshPath'], required: true }
        ]);
        const assetPath = extractString(params, 'assetPath');
        const res = await executeAutomationRequest(tools, 'manage_render', {
          subAction: 'nanite_rebuild_mesh',
          assetPath
        });
        return ResponseFactory.success(res, 'Nanite mesh rebuilt successfully');
      }
      case 'fixup_redirectors': {
        const argsTyped = args as AssetArgs;
        const directoryRaw = typeof argsTyped.directory === 'string' && argsTyped.directory.trim().length > 0
          ? argsTyped.directory.trim()
          : (typeof argsTyped.directoryPath === 'string' && argsTyped.directoryPath.trim().length > 0
            ? argsTyped.directoryPath.trim()
            : '');

        // Pass all args through to C++ handler, with normalized directoryPath
        const payload: Record<string, unknown> = { ...args };
        if (directoryRaw) {
          payload.directoryPath = directoryRaw;
        }

        const res = await executeAutomationRequest(tools, 'fixup_redirectors', payload);
        return ResponseFactory.success(res, 'Redirectors fixed up successfully');
      }
      case 'add_material_parameter': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', required: true },
          { key: 'parameterName', aliases: ['name'], required: true },
          { key: 'parameterType', aliases: ['type'] },
          { key: 'value', aliases: ['defaultValue'] }
        ]);
        const assetPath = extractString(params, 'assetPath');
        const parameterName = extractString(params, 'parameterName');
        const parameterType = extractOptionalString(params, 'parameterType');
        const value = params.value;
        const res = await executeAutomationRequest(tools, 'add_material_parameter', {
          assetPath,
          name: parameterName,
          type: parameterType,
          value
        });
        return ResponseFactory.success(res, 'Material parameter added successfully');
      }
      case 'list_instances': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', required: true }
        ]);
        const assetPath = extractString(params, 'assetPath');
        const res = await executeAutomationRequest(tools, 'list_instances', {
          assetPath
        });
        return ResponseFactory.success(res, 'Instances listed successfully');
      }
      case 'reset_instance_parameters': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', required: true }
        ]);
        const assetPath = extractString(params, 'assetPath');
        const res = await executeAutomationRequest(tools, 'reset_instance_parameters', {
          assetPath
        });
        return ResponseFactory.success(res, 'Instance parameters reset successfully');
      }
      case 'exists': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', required: true }
        ]);
        const assetPath = extractString(params, 'assetPath');
        const res = await executeAutomationRequest(tools, 'exists', {
          assetPath
        });
        return ResponseFactory.success(res, 'Asset existence check complete');
      }
      case 'get_material_stats': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', required: true }
        ]);
        const assetPath = extractString(params, 'assetPath');
        const res = await executeAutomationRequest(tools, 'get_material_stats', {
          assetPath
        });
        return ResponseFactory.success(res, 'Material stats retrieved');
      }
      case 'add_material_node': {
        const materialNodeAliases: Record<string, string> = {
          'Multiply': 'MaterialExpressionMultiply',
          'Add': 'MaterialExpressionAdd',
          'Subtract': 'MaterialExpressionSubtract',
          'Divide': 'MaterialExpressionDivide',
          'Power': 'MaterialExpressionPower',
          'Clamp': 'MaterialExpressionClamp',
          'Constant': 'MaterialExpressionConstant',
          'Constant2Vector': 'MaterialExpressionConstant2Vector',
          'Constant3Vector': 'MaterialExpressionConstant3Vector',
          'Constant4Vector': 'MaterialExpressionConstant4Vector',
          'TextureSample': 'MaterialExpressionTextureSample',
          'TextureCoordinate': 'MaterialExpressionTextureCoordinate',
          'Panner': 'MaterialExpressionPanner',
          'Rotator': 'MaterialExpressionRotator',
          'Lerp': 'MaterialExpressionLinearInterpolate',
          'LinearInterpolate': 'MaterialExpressionLinearInterpolate',
          'Sine': 'MaterialExpressionSine',
          'Cosine': 'MaterialExpressionCosine',
          'Append': 'MaterialExpressionAppendVector',
          'AppendVector': 'MaterialExpressionAppendVector',
          'ComponentMask': 'MaterialExpressionComponentMask',
          'Fresnel': 'MaterialExpressionFresnel',
          'Time': 'MaterialExpressionTime',
          'ScalarParameter': 'MaterialExpressionScalarParameter',
          'VectorParameter': 'MaterialExpressionVectorParameter',
          'StaticSwitchParameter': 'MaterialExpressionStaticSwitchParameter'
        };

        const params = normalizeArgs(args, [
          { key: 'assetPath', aliases: ['materialPath'], required: true },
          { key: 'nodeType', aliases: ['type'], required: true, map: materialNodeAliases },
          { key: 'posX' },
          { key: 'posY' }
        ]);

        const assetPath = extractString(params, 'assetPath');
        const nodeType = extractString(params, 'nodeType');
        const posX = extractOptionalNumber(params, 'posX');
        const posY = extractOptionalNumber(params, 'posY');

        const res = await executeAutomationRequest(tools, 'add_material_node', {
          assetPath,
          nodeType,
          posX,
          posY
        });
        return ResponseFactory.success(res, 'Material node added successfully');
      }
      case 'connect_material_pins': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', aliases: ['materialPath'], required: true },
          { key: 'sourceNodeId', aliases: ['sourceNode'], required: true },
          { key: 'sourcePin', aliases: ['fromPin', 'outputPin'], required: true },
          { key: 'targetNodeId', aliases: ['targetNode'], required: true },
          { key: 'targetPin', aliases: ['toPin', 'inputPin'], required: true }
        ]);
        const assetPath = extractString(params, 'assetPath');
        const sourceNodeId = extractString(params, 'sourceNodeId');
        const sourcePin = extractString(params, 'sourcePin');
        const targetNodeId = extractString(params, 'targetNodeId');
        const targetPin = extractString(params, 'targetPin');
        const res = await executeAutomationRequest(tools, 'connect_material_pins', {
          assetPath,
          sourceNodeId,
          sourcePin,
          targetNodeId,
          targetPin
        });
        return ResponseFactory.success(res, 'Material pins connected successfully');
      }
      case 'remove_material_node': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', aliases: ['materialPath'], required: true },
          { key: 'nodeId', required: true }
        ]);
        const assetPath = extractString(params, 'assetPath');
        const nodeId = extractString(params, 'nodeId');
        const res = await executeAutomationRequest(tools, 'remove_material_node', {
          assetPath,
          nodeId
        });
        return ResponseFactory.success(res, 'Material node removed successfully');
      }
      case 'break_material_connections': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', aliases: ['materialPath'], required: true },
          { key: 'nodeId' },
          { key: 'pinName' }
        ]);
        const assetPath = extractString(params, 'assetPath');
        const nodeId = extractOptionalString(params, 'nodeId');
        const pinName = extractOptionalString(params, 'pinName');
        const res = await executeAutomationRequest(tools, 'break_material_connections', {
          assetPath,
          nodeId,
          pinName
        });
        return ResponseFactory.success(res, 'Material connections broken successfully');
      }
      case 'get_material_node_details': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', aliases: ['materialPath'], required: true },
          { key: 'nodeId', required: true }
        ]);
        const assetPath = extractString(params, 'assetPath');
        const nodeId = extractString(params, 'nodeId');
        const res = await executeAutomationRequest(tools, 'get_material_node_details', {
          assetPath,
          nodeId
        });
        return ResponseFactory.success(res, 'Material node details retrieved');
      }
      case 'rebuild_material': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', aliases: ['materialPath'], required: true }
        ]);
        const assetPath = extractString(params, 'assetPath');
        const res = await executeAutomationRequest(tools, 'rebuild_material', {
          assetPath
        });
        return ResponseFactory.success(res, 'Material rebuilt successfully');
      }
      case 'bulk_rename': {
        // Accept either folderPath or assetPaths
        const argsTyped = args as AssetArgs;
        const folderPath = argsTyped.folderPath ?? argsTyped.path;
        const assetPaths = argsTyped.assetPaths ?? argsTyped.paths;
        
        if (!folderPath && (!assetPaths || (Array.isArray(assetPaths) && assetPaths.length === 0))) {
          return ResponseFactory.error('INVALID_ARGUMENT', 'Either folderPath or assetPaths is required for bulk_rename');
        }
        
        const res = await executeAutomationRequest(tools, 'bulk_rename', {
          ...args,
          folderPath,
          assetPaths
        });
        return ResponseFactory.success(res, 'Bulk rename completed');
      }
      case 'bulk_delete': {
        // Accept either folderPath or assetPaths
        const argsTyped = args as AssetArgs;
        const folderPath = argsTyped.folderPath ?? argsTyped.path;
        const assetPaths = argsTyped.assetPaths ?? argsTyped.paths;
        
        if (!folderPath && (!assetPaths || (Array.isArray(assetPaths) && assetPaths.length === 0))) {
          return ResponseFactory.error('INVALID_ARGUMENT', 'Either folderPath or assetPaths is required for bulk_delete');
        }
        
        const res = await executeAutomationRequest(tools, 'bulk_delete', {
          ...args,
          folderPath,
          assetPaths
        });
        return ResponseFactory.success(res, 'Bulk delete completed');
      }
      default: {
        // Pass all args through to C++ handler for unhandled actions
        const res = await executeAutomationRequest(tools, action || 'manage_asset', { ...args, subAction: action }) as AssetOperationResponse;
        const result = res ?? {};
        const errorCode = typeof result.error === 'string' ? result.error.toUpperCase() : '';
        const message = typeof result.message === 'string' ? result.message : '';
        const argsTyped = args as AssetArgs;

        // Check for unknown/invalid action errors from C++ (UNKNOWN_ACTION or INVALID_SUBACTION)
        if (errorCode === 'UNKNOWN_ACTION' || errorCode === 'INVALID_SUBACTION' ||
            message.toLowerCase().includes('unknown action') || message.toLowerCase().includes('unknown subaction')) {
          return cleanObject({
            success: false,
            error: 'UNKNOWN_ACTION',
            message: `Unknown asset action: ${action}`,
            action: action || 'manage_asset',
            assetPath: argsTyped.assetPath ?? argsTyped.path
          });
        }

        return ResponseFactory.success(res, 'Asset action executed successfully');
      }
    }
  } catch (error) {
    return ResponseFactory.error(error);
  }
}
