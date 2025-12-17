import { cleanObject } from '../../utils/safe-json.js';
import { ITools } from '../../types/tool-interfaces.js';
import { executeAutomationRequest } from './common-handlers.js';
import { normalizeArgs } from './argument-helper.js';
import { ResponseFactory } from '../../utils/response-factory.js';

export async function handleAssetTools(action: string, args: any, tools: ITools) {
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

        const recursive = params.recursive === true || (params.depth !== undefined && params.depth > 0);

        const res = await executeAutomationRequest(tools, 'list', {
          path: params.path,
          recursive,
          depth: params.depth
        });


        const response = res as any;
        const assets = (Array.isArray(response.assets) ? response.assets :
          (Array.isArray(response.result) ? response.result : (response.result?.assets || [])));

        // New: Handle folders
        const folders = Array.isArray(response.folders) ? response.folders : (response.result?.folders || []);

        const totalCount = assets.length;
        const limitedAssets = assets.slice(0, params.limit);
        const remaining = Math.max(0, totalCount - params.limit);

        let message = `Found ${totalCount} assets`;
        if (folders.length > 0) {
          message += ` and ${folders.length} folders`;
        }
        message += `: ${limitedAssets.map((a: any) => a.path || a.package || a.name).join(', ')}`;

        if (folders.length > 0 && limitedAssets.length < params.limit) {
          const remainingLimit = params.limit - limitedAssets.length;
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
        const res = await tools.assetTools.createFolder(params.path);
        return ResponseFactory.success(res, 'Folder created successfully');
      }
      case 'import': {
        const params = normalizeArgs(args, [
          { key: 'sourcePath', required: true },
          { key: 'destinationPath', required: true },
          { key: 'overwrite', default: false },
          { key: 'save', default: true }
        ]);

        const res = await tools.assetTools.importAsset({
          sourcePath: params.sourcePath,
          destinationPath: params.destinationPath,
          overwrite: params.overwrite,
          save: params.save
        });
        return ResponseFactory.success(res, 'Asset imported successfully');
      }
      case 'duplicate': {
        const params = normalizeArgs(args, [
          { key: 'sourcePath', aliases: ['assetPath'], required: true },
          { key: 'destinationPath' },
          { key: 'newName' }
        ]);

        let destinationPath = params.destinationPath;
        if (params.newName) {
          if (!destinationPath) {
            const lastSlash = params.sourcePath.lastIndexOf('/');
            const parentDir = lastSlash > 0 ? params.sourcePath.substring(0, lastSlash) : '/Game';
            destinationPath = `${parentDir}/${params.newName}`;
          } else if (!destinationPath.endsWith(params.newName)) {
            if (destinationPath.endsWith('/')) {
              destinationPath = `${destinationPath}${params.newName}`;
            }
          }
        }

        if (!destinationPath) {
          throw new Error('destinationPath or newName is required for duplicate action');
        }

        const res = await tools.assetTools.duplicateAsset({
          sourcePath: params.sourcePath,
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

        let destinationPath = params.destinationPath;
        if (!destinationPath && params.newName) {
          const lastSlash = params.sourcePath.lastIndexOf('/');
          const parentDir = lastSlash > 0 ? params.sourcePath.substring(0, lastSlash) : '/Game';
          destinationPath = `${parentDir}/${params.newName}`;
        }

        if (!destinationPath) throw new Error('Missing destinationPath or newName');

        const res: any = await tools.assetTools.renameAsset({
          sourcePath: params.sourcePath,
          destinationPath
        });

        if (res && res.success === false) {
          const msg = (res.message || '').toLowerCase();
          if (msg.includes('already exists') || msg.includes('exists')) {
            return cleanObject({
              success: false,
              error: 'ASSET_ALREADY_EXISTS',
              message: res.message || 'Asset already exists at destination',
              sourcePath: params.sourcePath,
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

        let destinationPath = params.destinationPath;
        const assetName = params.sourcePath.split('/').pop();
        if (assetName && destinationPath && !destinationPath.endsWith(assetName)) {
          destinationPath = `${destinationPath.replace(/\/$/, '')}/${assetName}`;
        }

        const res = await tools.assetTools.moveAsset({
          sourcePath: params.sourcePath,
          destinationPath
        });
        return ResponseFactory.success(res, 'Asset moved successfully');
      }
      case 'delete_assets':
      case 'delete_asset':
      case 'delete': {
        let paths: string[] = [];
        if (Array.isArray(args.paths)) {
          paths = args.paths;
        } else if (Array.isArray(args.assetPaths)) {
          paths = args.assetPaths;
        } else {
          const single = args.assetPath || args.path;
          if (typeof single === 'string' && single.trim()) {
            paths = [single.trim()];
          }
        }

        if (paths.length === 0) {
          throw new Error('No paths provided for delete action');
        }

        const res = await tools.assetTools.deleteAssets({ paths });
        return ResponseFactory.success(res, 'Assets deleted successfully');
      }
      case 'generate_lods': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', required: true },
          { key: 'lodCount', required: true }
        ]);
        const res = await tools.assetTools.generateLODs({
          assetPath: params.assetPath,
          lodCount: params.lodCount
        });
        return ResponseFactory.success(res, 'LODs generated successfully');
      }
      case 'create_thumbnail': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', required: true },
          { key: 'width' },
          { key: 'height' }
        ]);
        const res = await tools.assetTools.createThumbnail({
          assetPath: params.assetPath,
          width: params.width,
          height: params.height
        });
        return ResponseFactory.success(res, 'Thumbnail created successfully');
      }
      case 'set_tags': {
        try {
          const params = normalizeArgs(args, [
            { key: 'assetPath', required: true },
            { key: 'tags', required: true }
          ]);
          const res = await tools.assetTools.setTags({ assetPath: params.assetPath, tags: params.tags });
          return ResponseFactory.success(res, 'Tags set successfully');
        } catch (err: any) {
          const message = String(err?.message || err || '').toLowerCase();
          if (
            message.includes('not_implemented') ||
            message.includes('not implemented') ||
            message.includes('unknown action') ||
            message.includes('unknown subaction')
          ) {
            return ResponseFactory.error('NOT_IMPLEMENTED', 'Asset tag writes are not implemented by the automation plugin.');
          }
          throw err;
        }
      }
      case 'get_metadata': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', required: true }
        ]);
        const res: any = await tools.assetTools.getMetadata({ assetPath: params.assetPath });
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

        return ResponseFactory.success(cleanRes, cleanRes.message);
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
        const res = await tools.assetTools.validate({ assetPath: params.assetPath });
        return ResponseFactory.success(res, 'Asset validation complete');
      }
      case 'generate_report': {
        const params = normalizeArgs(args, [
          { key: 'directory' },
          { key: 'reportType' },
          { key: 'outputPath' }
        ]);
        const res = await tools.assetTools.generateReport({
          directory: params.directory,
          reportType: params.reportType,
          outputPath: params.outputPath
        });
        return ResponseFactory.success(res, 'Report generated successfully');
      }
      case 'create_material_instance': {
        const res: any = await executeAutomationRequest(
          tools,
          'create_material_instance',
          args,
          'Automation bridge not available for create_material_instance'
        );

        const result = res?.result ?? res ?? {};
        const errorCode = typeof result.error === 'string' ? result.error.toUpperCase() : '';
        const message = typeof result.message === 'string' ? result.message : '';

        if (errorCode === 'PARENT_NOT_FOUND' || message.toLowerCase().includes('parent material not found')) {
          // Keep specific error structure for this business logic case
          return cleanObject({
            success: false,
            error: 'PARENT_NOT_FOUND',
            message: message || 'Parent material not found',
            path: result.path,
            parentMaterial: args.parentMaterial
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
        const res = await tools.assetTools.searchAssets({
          classNames: params.classNames,
          packagePaths: params.packagePaths,
          recursivePaths: params.recursivePaths,
          recursiveClasses: params.recursiveClasses,
          limit: params.limit
        });
        return ResponseFactory.success(res, 'Assets found');
      }
      case 'find_by_tag': {
        const params = normalizeArgs(args, [
          { key: 'tag', required: true },
          { key: 'value' }
        ]);
        const res = await tools.assetTools.findByTag({ tag: params.tag, value: params.value });
        return ResponseFactory.success(res, 'Assets found by tag');
      }
      case 'get_dependencies': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', required: true },
          { key: 'recursive' }
        ]);
        const res = await tools.assetTools.getDependencies({ assetPath: params.assetPath, recursive: params.recursive });
        return ResponseFactory.success(res, 'Dependencies retrieved');
      }
      case 'get_source_control_state': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', required: true }
        ]);
        const res = await tools.assetTools.getSourceControlState({ assetPath: params.assetPath });
        return ResponseFactory.success(res, 'Source control state retrieved');
      }
      case 'analyze_graph': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', required: true },
          { key: 'maxDepth' }
        ]);
        const res = await executeAutomationRequest(tools, 'get_asset_graph', {
          assetPath: params.assetPath,
          maxDepth: params.maxDepth
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
        const res = await executeAutomationRequest(tools, 'manage_render', {
          subAction: 'create_render_target',
          name: params.name,
          packagePath: params.packagePath,
          width: params.width,
          height: params.height,
          format: params.format,
          save: true
        });
        return ResponseFactory.success(res, 'Render target created successfully');
      }
      case 'nanite_rebuild_mesh': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', aliases: ['meshPath'], required: true }
        ]);
        const res = await executeAutomationRequest(tools, 'manage_render', {
          subAction: 'nanite_rebuild_mesh',
          assetPath: params.assetPath
        });
        return ResponseFactory.success(res, 'Nanite mesh rebuilt successfully');
      }
      case 'fixup_redirectors': {
        const directoryRaw = typeof args.directory === 'string' && args.directory.trim().length > 0
          ? args.directory.trim()
          : (typeof args.directoryPath === 'string' && args.directoryPath.trim().length > 0
            ? args.directoryPath.trim()
            : '');

        const payload: any = {};
        if (directoryRaw) {
          payload.directoryPath = directoryRaw;
        }
        if (typeof args.checkoutFiles === 'boolean') {
          payload.checkoutFiles = args.checkoutFiles;
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
        const res = await executeAutomationRequest(tools, 'add_material_parameter', {
          assetPath: params.assetPath,
          name: params.parameterName,
          type: params.parameterType,
          value: params.value
        });
        return ResponseFactory.success(res, 'Material parameter added successfully');
      }
      case 'list_instances': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', required: true }
        ]);
        const res = await executeAutomationRequest(tools, 'list_instances', {
          assetPath: params.assetPath
        });
        return ResponseFactory.success(res, 'Instances listed successfully');
      }
      case 'reset_instance_parameters': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', required: true }
        ]);
        const res = await executeAutomationRequest(tools, 'reset_instance_parameters', {
          assetPath: params.assetPath
        });
        return ResponseFactory.success(res, 'Instance parameters reset successfully');
      }
      case 'exists': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', required: true }
        ]);
        const res = await executeAutomationRequest(tools, 'exists', {
          assetPath: params.assetPath
        });
        return ResponseFactory.success(res, 'Asset existence check complete');
      }
      case 'get_material_stats': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', required: true }
        ]);
        const res = await executeAutomationRequest(tools, 'get_material_stats', {
          assetPath: params.assetPath
        });
        return ResponseFactory.success(res, 'Material stats retrieved');
      }
      case 'rebuild_material': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', required: true }
        ]);
        const res = await executeAutomationRequest(tools, 'rebuild_material', {
          assetPath: params.assetPath
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

        const res = await executeAutomationRequest(tools, 'add_material_node', {
          assetPath: params.assetPath,
          nodeType: params.nodeType,
          posX: params.posX,
          posY: params.posY
        });
        return ResponseFactory.success(res, 'Material node added successfully');
      }
      default:
        const res: any = await executeAutomationRequest(tools, action || 'manage_asset', args);
        const result = res?.result ?? res ?? {};
        const errorCode = typeof result.error === 'string' ? result.error.toUpperCase() : '';
        const message = typeof result.message === 'string' ? result.message : '';

        if (errorCode === 'INVALID_SUBACTION' || message.toLowerCase().includes('unknown subaction')) {
          return cleanObject({
            success: false,
            error: 'INVALID_SUBACTION',
            message: 'Asset action not recognized by the automation plugin.',
            action: action || 'manage_asset',
            assetPath: args.assetPath ?? args.path
          });
        }

        return ResponseFactory.success(res, 'Asset action executed successfully');
    }
  } catch (error) {
    return ResponseFactory.error(error);
  }
}
