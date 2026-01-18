import { cleanObject } from '../../utils/safe-json.js';
import { ITools } from '../../types/tool-interfaces.js';
import type { HandlerArgs, HandlerResult, SystemArgs } from '../../types/handler-types.js';
import { executeAutomationRequest } from './common-handlers.js';

/** Response from various operations */
interface OperationResponse {
  success?: boolean;
  error?: string;
  message?: string;
  settings?: unknown;
  data?: unknown;
  result?: unknown;
  [key: string]: unknown;
}

/** Validation result for an asset */
interface AssetValidationResult {
  assetPath: string;
  success?: boolean;
  error?: string | null;
  [key: string]: unknown;
}

export async function handleSystemTools(action: string, args: HandlerArgs, tools: ITools): Promise<HandlerResult> {
  const argsTyped = args as SystemArgs;
  const sysAction = String(action || '').toLowerCase();
  
  switch (sysAction) {
    case 'show_fps':
      await tools.systemTools.executeConsoleCommand(argsTyped.enabled !== false ? 'stat fps' : 'stat fps 0');
      return { success: true, message: `FPS display ${argsTyped.enabled !== false ? 'enabled' : 'disabled'}`, action: 'show_fps' };
    case 'profile': {
      const rawType = typeof argsTyped.profileType === 'string' ? argsTyped.profileType.trim() : '';
      const profileKey = rawType ? rawType.toLowerCase() : 'cpu';
      const enabled = argsTyped.enabled !== false;

      // Use built-in stat commands that are known to exist in editor builds.
      // "stat unit" is a safe choice for CPU profiling in most configurations.
      const profileMap: Record<string, string> = {
        cpu: 'stat unit',
        gamethread: 'stat game',
        renderthread: 'stat scenerendering',
        gpu: 'stat gpu',
        memory: 'stat memory',
        fps: 'stat fps',
        all: 'stat unit'
      };

      const cmd = profileMap[profileKey];
      if (!cmd) {
        return {
          success: false,
          error: 'INVALID_PROFILE_TYPE',
          message: `Unsupported profileType: ${rawType || String(argsTyped.profileType ?? '')}`,
          action: 'profile',
          profileType: argsTyped.profileType
        };
      }

      await tools.systemTools.executeConsoleCommand(cmd);
      return {
        success: true,
        message: `Profiling ${enabled ? 'enabled' : 'disabled'} (${rawType || 'CPU'})`,
        action: 'profile',
        profileType: rawType || 'CPU'
      };
    }
    case 'show_stats': {
      const category = typeof argsTyped.category === 'string' ? argsTyped.category.trim() : 'Unit';
      const enabled = argsTyped.enabled !== false;
      const cmd = `stat ${category}`;
      await tools.systemTools.executeConsoleCommand(cmd);
      return {
        success: true,
        message: `Stats display ${enabled ? 'enabled' : 'disabled'} for category: ${category}`,
        action: 'show_stats',
        category,
        enabled
      };
    }
    case 'set_quality': {
      const quality = argsTyped.level ?? 'medium';
      let qVal: number;
      if (typeof quality === 'number') {
        qVal = quality;
      } else {
        const qStr = String(quality).toLowerCase();
        qVal = (qStr === 'high' || qStr === 'epic') ? 3 : (qStr === 'low' ? 0 : (qStr === 'cinematic' ? 4 : 1));
      }
      // Clamp quality level to valid range 0-4
      qVal = Math.max(0, Math.min(4, qVal));

      const category = String(argsTyped.category || 'ViewDistance').toLowerCase();
      let cvar = 'sg.ViewDistanceQuality';

      if (category.includes('shadow')) cvar = 'sg.ShadowQuality';
      else if (category.includes('texture')) cvar = 'sg.TextureQuality';
      else if (category.includes('effect')) cvar = 'sg.EffectsQuality';
      else if (category.includes('postprocess')) cvar = 'sg.PostProcessQuality';
      else if (category.includes('foliage')) cvar = 'sg.FoliageQuality';
      else if (category.includes('shading')) cvar = 'sg.ShadingQuality';
      else if (category.includes('globalillumination') || category.includes('gi')) cvar = 'sg.GlobalIlluminationQuality';
      else if (category.includes('reflection')) cvar = 'sg.ReflectionQuality';
      else if (category.includes('viewdistance')) cvar = 'sg.ViewDistanceQuality';

      await tools.systemTools.executeConsoleCommand(`${cvar} ${qVal}`);
      return { success: true, message: `${category} quality derived from '${quality}' set to ${qVal} via ${cvar}`, action: 'set_quality' };
    }
    case 'execute_command':
      return cleanObject(await tools.systemTools.executeConsoleCommand(argsTyped.command ?? '') as HandlerResult);
    case 'create_widget': {
      const name = typeof argsTyped.name === 'string' ? argsTyped.name.trim() : '';
      const widgetPathRaw = typeof argsTyped.widgetPath === 'string' ? argsTyped.widgetPath.trim() : '';

      // If name is missing but widgetPath is provided, try to extract name from path
      let effectiveName = name || `NewWidget_${Date.now()}`;
      let effectivePath = typeof (argsTyped as Record<string, unknown>).savePath === 'string' 
        ? ((argsTyped as Record<string, unknown>).savePath as string).trim() 
        : '';

      if (!name && widgetPathRaw) {
        const parts = widgetPathRaw.split('/').filter((p: string) => p.length > 0);
        if (parts.length > 0) {
          effectiveName = parts[parts.length - 1] ?? effectiveName;
          // If path was provided as widgetPath, use the directory as savePath if savePath wasn't explicit
          if (!effectivePath) {
            effectivePath = '/' + parts.slice(0, parts.length - 1).join('/');
          }
        }
      }

      if (!effectiveName) {
        return {
          success: false,
          error: 'INVALID_ARGUMENT',
          message: 'Widget name is required for creation',
          action: 'create_widget'
        };
      }

      try {
        const res = await tools.uiTools.createWidget({
          name: effectiveName,
          type: (argsTyped as Record<string, unknown>).widgetType as string | undefined,
          savePath: effectivePath
        });

        return cleanObject({
          ...res,
          action: 'create_widget'
        });
      } catch (error: unknown) {
        const msg = error instanceof Error ? error.message : String(error);
        return {
          success: false,
          error: `Failed to create widget: ${msg}`,
          message: msg,
          action: 'create_widget'
        };
      }
    }
    case 'show_widget': {
      const widgetId = typeof (argsTyped as Record<string, unknown>).widgetId === 'string' 
        ? ((argsTyped as Record<string, unknown>).widgetId as string).trim() 
        : '';

      if (widgetId.toLowerCase() === 'notification') {
        const message = typeof (argsTyped as Record<string, unknown>).message === 'string'
          ? ((argsTyped as Record<string, unknown>).message as string).trim()
          : '';
        const text = message.length > 0 ? message : 'Notification';
        const duration = typeof (argsTyped as Record<string, unknown>).duration === 'number' 
          ? (argsTyped as Record<string, unknown>).duration as number 
          : undefined;

        try {
          const res = await tools.uiTools.showNotification({ text, duration }) as OperationResponse;
          const ok = res && res.success !== false;
          if (ok) {
            return {
              success: true,
              message: res.message || 'Notification shown',
              action: 'show_widget',
              widgetId,
              handled: true
            };
          }
          return cleanObject({
            success: false,
            error: res?.error || 'NOTIFICATION_FAILED',
            message: res?.message || 'Failed to show notification',
            action: 'show_widget',
            widgetId
          });
        } catch (error: unknown) {
          const msg = error instanceof Error ? error.message : String(error);
          return {
            success: false,
            error: 'NOTIFICATION_FAILED',
            message: msg,
            action: 'show_widget',
            widgetId
          };
        }
      }

      const widgetPath = (typeof argsTyped.widgetPath === 'string' ? argsTyped.widgetPath.trim() : '') 
        || (typeof argsTyped.name === 'string' ? argsTyped.name.trim() : '');
      if (!widgetPath) {
        return {
          success: false,
          error: 'INVALID_ARGUMENT',
          message: 'widgetPath (or name) is required to show a widget',
          action: 'show_widget',
          widgetId
        };
      }

      return cleanObject(await tools.uiTools.showWidget(widgetPath));
    }
    case 'add_widget_child': {
      const widgetPath = typeof argsTyped.widgetPath === 'string' ? argsTyped.widgetPath.trim() : '';
      const childClass = typeof argsTyped.childClass === 'string' ? argsTyped.childClass.trim() : '';
      const parentName = typeof argsTyped.parentName === 'string' ? argsTyped.parentName.trim() : undefined;

      if (!widgetPath || !childClass) {
        return {
          success: false,
          error: 'INVALID_ARGUMENT',
          message: 'widgetPath and childClass are required',
          action: 'add_widget_child'
        };
      }

      try {
        const res = await tools.uiTools.addWidgetComponent({
          widgetName: widgetPath,
          componentType: childClass,
          componentName: 'NewChild',
          slot: parentName ? { position: [0, 0] } : undefined
        });
        return cleanObject({
          ...res,
          action: 'add_widget_child'
        });
      } catch (error: unknown) {
        const msg = error instanceof Error ? error.message : String(error);
        return {
          success: false,
          error: `Failed to add widget child: ${msg}`,
          message: msg,
          action: 'add_widget_child'
        };
      }
    }
    case 'set_cvar': {
      // Accept multiple parameter names: name, cvar, key
      const nameVal = typeof argsTyped.name === 'string' && argsTyped.name.trim().length > 0
        ? argsTyped.name.trim()
        : '';
      const cvarVal = typeof (argsTyped as Record<string, unknown>).cvar === 'string'
        ? ((argsTyped as Record<string, unknown>).cvar as string).trim()
        : '';
      const keyVal = typeof argsTyped.key === 'string' ? argsTyped.key.trim() : '';
      const cmdVal = typeof argsTyped.command === 'string' ? argsTyped.command.trim() : '';
      
      const rawInput = nameVal || cvarVal || keyVal || cmdVal;

      // Some callers pass a full "cvar value" command string.
      const tokens = rawInput.split(/\s+/).filter(Boolean);
      const rawName = tokens[0] ?? '';

      if (!rawName) {
        return {
          success: false,
          error: 'INVALID_ARGUMENT',
          message: 'CVar name is required',
          action: 'set_cvar'
        };
      }

      const value = (argsTyped.value !== undefined && argsTyped.value !== null)
        ? argsTyped.value
        : (tokens.length > 1 ? tokens.slice(1).join(' ') : '');
      await tools.systemTools.executeConsoleCommand(`${rawName} ${value}`);
      return {
        success: true,
        message: `CVar ${rawName} set to ${value}`,
        action: 'set_cvar',
        cvar: rawName,
        value
      };
    }
    case 'get_project_settings': {
      const section = typeof argsTyped.category === 'string' && argsTyped.category.trim().length > 0
        ? argsTyped.category
        : argsTyped.section;
      const resp = await tools.systemTools.getProjectSettings(section) as OperationResponse;
      if (resp && resp.success && (resp.settings || resp.data || resp.result)) {
        return cleanObject({
          success: true,
          message: 'Project settings retrieved',
          settings: resp.settings || resp.data || resp.result,
          ...resp
        });
      }
      return cleanObject(resp);
    }
    case 'validate_assets': {
      const paths: string[] = Array.isArray((argsTyped as Record<string, unknown>).paths) 
        ? (argsTyped as Record<string, unknown>).paths as string[]
        : [];
      if (!paths.length) {
        return {
          success: false,
          error: 'INVALID_ARGUMENT',
          message: 'Please provide array of "paths" to validate assets.',
          action: 'validate_assets',
          results: []
        };
      }

      // Construct batch requests
      const requests = paths.map((rawPath, index) => {
        const assetPath = typeof rawPath === 'string' ? rawPath : String(rawPath ?? '');
        return {
          action: 'validate_asset',
          requestId: String(index),
          assetPath
        };
      });

      // Execute batch via system_control
      const batchRes = await executeAutomationRequest(tools, 'system_control', { 
        action: 'batch_execute', 
        requests 
      }, 'Automation bridge not available') as { results?: Array<{ requestId?: string, success?: boolean, error?: unknown, [key: string]: unknown }> };

      // Map results back to order
      const resultsMap = new Map<string, AssetValidationResult>();
      
      if (Array.isArray(batchRes.results)) {
        for (const r of batchRes.results) {
          const reqId = String(r.requestId);
          const pathIndex = parseInt(reqId, 10);
          
          if (!isNaN(pathIndex) && pathIndex >= 0 && pathIndex < paths.length) {
            const assetPath = requests[pathIndex]?.assetPath ?? 'unknown';
            
            // Extract error message
            let errorStr: string | null = null;
            if (r.error) {
              if (typeof r.error === 'string') {
                errorStr = r.error;
              } else if (typeof r.error === 'object' && r.error !== null && 'message' in r.error) {
                errorStr = String((r.error as { message: string }).message);
              } else {
                errorStr = String(r.error);
              }
            }

            resultsMap.set(reqId, {
              assetPath,
              success: r.success,
              error: errorStr
            });
          }
        }
      }

      // Reconstruct final results array in original order
      const results: AssetValidationResult[] = requests.map((req, index) => {
        const reqId = String(index);
        const result = resultsMap.get(reqId);
        if (result) {
          return result;
        }
        return {
          assetPath: req.assetPath,
          success: false,
          error: 'Batch execution failed to return result'
        };
      });

      return {
        success: true,
        message: 'Asset validation completed',
        action: 'validate_assets',
        results
      };
    }
    case 'play_sound': {
      const soundPath = typeof (argsTyped as Record<string, unknown>).soundPath === 'string' 
        ? ((argsTyped as Record<string, unknown>).soundPath as string).trim() 
        : '';
      const volume = typeof (argsTyped as Record<string, unknown>).volume === 'number' 
        ? (argsTyped as Record<string, unknown>).volume as number 
        : undefined;
      const pitch = typeof (argsTyped as Record<string, unknown>).pitch === 'number' 
        ? (argsTyped as Record<string, unknown>).pitch as number 
        : undefined;

      // Volume 0 should behave as a silent, handled no-op
      if (typeof volume === 'number' && volume <= 0) {
        return {
          success: true,
          message: 'Sound request handled (volume is 0 - silent)',
          action: 'play_sound',
          soundPath,
          volume,
          pitch,
          handled: true
        };
      }

      try {
        const res = await tools.audioTools.playSound(soundPath, volume, pitch) as OperationResponse;
        if (!res || res.success === false) {
          const errText = String(res?.error || '').toLowerCase();
          const isMissingAsset = errText.includes('asset_not_found') || errText.includes('asset not found');

          if (isMissingAsset || !soundPath) {
            // Attempt fallback to a known engine sound
            const fallbackPath = '/Engine/EditorSounds/Notifications/CompileSuccess_Cue';
            if (soundPath !== fallbackPath) {
              const fallbackRes = await tools.audioTools.playSound(fallbackPath, volume, pitch) as OperationResponse;
              if (fallbackRes.success) {
                return {
                  success: true,
                  message: `Sound asset not found, played fallback sound: ${fallbackPath}`,
                  action: 'play_sound',
                  soundPath: fallbackPath,
                  originalPath: soundPath,
                  volume,
                  pitch
                };
              }
            }

            return {
              success: false,
              error: 'ASSET_NOT_FOUND',
              message: 'Sound asset not found (and fallback failed)',
              action: 'play_sound',
              soundPath,
              volume,
              pitch
            };
          }

          return cleanObject({
            success: false,
            error: res?.error || 'Failed to play 2D sound',
            action: 'play_sound',
            soundPath,
            volume,
            pitch
          });
        }

        return cleanObject({
          ...res,
          action: 'play_sound',
          soundPath,
          volume,
          pitch
        });
      } catch (error: unknown) {
        const msg = error instanceof Error ? error.message : String(error);
        const lowered = msg.toLowerCase();
        const isMissingAsset = lowered.includes('asset_not_found') || lowered.includes('asset not found');

        if (isMissingAsset || !soundPath) {
          return {
            success: false,
            error: 'ASSET_NOT_FOUND',
            message: 'Sound asset not found',
            action: 'play_sound',
            soundPath,
            volume,
            pitch
          };
        }

        // Fallback: If asset not found, try playing default engine sound
        if (isMissingAsset) {
          const fallbackSound = '/Engine/EditorSounds/Notifications/CompileSuccess_Cue';
          try {
            const fallbackRes = await tools.audioTools.playSound(fallbackSound, volume, pitch) as OperationResponse;
            if (fallbackRes && fallbackRes.success) {
              return {
                success: true,
                message: `Original sound not found. Played fallback sound: ${fallbackSound}`,
                action: 'play_sound',
                soundPath,
                fallback: true,
                volume,
                pitch
              };
            }
          } catch (_fallbackErr) {
            // Ignore fallback failure and return original error
          }
        }

        return {
          success: false,
          error: `Failed to play 2D sound: ${msg}`,
          action: 'play_sound',
          soundPath,
          volume,
          pitch
        };
      }
    }
    case 'screenshot': {
      const includeMetadata = (argsTyped as Record<string, unknown>).includeMetadata === true;
      const filenameArg = typeof (argsTyped as Record<string, unknown>).filename === 'string' 
        ? (argsTyped as Record<string, unknown>).filename as string 
        : undefined;

      if (includeMetadata) {
        const baseName = filenameArg && filenameArg.trim().length > 0
          ? filenameArg.trim()
          : `Screenshot_${Date.now()}`;

        try {
          await tools.editorTools.takeScreenshot(baseName);
        } catch (error: unknown) {
          const msg = error instanceof Error ? error.message : String(error);
          return {
            success: false,
            error: 'SCREENSHOT_FAILED',
            message: msg,
            filename: baseName,
            includeMetadata: true,
            metadata: (argsTyped as Record<string, unknown>).metadata,
            action: 'screenshot'
          };
        }

        return {
          success: true,
          message: `Metadata screenshot captured: ${baseName}`,
          filename: baseName,
          includeMetadata: true,
          metadata: (argsTyped as Record<string, unknown>).metadata,
          action: 'screenshot',
          handled: true
        };
      }

      return cleanObject(await tools.editorTools.takeScreenshot(filenameArg));
    }
    case 'set_resolution': {
      const parseResolution = (value: unknown): { width?: number; height?: number } => {
        if (typeof value !== 'string') return {};
        const m = value.trim().match(/^(\d+)x(\d+)$/i);
        if (!m) return {};
        return { width: Number(m[1]), height: Number(m[2]) };
      };

      const parsed = parseResolution(argsTyped.resolution);
      const argsRecord = argsTyped as HandlerResult;
      const width = Number.isFinite(Number(argsRecord.width)) ? Number(argsRecord.width) : (parsed.width ?? NaN);
      const height = Number.isFinite(Number(argsRecord.height)) ? Number(argsRecord.height) : (parsed.height ?? NaN);
      if (!Number.isFinite(width) || !Number.isFinite(height) || width <= 0 || height <= 0) {
        return {
          success: false,
          error: 'VALIDATION_ERROR',
          message: 'Validation error: Invalid resolution: width and height must be positive numbers',
          action: 'set_resolution'
        };
      }
      const windowed = argsRecord.windowed !== false; // default to windowed=true
      const suffix = windowed ? 'w' : 'f';
      await tools.systemTools.executeConsoleCommand(`r.SetRes ${width}x${height}${suffix}`);
      return {
        success: true,
        message: `Resolution set to ${width}x${height} (${windowed ? 'windowed' : 'fullscreen'})`,
        action: 'set_resolution'
      };
    }
    case 'set_fullscreen': {
      const parseResolution = (value: unknown): { width?: number; height?: number } => {
        if (typeof value !== 'string') return {};
        const m = value.trim().match(/^(\d+)x(\d+)$/i);
        if (!m) return {};
        return { width: Number(m[1]), height: Number(m[2]) };
      };

      const parsed = parseResolution(argsTyped.resolution);
      const argsRecord = argsTyped as HandlerResult;
      const width = Number.isFinite(Number(argsRecord.width)) ? Number(argsRecord.width) : (parsed.width ?? NaN);
      const height = Number.isFinite(Number(argsRecord.height)) ? Number(argsRecord.height) : (parsed.height ?? NaN);

      const windowed = argsRecord.windowed === true || argsTyped.enabled === false;
      const suffix = windowed ? 'w' : 'f';

      if (!Number.isFinite(width) || !Number.isFinite(height) || width <= 0 || height <= 0) {
        // If only toggling mode and no resolution provided, attempt a mode toggle.
        if (typeof argsRecord.windowed === 'boolean' || typeof argsTyped.enabled === 'boolean') {
          await tools.systemTools.executeConsoleCommand(`r.FullScreenMode ${windowed ? 1 : 0}`);
          return {
            success: true,
            message: `Fullscreen mode toggled (${windowed ? 'windowed' : 'fullscreen'})`,
            action: 'set_fullscreen',
            handled: true
          };
        }

        return {
          success: false,
          error: 'VALIDATION_ERROR',
          message: 'Invalid resolution: provide width/height or resolution like "1920x1080"',
          action: 'set_fullscreen'
        };
      }

      await tools.systemTools.executeConsoleCommand(`r.SetRes ${width}x${height}${suffix}`);
      return {
        success: true,
        message: `Fullscreen mode set to ${width}x${height} (${windowed ? 'windowed' : 'fullscreen'})`,
        action: 'set_fullscreen'
      };
    }
    case 'read_log':
      return cleanObject(await tools.logTools.readOutputLog(args as Record<string, unknown>)) as HandlerResult;
    case 'batch_execute': {
      const requests = (argsTyped as Record<string, unknown>).requests;
      if (!Array.isArray(requests)) {
        return {
          success: false,
          error: 'INVALID_ARGUMENT',
          message: 'requests must be an array',
          action: 'batch_execute'
        };
      }
      // Pass through to bridge
      return cleanObject(await executeAutomationRequest(tools, 'system_control', { action: 'batch_execute', requests }, 'Automation bridge not available')) as HandlerResult;
    }
    // =========================================================================
    // PHASE 4.1: EVENT PUSH SYSTEM
    // =========================================================================
    case 'subscribe_to_event': {
      const eventType = (argsTyped as Record<string, unknown>).eventType as string;
      if (!eventType) {
        return {
          success: false,
          error: 'VALIDATION_ERROR',
          message: 'eventType is required (e.g., asset.saved, actor.spawned, level.loaded, compile.complete)',
          action: 'subscribe_to_event'
        };
      }
      return cleanObject(await executeAutomationRequest(tools, 'control_editor', { action: 'subscribe_to_event', eventType }, 'Automation bridge not available')) as HandlerResult;
    }
    case 'unsubscribe_from_event': {
      const eventType = (argsTyped as Record<string, unknown>).eventType as string;
      if (!eventType) {
        return {
          success: false,
          error: 'VALIDATION_ERROR',
          message: 'eventType is required',
          action: 'unsubscribe_from_event'
        };
      }
      return cleanObject(await executeAutomationRequest(tools, 'control_editor', { action: 'unsubscribe_from_event', eventType }, 'Automation bridge not available')) as HandlerResult;
    }
    case 'get_subscribed_events':
      return cleanObject(await executeAutomationRequest(tools, 'control_editor', { action: 'get_subscribed_events' }, 'Automation bridge not available')) as HandlerResult;
    case 'configure_event_channel': {
      const channelId = (argsTyped as Record<string, unknown>).channelId as string;
      return cleanObject(await executeAutomationRequest(tools, 'control_editor', { action: 'configure_event_channel', channelId, ...(argsTyped as Record<string, unknown>) }, 'Automation bridge not available')) as HandlerResult;
    }
    case 'get_event_history': {
      const limit = (argsTyped as Record<string, unknown>).limit as number || 100;
      const eventType = (argsTyped as Record<string, unknown>).eventType as string;
      return cleanObject(await executeAutomationRequest(tools, 'control_editor', { action: 'get_event_history', limit, eventType }, 'Automation bridge not available')) as HandlerResult;
    }
    case 'clear_event_subscriptions':
      return cleanObject(await executeAutomationRequest(tools, 'control_editor', { action: 'clear_event_subscriptions' }, 'Automation bridge not available')) as HandlerResult;
    // =========================================================================
    // PHASE 4.3: BACKGROUND JOB MANAGEMENT
    // =========================================================================
    case 'start_background_job': {
      const jobType = (argsTyped as Record<string, unknown>).jobType as string;
      if (!jobType) {
        return {
          success: false,
          error: 'VALIDATION_ERROR',
          message: 'jobType is required (e.g., cook, validate, bulk_import)',
          action: 'start_background_job'
        };
      }
      return cleanObject(await executeAutomationRequest(tools, 'control_editor', { action: 'start_background_job', jobType, ...(argsTyped as Record<string, unknown>) }, 'Automation bridge not available')) as HandlerResult;
    }
    case 'get_job_status': {
      const jobId = (argsTyped as Record<string, unknown>).jobId as string;
      if (!jobId) {
        return {
          success: false,
          error: 'VALIDATION_ERROR',
          message: 'jobId is required',
          action: 'get_job_status'
        };
      }
      return cleanObject(await executeAutomationRequest(tools, 'control_editor', { action: 'get_job_status', jobId }, 'Automation bridge not available')) as HandlerResult;
    }
    case 'cancel_job': {
      const jobId = (argsTyped as Record<string, unknown>).jobId as string;
      if (!jobId) {
        return {
          success: false,
          error: 'VALIDATION_ERROR',
          message: 'jobId is required',
          action: 'cancel_job'
        };
      }
      return cleanObject(await executeAutomationRequest(tools, 'control_editor', { action: 'cancel_job', jobId }, 'Automation bridge not available')) as HandlerResult;
    }
    case 'get_active_jobs':
      return cleanObject(await executeAutomationRequest(tools, 'control_editor', { action: 'get_active_jobs' }, 'Automation bridge not available')) as HandlerResult;
    default: {
      const res = await executeAutomationRequest(tools, 'system_control', args, 'Automation bridge not available for system control operations');
      return cleanObject(res) as HandlerResult;
    }
  }
}

export async function handleConsoleCommand(args: HandlerArgs, tools: ITools): Promise<HandlerResult> {
  const rawCommand = typeof args?.command === 'string' ? args.command : '';
  const trimmed = rawCommand.trim();

  if (!trimmed) {
    return cleanObject({
      success: false,
      error: 'EMPTY_COMMAND',
      message: 'Console command is empty',
      command: rawCommand
    });
  }

  const res = await executeAutomationRequest(
    tools,
    'console_command',
    { ...args, command: trimmed },
    'Automation bridge not available for console command operations'
  );
  return cleanObject(res) as HandlerResult;
}
