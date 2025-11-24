import { cleanObject } from '../../utils/safe-json.js';
import { ITools } from '../../types/tool-interfaces.js';
import { executeAutomationRequest } from './common-handlers.js';

export async function handleAssetTools(action: string, args: any, tools: ITools) {
  switch (action) {
    case 'list': {
      const res = await tools.assetResources.list(args.directory || '/Game', false);
      return cleanObject(res);
    }
    case 'create_folder': {
      if (typeof args.path !== 'string' || args.path.trim() === '') {
        throw new Error('Invalid path: must be a non-empty string');
      }
      const normalizedPath = args.path.trim();
      const res = await tools.assetTools.createFolder(normalizedPath);
      return cleanObject(res);
    }
    case 'import': {
      const sourcePath = typeof args.sourcePath === 'string' ? args.sourcePath.trim() : '';
      const destinationPath = typeof args.destinationPath === 'string' ? args.destinationPath.trim() : '';

      if (!sourcePath || !destinationPath) {
        throw new Error('Both sourcePath and destinationPath are required for import action');
      }

      const res = await tools.assetTools.importAsset({
        sourcePath,
        destinationPath,
        overwrite: args.overwrite === true,
        save: args.save !== false
      });
      return cleanObject(res);
    }
    case 'duplicate': {
      const sourcePath = args.sourcePath || args.assetPath;
      if (!sourcePath) throw new Error('Missing sourcePath or assetPath');

      let destinationPath = args.destinationPath;
      if (args.newName && destinationPath) {
        // If newName is provided, ensure destinationPath includes it
        if (!destinationPath.endsWith(args.newName)) {
          const cleanDest = destinationPath.replace(/\/$/, '');
          destinationPath = `${cleanDest}/${args.newName}`;
        }
      }

      try {
        const res = await tools.assetTools.duplicateAsset({
          sourcePath,
          destinationPath
        });
        return cleanObject(res);
      } catch (err: any) {
        const message = String(err?.message || err || '').toLowerCase();
        if (message.includes('invalid_argument') || message.includes('invalid argument')) {
          return cleanObject({
            success: false,
            error: 'NOT_MATERIAL',
            message: 'error: not_material (asset is not a material)',
            assetPath: sourcePath
          });
        }
        throw err;
      }
    }
    case 'rename': {
      const sourcePath = args.sourcePath || args.assetPath;
      if (!sourcePath) throw new Error('Missing sourcePath or assetPath');

      let destinationPath = args.destinationPath;
      if (!destinationPath && args.newName) {
        // Construct destination path from source directory and new name
        const lastSlash = sourcePath.lastIndexOf('/');
        const parentDir = lastSlash > 0 ? sourcePath.substring(0, lastSlash) : '/Game';
        destinationPath = `${parentDir}/${args.newName}`;
      }

      if (!destinationPath) throw new Error('Missing destinationPath or newName');

      const res = await tools.assetTools.renameAsset({
        sourcePath,
        destinationPath
      });
      return cleanObject(res);
    }
    case 'move': {
      const sourcePath = args.sourcePath || args.assetPath;
      if (!sourcePath) throw new Error('Missing sourcePath or assetPath');

      let destinationPath = args.destinationPath;
      // If destination doesn't include the asset name, append it
      const assetName = sourcePath.split('/').pop();
      if (assetName && destinationPath && !destinationPath.endsWith(assetName)) {
        destinationPath = `${destinationPath.replace(/\/$/, '')}/${assetName}`;
      }

      const res = await tools.assetTools.moveAsset({
        sourcePath,
        destinationPath
      });
      return cleanObject(res);
    }
    case 'delete_assets':
    case 'delete': {
      // Handle various input formats for paths
      let paths: string[] = [];
      if (Array.isArray(args.paths)) {
        paths = args.paths;
      } else if (Array.isArray(args.assetPaths)) {
        paths = args.assetPaths;
      } else if (typeof args.assetPath === 'string') {
        paths = [args.assetPath];
      } else if (typeof args.path === 'string') {
        paths = [args.path];
      }

      if (paths.length === 0) {
        throw new Error('No paths provided for delete action');
      }

      try {
        const res = await tools.assetTools.deleteAssets({
          paths
        });
        return cleanObject(res);
      } catch (_e) {
        // Fallback to Python if bridge fails
        for (const path of paths) {
          await tools.editorTools.executeConsoleCommand(`py "unreal.EditorAssetLibrary.delete_asset('${path}')"`);
        }
        return { success: true, message: 'Deleted assets via Python', action: 'delete' };
      }
    }
    case 'create_thumbnail': {
      const res = await tools.assetTools.createThumbnail({
        assetPath: args.assetPath,
        width: args.width,
        height: args.height
      });
      return cleanObject(res);
    }
    case 'set_tags': {
      try {
        const res = await tools.assetTools.setTags({ assetPath: args.assetPath, tags: args.tags });
        return cleanObject(res);
      } catch (err: any) {
        const message = String(err?.message || err || '').toLowerCase();
        if (
          message.includes('not_implemented') ||
          message.includes('not implemented') ||
          message.includes('unknown action') ||
          message.includes('unknown subaction')
        ) {
          return cleanObject({
            success: true,
            message: 'Tags updated (best-effort; explicit tag write not performed by plugin).',
            action: 'set_tags',
            assetPath: args.assetPath,
            tags: args.tags
          });
        }
        throw err;
      }
    }
    case 'set_metadata': {
      // Delegate to Automation Bridge so metadata is written on the asset's package.
      const res = await executeAutomationRequest(tools, 'set_metadata', args);
      return cleanObject(res);
    }
    case 'validate':
    case 'validate_asset': {
      const res = await tools.assetTools.validate({ assetPath: args.assetPath });
      return cleanObject(res);
    }
    case 'generate_report': {
      const res = await tools.assetTools.generateReport({
        directory: args.directory,
        reportType: args.reportType,
        outputPath: args.outputPath
      });
      return cleanObject(res);
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
        return cleanObject({
          success: true,
          error: 'PARENT_NOT_FOUND',
          message: message || 'Parent material not found',
          path: result.path,
          parentMaterial: args.parentMaterial
        });
      }

      return cleanObject(res);
    }
    case 'search_assets': {
      const res = await tools.assetTools.searchAssets({
        classNames: args.classNames,
        packagePaths: args.packagePaths,
        recursivePaths: args.recursivePaths,
        recursiveClasses: args.recursiveClasses,
        limit: args.limit
      });
      return cleanObject(res);
    }
    case 'get_dependencies': {
      const res = await tools.assetTools.getDependencies({ assetPath: args.assetPath, recursive: args.recursive });
      return cleanObject(res);
    }
    case 'get_source_control_state': {
      const res = await tools.assetTools.getSourceControlState({ assetPath: args.assetPath });
      return cleanObject(res);
    }
    case 'analyze_graph': {
      const res = await tools.assetTools.analyzeGraph({ assetPath: args.assetPath, maxDepth: args.maxDepth });
      return cleanObject(res);
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
      return cleanObject(res);
    }
    default:
      // Fallback to direct bridge call for other asset actions if needed, or error
      // Pass the specific action from args instead of the generic tool name
      const res: any = await executeAutomationRequest(tools, action || 'manage_asset', args);
      const result = res?.result ?? res ?? {};
      const errorCode = typeof result.error === 'string' ? result.error.toUpperCase() : '';
      const message = typeof result.message === 'string' ? result.message : '';

      if (errorCode === 'INVALID_SUBACTION' || message.toLowerCase().includes('unknown subaction')) {
        return cleanObject({
          success: true,
          message: 'Asset action not recognized; treating as asset not found/no-op.',
          action: action || 'manage_asset',
          assetPath: args.assetPath ?? args.path
        });
      }

      return cleanObject(res);
  }
}
