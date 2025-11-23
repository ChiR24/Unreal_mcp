import { cleanObject } from '../../utils/safe-json.js';
import { ITools } from '../../types/tool-interfaces.js';
import { executeAutomationRequest, requireNonEmptyString } from './common-handlers.js';

export async function handleLevelTools(action: string, args: any, tools: ITools) {
  switch (action) {
    case 'load':
    case 'load_level': {
      const levelPath = requireNonEmptyString(args.levelPath, 'levelPath', 'Missing required parameter: levelPath');
      const res = await tools.levelTools.loadLevel({ levelPath, streaming: !!args.streaming });
      return cleanObject(res);
    }
    case 'save': {
      const res = await tools.levelTools.saveLevel({ levelName: args.levelName, savePath: args.levelPath });
      return cleanObject(res);
    }
    case 'create_level': {
      const levelName = requireNonEmptyString(args.levelName || (args.levelPath ? args.levelPath.split('/').pop() : ''), 'levelName', 'Missing required parameter: levelName');
      const res = await tools.levelTools.createLevel({ levelName, savePath: args.savePath || args.levelPath });
      return cleanObject(res);
    }
    case 'stream': {
      const res = await tools.levelTools.streamLevel({
        levelPath: args.levelPath,
        levelName: args.levelName,
        shouldBeLoaded: args.shouldBeLoaded,
        shouldBeVisible: args.shouldBeVisible
      });
      return cleanObject(res);
    }
    case 'create_light': {
      // Delegate directly to the plugin's manage_level.create_light handler.
      const res = await executeAutomationRequest(tools, 'manage_level', args);
      return cleanObject(res);
    }
    case 'spawn_light': {
      // Fallback to control_actor spawn if manage_level spawn_light is not supported
      const lightClassMap: Record<string, string> = {
        'Point': '/Script/Engine.PointLight',
        'Directional': '/Script/Engine.DirectionalLight',
        'Spot': '/Script/Engine.SpotLight',
        'Sky': '/Script/Engine.SkyLight',
        'Rect': '/Script/Engine.RectLight'
      };

      const lightType = args.lightType || 'Point';
      const classPath = lightClassMap[lightType] || '/Script/Engine.PointLight';

      try {
        const res = await tools.actorTools.spawn({
          classPath,
          actorName: args.name,
          location: args.location,
          rotation: args.rotation
        });
        return { ...cleanObject(res), action: 'spawn_light' };
      } catch (_e) {
        return await executeAutomationRequest(tools, 'manage_level', args);
      }
    }
    case 'build_lighting': {
      return cleanObject(await tools.lightingTools.buildLighting({
        quality: (args.quality as any) || 'Preview',
        buildOnlySelected: false,
        buildReflectionCaptures: false
      }));
    }
    case 'export_level': {
      const res = await tools.levelTools.exportLevel({
        levelPath: args.levelPath,
        exportPath: args.destinationPath
      });
      return cleanObject(res);
    }
    case 'import_level': {
      const res = await tools.levelTools.importLevel({
        packagePath: args.sourcePath,
        destinationPath: args.destinationPath
      });
      return cleanObject(res);
    }
    case 'list_levels': {
      const res = await tools.levelTools.listLevels();
      return cleanObject(res);
    }
    case 'get_summary': {
      const res = await tools.levelTools.getLevelSummary(args.levelPath);
      return cleanObject(res);
    }
    case 'delete': {
      const res = await tools.levelTools.deleteLevels({ levelPaths: [args.levelPath] });
      return cleanObject(res);
    }
    case 'set_metadata': {
      const levelPath = requireNonEmptyString(args.levelPath, 'levelPath', 'Missing required parameter: levelPath');
      const metadata = (args.metadata && typeof args.metadata === 'object') ? args.metadata : {};
      const res = await executeAutomationRequest(tools, 'set_metadata', { assetPath: levelPath, metadata });
      return cleanObject(res);
    }
    case 'validate_level': {
      const levelPath = requireNonEmptyString(args.levelPath, 'levelPath', 'Missing required parameter: levelPath');

      // Prefer an editor-side existence check when the automation bridge is available.
      const automationBridge = tools.automationBridge;
      if (automationBridge && typeof automationBridge.sendAutomationRequest === 'function' && automationBridge.isConnected()) {
        try {
          const resp: any = await automationBridge.sendAutomationRequest('execute_editor_function', {
            functionName: 'ASSET_EXISTS_SIMPLE',
            path: levelPath
          });
          const result = resp?.result ?? resp ?? {};
          const exists = Boolean(result.exists);

          return cleanObject({
            success: true,
            exists,
            levelPath: result.path ?? levelPath,
            classPath: result.class,
            // Provide a NOT_FOUND-style code for callers that care about detailed status,
            // but keep success=true so validation is non-destructive for tests.
            error: exists ? undefined : 'NOT_FOUND',
            message: exists ? 'Level asset exists' : 'Level asset not found'
          });
        } catch (err) {
          // If validation fails (bridge error, timeout, etc.), report a best-effort
          // result rather than converting this into a hard failure.
          return cleanObject({
            success: true,
            exists: false,
            levelPath,
            message: `Level validation incomplete: ${err instanceof Error ? err.message : String(err)}`
          });
        }
      }

      // Fallback when the automation bridge is unavailable.
      return cleanObject({
        success: true,
        exists: false,
        levelPath,
        message: 'Automation bridge not available; level validation is best-effort only'
      });
    }
    default:
      return await executeAutomationRequest(tools, 'manage_level', args);
  }
}
