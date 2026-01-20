import { cleanObject } from '../../utils/safe-json.js';
import { ITools } from '../../types/tool-interfaces.js';
import type { HandlerArgs, HandlerResult, AssetArgs } from '../../types/handler-types.js';
import { executeAutomationRequest } from './common-handlers.js';
import { normalizeArgs, extractString, extractOptionalString, extractOptionalNumber, extractOptionalBoolean, extractOptionalArray } from './argument-helper.js';
import { ResponseFactory } from '../../utils/response-factory.js';

/** Asset info from list response */
interface AssetListItem {
  path?: string;
  package?: string;
  name?: string;
  isFolder?: boolean;
  Class?: string;
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

export async function handleAssetTools(action: string, args: HandlerArgs, tools: ITools): Promise<HandlerResult> {
  try {
    switch (action) {
      case 'list': {
        // Route through AssetResources for proper caching
        const params = normalizeArgs(args, [
          { key: 'path', aliases: ['directory', 'assetPath'], default: '/Game' },
          { key: 'limit', default: 50 },
          { key: 'recursive', default: false },
          { key: 'depth', default: undefined },
          { key: 'refresh', default: false }
        ]);

        const path = extractOptionalString(params, 'path') ?? '/Game';
        const limit = extractOptionalNumber(params, 'limit') ?? 50;
        const recursive = extractOptionalBoolean(params, 'recursive') ?? false;
        const depth = extractOptionalNumber(params, 'depth');
        const refresh = extractOptionalBoolean(params, 'refresh');

        const effectiveRecursive = recursive === true || (depth !== undefined && depth > 0);

        const res = await tools.assetResources.list(path, effectiveRecursive, limit, { refresh, depth });

        // Check if the underlying operation failed (e.g., NOT_IMPLEMENTED from C++)
        const resObj = res as Record<string, unknown>;
        if (resObj.success === false) {
          const errorMsg = String(resObj.error || resObj.message || 'Asset listing failed');
          return ResponseFactory.error('NOT_IMPLEMENTED', errorMsg);
        }

        const assets: AssetListItem[] = (Array.isArray(res.assets) ? res.assets : []) as AssetListItem[];

        const totalCount = typeof res.count === 'number' ? res.count : assets.length;
        const folderCount = typeof res.folders === 'number' ? res.folders : 0;
        const fileCount = typeof res.files === 'number' ? res.files : (totalCount - folderCount);
        
        const limitedAssets = assets.slice(0, limit);
        const remaining = Math.max(0, totalCount - limit);

        let message = `Found ${fileCount} assets`;
        if (folderCount > 0) {
          message += ` and ${folderCount} folders`;
        }
        
        const names = limitedAssets.map((a) => {
             const isFolder = a.isFolder || a.package === 'Folder' || a.name?.endsWith('/');
             const name = a.path || a.package || a.name || 'unknown';
             return isFolder ? `[${name}]` : name;
        }).join(', ');
        
        message += `: ${names}`;

        if (remaining > 0) {
          message += `... and ${remaining} others`;
        }

        return ResponseFactory.success({
          assets: limitedAssets,
          folders: folderCount, // Return count instead of list
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
        if (Array.isArray(argsTyped.assetPaths)) {
          paths = argsTyped.assetPaths as string[];
        } else {
          const single = argsTyped.assetPath || argsTyped.path;
          if (typeof single === 'string' && single.trim()) {
            paths = [single.trim()];
          }
        }

        if (paths.length === 0) {
          throw new Error('No paths provided for delete action');
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
        const res = await executeAutomationRequest(tools, 'set_metadata', args);
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
        const res = await executeAutomationRequest(
          tools,
          'create_material_instance',
          args,
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
        const res = await executeAutomationRequest(tools, 'manage_asset', {
          subAction: 'analyze_graph',
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
      case 'enable_nanite_mesh': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', aliases: ['meshPath'], required: true },
          { key: 'enableNanite', default: true }
        ]);
        const assetPath = extractString(params, 'assetPath');
        const enableNanite = extractOptionalBoolean(params, 'enableNanite') ?? true;
        
        // Note: Using 'manage_asset' tool for C++ dispatch as these are asset workflow actions
        const res = await executeAutomationRequest(tools, 'manage_asset', {
          subAction: 'enable_nanite_mesh',
          assetPath,
          enableNanite
        });
        return ResponseFactory.success(res, `Nanite ${enableNanite ? 'enabled' : 'disabled'} successfully`);
      }
      case 'set_nanite_settings': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', required: true },
          { key: 'nanitePositionPrecision' },
          { key: 'nanitePercentTriangles' },
          { key: 'naniteFallbackRelativeError' }
        ]);
        const assetPath = extractString(params, 'assetPath');
        const positionPrecision = extractOptionalNumber(params, 'nanitePositionPrecision');
        const percentTriangles = extractOptionalNumber(params, 'nanitePercentTriangles');
        const fallbackRelativeError = extractOptionalNumber(params, 'naniteFallbackRelativeError');

        const res = await executeAutomationRequest(tools, 'manage_asset', {
          subAction: 'set_nanite_settings',
          assetPath,
          positionPrecision,
          percentTriangles,
          fallbackRelativeError
        });
        return ResponseFactory.success(res, 'Nanite settings configured successfully');
      }
      case 'batch_nanite_convert': {
        const params = normalizeArgs(args, [
          { key: 'directory', aliases: ['path'], required: true },
          { key: 'recursive', default: true },
          { key: 'enableNanite', default: true }
        ]);
        const directory = extractString(params, 'directory');
        const recursive = extractOptionalBoolean(params, 'recursive') ?? true;
        const enableNanite = extractOptionalBoolean(params, 'enableNanite') ?? true;

        const res = await executeAutomationRequest(tools, 'manage_asset', {
          subAction: 'batch_nanite_convert',
          directory,
          recursive,
          enableNanite
        });
        return ResponseFactory.success(res, 'Batch Nanite conversion started');
      }
      case 'fixup_redirectors': {
        const argsTyped = args as AssetArgs;
        const directoryRaw = typeof argsTyped.directory === 'string' && argsTyped.directory.trim().length > 0
          ? argsTyped.directory.trim()
          : (typeof argsTyped.directoryPath === 'string' && argsTyped.directoryPath.trim().length > 0
            ? argsTyped.directoryPath.trim()
            : '');

        const payload: Record<string, unknown> = {};
        if (directoryRaw) {
          payload.directoryPath = directoryRaw;
        }
        if (typeof args.checkoutFiles === 'boolean') {
          payload.checkoutFiles = args.checkoutFiles;
        }

        const res = await executeAutomationRequest(tools, 'manage_asset', { ...payload, subAction: 'fixup_redirectors' });
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
        const res = await executeAutomationRequest(tools, 'manage_asset', {
          subAction: 'add_material_parameter',
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
        const res = await executeAutomationRequest(tools, 'manage_asset', {
          subAction: 'list_instances',
          assetPath
        });
        return ResponseFactory.success(res, 'Instances listed successfully');
      }
      case 'reset_instance_parameters': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', required: true }
        ]);
        const assetPath = extractString(params, 'assetPath');
        const res = await executeAutomationRequest(tools, 'manage_asset', {
          subAction: 'reset_instance_parameters',
          assetPath
        });
        return ResponseFactory.success(res, 'Instance parameters reset successfully');
      }
      case 'exists': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', required: true }
        ]);
        const assetPath = extractString(params, 'assetPath');
        // Route through manage_asset with subAction for proper C++ dispatch
        const res = await executeAutomationRequest(tools, 'manage_asset', {
          subAction: 'exists',
          assetPath
        });
        return ResponseFactory.success(res, 'Asset existence check complete');
      }
      case 'get_material_stats': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', required: true }
        ]);
        const assetPath = extractString(params, 'assetPath');
        const res = await executeAutomationRequest(tools, 'manage_asset', {
          subAction: 'get_material_stats',
          assetPath
        });
        return ResponseFactory.success(res, 'Material stats retrieved');
      }
      case 'rebuild_material': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', required: true }
        ]);
        const assetPath = extractString(params, 'assetPath');
        const res = await executeAutomationRequest(tools, 'manage_asset', {
          subAction: 'rebuild_material',
          assetPath
        });
        return ResponseFactory.success(res, 'Material rebuilt successfully');
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
          { key: 'assetPath', required: true },
          { key: 'nodeType', aliases: ['type'], required: true, map: materialNodeAliases },
          { key: 'posX' },
          { key: 'posY' }
        ]);

        const assetPath = extractString(params, 'assetPath');
        const nodeType = extractString(params, 'nodeType');
        const posX = extractOptionalNumber(params, 'posX');
        const posY = extractOptionalNumber(params, 'posY');

        // Route through manage_material_graph with subAction: add_node for proper C++ dispatch
        const res = await executeAutomationRequest(tools, 'manage_material_graph', {
          subAction: 'add_node',
          assetPath,
          nodeType,
          x: posX,
          y: posY
        });
        return ResponseFactory.success(res, 'Material node added successfully');
      }
      // Wave 1.14: Query Enhancement - query assets by predicate
      case 'query_assets_by_predicate': {
        const predicate = args.predicate || {};
        const limit = typeof args.limit === 'number' ? args.limit : 100;
        const res = await executeAutomationRequest(tools, 'manage_asset', {
          subAction: 'query_assets_by_predicate',
          predicate,
          limit
        });
        return ResponseFactory.success(res, 'Assets queried by predicate');
      }
      // Wave 2.1-2.10: Blueprint Enhancement Actions
      case 'bp_implement_interface': {
        const blueprintPath = extractString(normalizeArgs(args, [{ key: 'blueprintPath', aliases: ['assetPath'], required: true }]), 'blueprintPath');
        const interfacePath = extractString(normalizeArgs(args, [{ key: 'interfacePath', required: true }]), 'interfacePath');
        const res = await executeAutomationRequest(tools, 'manage_asset', {
          subAction: 'bp_implement_interface',
          blueprintPath,
          interfacePath
        });
        return ResponseFactory.success(res, 'Interface implemented');
      }
      case 'bp_add_macro': {
        const params = normalizeArgs(args, [
          { key: 'blueprintPath', aliases: ['assetPath'], required: true },
          { key: 'macroName', aliases: ['name'], required: true },
          { key: 'inputs' },
          { key: 'outputs' }
        ]);
        const blueprintPath = extractString(params, 'blueprintPath');
        const macroName = extractString(params, 'macroName');
        const inputs = extractOptionalArray<Record<string, unknown>>(params, 'inputs');
        const outputs = extractOptionalArray<Record<string, unknown>>(params, 'outputs');
        const res = await executeAutomationRequest(tools, 'manage_asset', {
          subAction: 'bp_add_macro',
          blueprintPath,
          macroName,
          inputs,
          outputs
        });
        return ResponseFactory.success(res, 'Macro added');
      }
      case 'bp_create_widget_binding': {
        const params = normalizeArgs(args, [
          { key: 'blueprintPath', aliases: ['assetPath'], required: true },
          { key: 'propertyName', required: true },
          { key: 'bindingType' },
          { key: 'functionName' }
        ]);
        const blueprintPath = extractString(params, 'blueprintPath');
        const propertyName = extractString(params, 'propertyName');
        const bindingType = extractOptionalString(params, 'bindingType');
        const functionName = extractOptionalString(params, 'functionName');
        const res = await executeAutomationRequest(tools, 'manage_asset', {
          subAction: 'bp_create_widget_binding',
          blueprintPath,
          propertyName,
          bindingType,
          functionName
        });
        return ResponseFactory.success(res, 'Widget binding created');
      }
      case 'bp_add_custom_event': {
        const params = normalizeArgs(args, [
          { key: 'blueprintPath', aliases: ['assetPath'], required: true },
          { key: 'eventName', aliases: ['name'], required: true },
          { key: 'parameters' }
        ]);
        const blueprintPath = extractString(params, 'blueprintPath');
        const eventName = extractString(params, 'eventName');
        const parameters = extractOptionalArray<Record<string, unknown>>(params, 'parameters') ?? [];
        const res = await executeAutomationRequest(tools, 'manage_asset', {
          subAction: 'bp_add_custom_event',
          blueprintPath,
          eventName,
          parameters
        });
        return ResponseFactory.success(res, 'Custom event added');
      }
      case 'bp_set_replication_settings': {
        const params = normalizeArgs(args, [
          { key: 'blueprintPath', aliases: ['assetPath'], required: true },
          { key: 'replicates' },
          { key: 'replicateMovement' },
          { key: 'netUpdateFrequency' }
        ]);
        const blueprintPath = extractString(params, 'blueprintPath');
        const replicates = extractOptionalBoolean(params, 'replicates');
        const replicateMovement = extractOptionalBoolean(params, 'replicateMovement');
        const netUpdateFrequency = extractOptionalNumber(params, 'netUpdateFrequency');
        const res = await executeAutomationRequest(tools, 'manage_asset', {
          subAction: 'bp_set_replication_settings',
          blueprintPath,
          replicates,
          replicateMovement,
          netUpdateFrequency
        });
        return ResponseFactory.success(res, 'Replication settings configured');
      }
      case 'bp_add_event_dispatcher': {
        const params = normalizeArgs(args, [
          { key: 'blueprintPath', aliases: ['assetPath'], required: true },
          { key: 'dispatcherName', aliases: ['name'], required: true },
          { key: 'parameters' }
        ]);
        const blueprintPath = extractString(params, 'blueprintPath');
        const dispatcherName = extractString(params, 'dispatcherName');
        const parameters = extractOptionalArray<Record<string, unknown>>(params, 'parameters') ?? [];
        const res = await executeAutomationRequest(tools, 'manage_asset', {
          subAction: 'bp_add_event_dispatcher',
          blueprintPath,
          dispatcherName,
          parameters
        });
        return ResponseFactory.success(res, 'Event dispatcher added');
      }
      case 'bp_bind_event': {
        const params = normalizeArgs(args, [
          { key: 'blueprintPath', aliases: ['assetPath'], required: true },
          { key: 'eventName', required: true },
          { key: 'functionName', required: true }
        ]);
        const blueprintPath = extractString(params, 'blueprintPath');
        const eventName = extractString(params, 'eventName');
        const functionName = extractString(params, 'functionName');
        const res = await executeAutomationRequest(tools, 'manage_asset', {
          subAction: 'bp_bind_event',
          blueprintPath,
          eventName,
          functionName
        });
        return ResponseFactory.success(res, 'Event bound to function');
      }
      case 'get_blueprint_dependencies': {
        const params = normalizeArgs(args, [
          { key: 'blueprintPath', aliases: ['assetPath'], required: true },
          { key: 'recursive' }
        ]);
        const blueprintPath = extractString(params, 'blueprintPath');
        const recursive = extractOptionalBoolean(params, 'recursive') ?? false;
        const res = await executeAutomationRequest(tools, 'manage_asset', {
          subAction: 'get_blueprint_dependencies',
          blueprintPath,
          recursive
        });
        return ResponseFactory.success(res, 'Dependencies retrieved');
      }
      case 'validate_blueprint': {
        const params = normalizeArgs(args, [
          { key: 'blueprintPath', aliases: ['assetPath'], required: true }
        ]);
        const blueprintPath = extractString(params, 'blueprintPath');
        const res = await executeAutomationRequest(tools, 'manage_asset', {
          subAction: 'validate_blueprint',
          blueprintPath
        });
        return ResponseFactory.success(res, 'Blueprint validated');
      }
      case 'compile_blueprint_batch': {
        const params = normalizeArgs(args, [
          { key: 'blueprintPaths', aliases: ['assetPaths'], required: true },
          { key: 'stopOnError' }
        ]);
        const blueprintPaths = extractOptionalArray<string>(params, 'blueprintPaths');
        if (!Array.isArray(blueprintPaths) || blueprintPaths.length === 0) {
          throw new Error('manage_asset.compile_blueprint_batch: blueprintPaths array is required');
        }
        const stopOnError = extractOptionalBoolean(params, 'stopOnError') ?? false;
        const res = await executeAutomationRequest(tools, 'manage_asset', {
          subAction: 'compile_blueprint_batch',
          blueprintPaths,
          stopOnError
        });
        return ResponseFactory.success(res, 'Batch compile completed');
      }
      default: {
        // Route ALL actions through manage_asset with subAction for proper C++ dispatch
        const res = await executeAutomationRequest(tools, 'manage_asset', { ...args, subAction: action }) as AssetOperationResponse;
        const result = res ?? {};
        const errorCode = typeof result.error === 'string' ? result.error.toUpperCase() : '';
        const message = typeof result.message === 'string' ? result.message : '';
        const argsTyped = args as AssetArgs;

        if (errorCode === 'INVALID_SUBACTION' || message.toLowerCase().includes('unknown subaction')) {
          return cleanObject({
            success: false,
            error: 'INVALID_SUBACTION',
            message: 'Asset action not recognized by the automation plugin.',
            action: action || 'manage_asset',
            assetPath: argsTyped.assetPath ?? argsTyped.path
          });
        }

        return ResponseFactory.success(res, 'Asset action executed successfully');
      }
    }
  } catch (error: unknown) {
    return ResponseFactory.error(error);
  }
}
