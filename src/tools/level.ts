import { BaseTool } from './base-tool.js';
import { ILevelTools, StandardActionResponse } from '../types/tool-interfaces.js';
import { LevelResponse } from '../types/automation-responses.js';
import { wasmIntegration as _wasmIntegration } from '../wasm/index.js';
import { sanitizePath } from '../utils/path-security.js';
import {
  DEFAULT_OPERATION_TIMEOUT_MS,
  DEFAULT_ASSET_OP_TIMEOUT_MS,
  LONG_RUNNING_OP_TIMEOUT_MS
} from '../constants.js';

type LevelExportRecord = { target: string; timestamp: number; note?: string };
type ManagedLevelRecord = {
  path: string;
  name: string;
  partitioned: boolean;
  streaming: boolean;
  loaded: boolean;
  visible: boolean;
  createdAt: number;
  lastSavedAt?: number;
  metadata?: Record<string, unknown>;
  exports: LevelExportRecord[];
  lights: Array<{ name: string; type: string; createdAt: number; details?: Record<string, unknown> }>;
};

export class LevelTools extends BaseTool implements ILevelTools {
  private managedLevels = new Map<string, ManagedLevelRecord>();
  private listCache?: { result: { success: true; message: string; count: number; levels: Array<Record<string, unknown>> }; timestamp: number };
  private readonly LIST_CACHE_TTL_MS = 750;
  private currentLevelPath?: string;

  private invalidateListCache() {
    this.listCache = undefined;
  }

  private normalizeLevelPath(rawPath: string | undefined): { path: string; name: string } {
    if (!rawPath) {
      return { path: '/Game/Maps/Untitled', name: 'Untitled' };
    }

    let formatted = rawPath.replace(/\\/g, '/').trim();
    if (!formatted.startsWith('/')) {
      formatted = formatted.startsWith('Game/') ? `/${formatted}` : `/Game/${formatted.replace(/^\/?Game\//i, '')}`;
    }
    if (!formatted.startsWith('/Game/')) {
      formatted = `/Game/${formatted.replace(/^\/+/, '')}`;
    }

    // Security validation
    try {
      formatted = sanitizePath(formatted);
    } catch (e: unknown) {
      // If sanitizePath fails, we should probably propagate that error, 
      // but normalizeLevelPath signature expects to return an object.
      // For now, let's log and rethrow or fallback? 
      // Throwing is safer as it prevents operation on invalid path.
      throw new Error(`Security validation failed for level path: ${e instanceof Error ? e.message : String(e)}`);
    }

    formatted = formatted.replace(/\.umap$/i, '');
    if (formatted.endsWith('/')) {
      formatted = formatted.slice(0, -1);
    }
    const segments = formatted.split('/').filter(Boolean);
    const lastSegment = segments[segments.length - 1] ?? 'Untitled';
    const name = lastSegment.includes('.') ? lastSegment.split('.').pop() ?? lastSegment : lastSegment;
    return { path: formatted, name: name || 'Untitled' };
  }

  private ensureRecord(path: string, seed?: Partial<ManagedLevelRecord>): ManagedLevelRecord {
    const normalized = this.normalizeLevelPath(path);
    let record = this.managedLevels.get(normalized.path);
    if (!record) {
      record = {
        path: normalized.path,
        name: seed?.name ?? normalized.name,
        partitioned: seed?.partitioned ?? false,
        streaming: seed?.streaming ?? false,
        loaded: seed?.loaded ?? false,
        visible: seed?.visible ?? false,
        createdAt: seed?.createdAt ?? Date.now(),
        lastSavedAt: seed?.lastSavedAt,
        metadata: seed?.metadata ? { ...seed.metadata } : undefined,
        exports: seed?.exports ? [...seed.exports] : [],
        lights: seed?.lights ? [...seed.lights] : []
      };
      this.managedLevels.set(normalized.path, record);
      this.invalidateListCache();
    }
    return record;
  }

  private mutateRecord(path: string | undefined, updates: Partial<ManagedLevelRecord>): ManagedLevelRecord | undefined {
    if (!path || !path.trim()) {
      return undefined;
    }

    const record = this.ensureRecord(path, updates);
    let changed = false;

    if (updates.name !== undefined && updates.name !== record.name) {
      record.name = updates.name;
      changed = true;
    }
    if (updates.partitioned !== undefined && updates.partitioned !== record.partitioned) {
      record.partitioned = updates.partitioned;
      changed = true;
    }
    if (updates.streaming !== undefined && updates.streaming !== record.streaming) {
      record.streaming = updates.streaming;
      changed = true;
    }
    if (updates.loaded !== undefined && updates.loaded !== record.loaded) {
      record.loaded = updates.loaded;
      changed = true;
    }
    if (updates.visible !== undefined && updates.visible !== record.visible) {
      record.visible = updates.visible;
      changed = true;
    }
    if (updates.createdAt !== undefined && updates.createdAt !== record.createdAt) {
      record.createdAt = updates.createdAt;
      changed = true;
    }
    if (updates.lastSavedAt !== undefined && updates.lastSavedAt !== record.lastSavedAt) {
      record.lastSavedAt = updates.lastSavedAt;
      changed = true;
    }
    if (updates.metadata) {
      record.metadata = { ...(record.metadata ?? {}), ...updates.metadata };
      changed = true;
    }
    if (updates.exports && updates.exports.length > 0) {
      record.exports = [...record.exports, ...updates.exports];
      changed = true;
    }
    if (updates.lights && updates.lights.length > 0) {
      record.lights = [...record.lights, ...updates.lights];
      changed = true;
    }

    if (changed) {
      this.invalidateListCache();
    }

    return record;
  }

  private getRecord(path: string | undefined): ManagedLevelRecord | undefined {
    if (!path || !path.trim()) {
      return undefined;
    }
    const normalized = this.normalizeLevelPath(path);
    return this.managedLevels.get(normalized.path);
  }

  private resolveLevelPath(explicit?: string): string | undefined {
    if (explicit && explicit.trim()) {
      return this.normalizeLevelPath(explicit).path;
    }
    return this.currentLevelPath;
  }

  private removeRecord(path: string) {
    const normalized = this.normalizeLevelPath(path);
    if (this.managedLevels.delete(normalized.path)) {
      if (this.currentLevelPath === normalized.path) {
        this.currentLevelPath = undefined;
      }
      this.invalidateListCache();
    }
  }

  private listManagedLevels(): { success: true; message: string; count: number; levels: Array<Record<string, unknown>> } {
    const now = Date.now();
    if (this.listCache && now - this.listCache.timestamp < this.LIST_CACHE_TTL_MS) {
      return this.listCache.result;
    }

    const levels = Array.from(this.managedLevels.values()).map((record) => ({
      path: record.path,
      name: record.name,
      partitioned: record.partitioned,
      streaming: record.streaming,
      loaded: record.loaded,
      visible: record.visible,
      createdAt: record.createdAt,
      lastSavedAt: record.lastSavedAt,
      exports: record.exports,
      lightCount: record.lights.length
    }));

    const result = { success: true as const, message: 'Managed levels listed', count: levels.length, levels };
    this.listCache = { result, timestamp: now };
    return result;
  }

  private summarizeLevel(path: string): Record<string, unknown> {
    const record = this.getRecord(path);
    if (!record) {
      return { success: false, error: `Level not tracked: ${path}` };
    }

    return {
      success: true,
      message: 'Level summary ready',
      path: record.path,
      name: record.name,
      partitioned: record.partitioned,
      streaming: record.streaming,
      loaded: record.loaded,
      visible: record.visible,
      createdAt: record.createdAt,
      lastSavedAt: record.lastSavedAt,
      exports: record.exports,
      lights: record.lights,
      metadata: record.metadata
    };
  }

  private setCurrentLevel(path: string) {
    const normalized = this.normalizeLevelPath(path);
    this.currentLevelPath = normalized.path;
    this.ensureRecord(normalized.path, { loaded: true, visible: true });
  }

  async listLevels(): Promise<StandardActionResponse> {
    // Try to get actual levels from UE via automation bridge
    try {
      const response = await this.sendAutomationRequest<LevelResponse>('list_levels', {}, {
        timeoutMs: 10000
      });

      if (response && response.success !== false) {
        // Also include managed levels for backwards compatibility and immediate visibility
        const managed = this.listManagedLevels();

        // Merge managed levels into the main list if not already present
        const ueLevels = (response.allMaps || []) as Array<Record<string, unknown>>;
        const managedOnly = managed.levels.filter(m => !ueLevels.some(u => u.path === m.path));
        const finalLevels = [...ueLevels, ...managedOnly];

        const result: Record<string, unknown> = {
          ...response,
          success: true,
          message: 'Levels listed from Unreal Engine',
          levels: finalLevels,
          currentMap: response.currentMap,
          currentMapPath: response.currentMapPath,
          currentWorldLevels: response.currentWorldLevels || [],
          data: {
            levels: finalLevels,
            count: finalLevels.length
          },
          managedLevels: managed.levels,
          managedLevelCount: managed.count
        };

        return result as StandardActionResponse;
      }
    } catch {
      // Fall back to managed levels if automation bridge fails
    }

    // Fallback to locally managed levels
    return this.listManagedLevels();
  }

  async getLevelSummary(levelPath?: string): Promise<StandardActionResponse> {
    const resolved = this.resolveLevelPath(levelPath);
    if (!resolved) {
      return { success: false, error: 'No level specified' };
    }

    // Try to get actual level summary from UE via automation bridge
    try {
      const response = await this.sendAutomationRequest<LevelResponse>('manage_level', {
        action: 'get_summary',
        levelPath: resolved
      }, {
        timeoutMs: DEFAULT_OPERATION_TIMEOUT_MS
      });

      if (response && response.success !== false) {
        // Merge with local tracking data if available
        const localRecord = this.getRecord(resolved);
        const result: Record<string, unknown> = {
          ...response,
          success: true,
          message: response.message || 'Level summary retrieved from Unreal Engine',
          path: resolved
        };

        // Add local tracking info if available
        if (localRecord) {
          result.localTracking = {
            partitioned: localRecord.partitioned,
            streaming: localRecord.streaming,
            loaded: localRecord.loaded,
            visible: localRecord.visible,
            createdAt: localRecord.createdAt,
            lastSavedAt: localRecord.lastSavedAt,
            exports: localRecord.exports,
            lights: localRecord.lights,
            metadata: localRecord.metadata
          };
        }

        return result as StandardActionResponse;
      }
    } catch {
      // Fall back to local summary if automation bridge fails
    }

    // Fallback to locally managed summary
    return this.summarizeLevel(resolved) as StandardActionResponse;
  }

  registerLight(levelPath: string | undefined, info: { name: string; type: string; details?: Record<string, unknown> }) {
    const resolved = this.resolveLevelPath(levelPath);
    if (!resolved) {
      return;
    }
    this.mutateRecord(resolved, {
      lights: [
        {
          name: info.name,
          type: info.type,
          createdAt: Date.now(),
          details: info.details
        }
      ]
    });
  }

  async exportLevel(params: { levelPath?: string; exportPath: string; note?: string; timeoutMs?: number }): Promise<StandardActionResponse> {
    const resolved = this.resolveLevelPath(params.levelPath);
    if (!resolved) {
      return { success: false, error: 'No level specified for export' };
    }

    try {
      const res = await this.sendAutomationRequest<LevelResponse>('manage_level', {
        action: 'export_level',
        levelPath: resolved,
        exportPath: params.exportPath
      }, { timeoutMs: params.timeoutMs ?? LONG_RUNNING_OP_TIMEOUT_MS });

      if (res?.success === false) {
        return {
          success: false,
          error: res.error || res.message || 'Export failed',
          levelPath: resolved,
          exportPath: params.exportPath,
          details: res
        } as StandardActionResponse;
      }

      return {
        success: true,
        message: `Level exported to ${params.exportPath}`,
        levelPath: resolved,
        exportPath: params.exportPath,
        details: res
      } as StandardActionResponse;
    } catch (e: unknown) {
      return { success: false, error: `Export failed: ${e instanceof Error ? e.message : String(e)}` };
    }
  }

  async importLevel(params: { packagePath: string; destinationPath?: string; streaming?: boolean; timeoutMs?: number }): Promise<StandardActionResponse> {
    const destination = params.destinationPath
      ? this.normalizeLevelPath(params.destinationPath)
      : this.normalizeLevelPath(`/Game/Maps/Imported_${Math.floor(Date.now() / 1000)}`);

    try {
      const res = await this.sendAutomationRequest<LevelResponse>('manage_level', {
        action: 'import_level',
        packagePath: params.packagePath,
        destinationPath: destination.path
      }, { timeoutMs: params.timeoutMs ?? LONG_RUNNING_OP_TIMEOUT_MS });

      if ((res as Record<string, unknown>)?.success === false) {
        const resObj = res as Record<string, unknown>;
        return {
          success: false,
          error: (resObj.error as string) || (resObj.message as string) || 'Import failed',
          levelPath: destination.path,
          details: res
        } as StandardActionResponse;
      }

      return {
        success: true,
        message: `Level imported to ${destination.path}`,
        levelPath: destination.path,
        partitioned: true,
        streaming: Boolean(params.streaming),
        details: res
      } as StandardActionResponse;
    } catch (e: unknown) {
      return { success: false, error: `Import failed: ${e instanceof Error ? e.message : String(e)}` };
    }
  }

  async saveLevelAs(params: { sourcePath?: string; targetPath: string }): Promise<StandardActionResponse> {
    const source = this.resolveLevelPath(params.sourcePath);
    const target = this.normalizeLevelPath(params.targetPath);

    // Delegate to automation bridge
    try {
      const response = await this.sendAutomationRequest<LevelResponse>('manage_level', {
        action: 'save_level_as',
        savePath: target.path
      }, {
        timeoutMs: DEFAULT_ASSET_OP_TIMEOUT_MS
      });

      if (response.success === false) {
        return { success: false, error: response.error || response.message || 'Failed to save level as' };
      }

      // If successful, update local state
      if (!source) {
        // If no source known, just ensure target record
        this.ensureRecord(target.path, {
          name: target.name,
          loaded: true,
          visible: true,
          createdAt: Date.now(),
          lastSavedAt: Date.now()
        });
      } else {
        const sourceRecord = this.getRecord(source);
        const now = Date.now();
        this.ensureRecord(target.path, {
          name: target.name,
          partitioned: sourceRecord?.partitioned ?? true,
          streaming: sourceRecord?.streaming ?? false,
          loaded: true,
          visible: true,
          metadata: { ...(sourceRecord?.metadata ?? {}), savedFrom: source },
          exports: sourceRecord?.exports ?? [],
          lights: sourceRecord?.lights ?? [],
          createdAt: sourceRecord?.createdAt ?? now,
          lastSavedAt: now
        });
      }

      this.setCurrentLevel(target.path);

      return {
        success: true,
        message: response.message || `Level saved as ${target.path}`,
        levelPath: target.path
      } as StandardActionResponse;
    } catch (error) {
      return { success: false, error: `Failed to save level as: ${error instanceof Error ? error.message : String(error)}` };
    }
  }

  async deleteLevels(params: { levelPaths: string[] }): Promise<StandardActionResponse> {
    const normalizedPaths: string[] = [];
    for (const path of params.levelPaths) {
      normalizedPaths.push(this.normalizeLevelPath(path).path);
    }

    // Route to C++ bridge for actual asset deletion
    try {
      const response = await this.sendAutomationRequest<LevelResponse>('manage_level', {
        action: 'delete',
        levelPaths: normalizedPaths
      }, {
        timeoutMs: DEFAULT_ASSET_OP_TIMEOUT_MS
      });

      if (response && response.success !== false) {
        // Also remove from local tracking
        const removed: string[] = [];
        for (const normalized of normalizedPaths) {
          if (this.managedLevels.has(normalized)) {
            this.removeRecord(normalized);
            removed.push(normalized);
          }
        }

        return {
          ...response,
          success: true,
          message: response.message || `Deleted ${normalizedPaths.length} level(s)`,
          deleted: normalizedPaths,
          locallyRemoved: removed
        } as StandardActionResponse;
      }

      return {
        success: false,
        error: response.error || response.message || 'Failed to delete levels',
        levelPaths: normalizedPaths
      } as StandardActionResponse;
    } catch (error) {
      return {
        success: false,
        error: `Failed to delete levels: ${error instanceof Error ? error.message : String(error)}`,
        levelPaths: normalizedPaths
      } as StandardActionResponse;
    }
  }

  async loadLevel(params: {
    levelPath: string;
    streaming?: boolean;
    position?: [number, number, number];
  }): Promise<StandardActionResponse> {
    const normalizedPath = this.normalizeLevelPath(params.levelPath).path;

    if (params.streaming) {
      try {
        const simpleName = (params.levelPath || '').split('/').filter(Boolean).pop() || params.levelPath;
        await this.bridge.executeConsoleCommand(`StreamLevel ${simpleName} Load Show`);
        this.mutateRecord(normalizedPath, {
          streaming: true,
          loaded: true,
          visible: true
        });
        return {
          success: true,
          message: `Streaming level loaded: ${params.levelPath}`,
          levelPath: normalizedPath,
          streaming: true
        } as StandardActionResponse;
      } catch (err) {
        return {
          success: false,
          error: `Failed to load streaming level: ${err}`,
          levelPath: normalizedPath
        };
      }
    } else {
      // Try loading via automation bridge first (more robust)
      try {
        const response = await this.sendAutomationRequest<LevelResponse>('manage_level', {
          action: 'load',
          levelPath: params.levelPath
        }, { timeoutMs: DEFAULT_OPERATION_TIMEOUT_MS });

        if (response.success) {
          this.setCurrentLevel(normalizedPath);
          this.mutateRecord(normalizedPath, {
            streaming: false,
            loaded: true,
            visible: true
          });
          return {
            ...response,
            success: true,
            message: `Level loaded: ${params.levelPath}`,
            level: normalizedPath,
            streaming: false
          } as StandardActionResponse;
        }
      } catch (_e) {
        // Fallback to console logic
      }

      try {
        // Best-effort existence check using the Automation Bridge when available.
        try {
          const automation = this.getAutomationBridge();
          if (automation && typeof automation.sendAutomationRequest === 'function' && automation.isConnected()) {
            const targetPath = (params.levelPath ?? '').toString();
            const existsResp = await automation.sendAutomationRequest('execute_editor_function', {
              functionName: 'ASSET_EXISTS_SIMPLE',
              path: targetPath
            }, {
              timeoutMs: 5000
            }) as Record<string, unknown>;
            const result = (existsResp?.result ?? existsResp ?? {}) as Record<string, unknown>;
            const exists = Boolean(result.exists);

            if (!exists) {
              const message = typeof result.message === 'string' ? result.message : 'Level not found';
              return {
                success: false,
                error: 'not_found',
                message,
                level: normalizedPath
              } as StandardActionResponse;
            }
          }
        } catch {
          // If the existence check fails for any reason, fall back to the console command path below.
        }

        await this.bridge.executeConsoleCommand(`Open ${params.levelPath}`);
        this.setCurrentLevel(normalizedPath);
        this.mutateRecord(normalizedPath, {
          streaming: false,
          loaded: true,
          visible: true
        });
        return {
          success: true,
          message: `Level loaded: ${params.levelPath}`,
          level: normalizedPath,
          streaming: false
        } as StandardActionResponse;
      } catch (err) {
        return {
          success: false,
          error: `Failed to load level: ${err}`,
          level: normalizedPath
        };
      }
    }
  }

  async saveLevel(params: {
    levelName?: string;
    savePath?: string;
  }): Promise<StandardActionResponse> {
    try {
      if (params.savePath && !params.savePath.startsWith('/Game/')) {
        throw new Error(`Invalid save path: ${params.savePath}`);
      }

      const action = params.savePath ? 'save_level_as' : 'save';
      const payload: Record<string, unknown> = { action };
      if (params.savePath) {
        payload.savePath = params.savePath;
      }

      const response = await this.sendAutomationRequest<LevelResponse>('manage_level', payload, {
        timeoutMs: DEFAULT_ASSET_OP_TIMEOUT_MS
      });

      if (response.success === false) {
        return { success: false, error: response.error || response.message || 'Failed to save level' };
      }

      const result: Record<string, unknown> = {
        ...response,
        success: true,
        message: response.message || 'Level saved'
      };

      if (response.skipped) {
        result.skipped = response.skipped;
      }
      if (response.reason) {
        result.reason = response.reason;
      }
      if (response.warnings) {
        result.warnings = response.warnings;
      }
      if (response.details) {
        result.details = response.details;
      }

      return result as StandardActionResponse;
    } catch (error) {
      return { success: false, error: `Failed to save level: ${error instanceof Error ? error.message : String(error)}` };
    }
  }

  async createLevel(params: {
    levelName: string;
    template?: 'Empty' | 'Default' | 'VR' | 'TimeOfDay';
    savePath?: string;
  }): Promise<StandardActionResponse> {
    const basePath = params.savePath || '/Game/Maps';
    const isPartitioned = true; // default to World Partition for UE5
    const fullPath = `${basePath}/${params.levelName}`;

    try {
      const response = await this.sendAutomationRequest<LevelResponse>('create_new_level', {
        levelPath: fullPath,
        useWorldPartition: isPartitioned
      }, {
        timeoutMs: DEFAULT_ASSET_OP_TIMEOUT_MS
      });

      if (response.success === false) {
        return {
          success: false,
          error: response.error || response.message || 'Failed to create level',
          path: fullPath,
          partitioned: isPartitioned
        } as StandardActionResponse;
      }

      const result: Record<string, unknown> = {
        ...response,
        success: true,
        message: response.message || 'Level created',
        path: response.levelPath || fullPath,
        packagePath: response.packagePath ?? fullPath,
        objectPath: response.objectPath,
        partitioned: isPartitioned
      };

      if (response.warnings) {
        result.warnings = response.warnings;
      }
      if (response.details) {
        result.details = response.details;
      }

      this.ensureRecord(fullPath, {
        name: params.levelName,
        partitioned: isPartitioned,
        loaded: true,
        visible: true,
        createdAt: Date.now()
      });

      return result as StandardActionResponse;
    } catch (error) {
      return {
        success: false,
        error: `Failed to create level: ${error instanceof Error ? error.message : String(error)}`,
        path: fullPath,
        partitioned: isPartitioned
      } as StandardActionResponse;
    }
  }

  async addSubLevel(params: {
    parentLevel?: string;
    subLevelPath: string;
    streamingMethod?: 'Blueprint' | 'AlwaysLoaded';
  }): Promise<StandardActionResponse> {
    const parent = params.parentLevel ? this.resolveLevelPath(params.parentLevel) : this.currentLevelPath;
    const sub = this.normalizeLevelPath(params.subLevelPath).path;

    // Use console command as primary method for adding sublevels
    // "WorldComposition" commands or generic "AddLevelToWorld"
    // Since stream_level handles existing sublevels, we just need to ADD it.
    // Console command: 'LevelEditor.AddLevel <Path>' works in editor context mostly, but might be tricky.
    // Falling back to automation request if we have      const sub = this.normalizeLevelPath(params.subLevelPath).path;

    // Ensure path corresponds to what automation expects (Package path usually, but C++ might check file)
    // If C++ FPackageName::DoesPackageExist expects pure package path (e.g. /Game/Map), we are good.
    // But if it's recently created, it might need to receive the full path as verified in createLevel.

    // Attempt automation first (cleaner)
    try {
      let response = await this.sendAutomationRequest<LevelResponse>('manage_level', {
        action: 'add_sublevel',
        levelPath: sub, // Backwards compat
        subLevelPath: sub,
        parentPath: parent,
        streamingMethod: params.streamingMethod
      }, { timeoutMs: DEFAULT_OPERATION_TIMEOUT_MS });

      // Retry with .umap if package not found (Workaround for C++ strictness)
      // Also retry if ADD_FAILED, as UEditorLevelUtils might have failed due to path resolution internally
      if (response && (response.error === 'PACKAGE_NOT_FOUND' || response.error === 'ADD_FAILED') && !sub.endsWith('.umap')) {
        const subWithExt = sub + '.umap';
        response = await this.sendAutomationRequest<LevelResponse>('manage_level', {
          action: 'add_sublevel',
          levelPath: subWithExt,
          subLevelPath: subWithExt,
          parentPath: parent,
          streamingMethod: params.streamingMethod
        }, { timeoutMs: DEFAULT_OPERATION_TIMEOUT_MS });
      }

      if (response.success) {
        this.ensureRecord(sub, { loaded: true, visible: true, streaming: true });
        return response as StandardActionResponse;
      } else if (response.error === 'UNKNOWN_ACTION') {
        // Fallthrough to console fallback if action not implemented
      } else {
        // Return actual error if it's something else (e.g. execution failed)
        return response as StandardActionResponse;
      }
    } catch (_e: unknown) {
      // If connection failed, might fallback. But if we got a response, respect it.
    }

    // Console fallback
    // Try using LevelEditor.AddLevel command which is available in Editor context
    const consoleResponse = await this.sendAutomationRequest<LevelResponse>('console_command', {
      command: `LevelEditor.AddLevel ${sub}`
    });

    if (consoleResponse.success) {
      this.ensureRecord(sub, { loaded: true, visible: true, streaming: true });
      return {
        success: true,
        message: `Sublevel added via console: ${sub}`,
        data: { method: 'console' }
      } as StandardActionResponse;
    }

    return {
      success: false,
      error: 'Fallbacks failed',
      // Return the last relevant error + console error
      message: 'Failed to add sublevel via automation or console.',
      details: { consoleError: consoleResponse }
    } as StandardActionResponse;
  }

  async streamLevel(params: {
    levelPath?: string;
    levelName?: string;
    shouldBeLoaded: boolean;
    shouldBeVisible?: boolean;
    position?: [number, number, number];
  }): Promise<StandardActionResponse> {
    const rawPath = typeof params.levelPath === 'string' ? params.levelPath.trim() : '';
    const levelPath = rawPath.length > 0 ? rawPath : undefined;
    const providedName = typeof params.levelName === 'string' ? params.levelName.trim() : '';
    const derivedName = providedName.length > 0
      ? providedName
      : (levelPath ? levelPath.split('/').filter(Boolean).pop() ?? '' : '');
    const levelName = derivedName.length > 0 ? derivedName : undefined;
    const shouldBeVisible = params.shouldBeVisible ?? params.shouldBeLoaded;

    try {
      const response = await this.sendAutomationRequest<LevelResponse>('stream_level', {
        levelPath: levelPath || '',
        levelName: levelName || '',
        shouldBeLoaded: params.shouldBeLoaded,
        shouldBeVisible
      }, {
        timeoutMs: DEFAULT_ASSET_OP_TIMEOUT_MS
      });

      if (response.success === false) {
        const errorCode = typeof response.error === 'string' ? response.error : '';
        const isExecFailed = errorCode.toLowerCase() === 'exec_failed';

        if (isExecFailed) {
          const handledResult: Record<string, unknown> = {
            success: true,
            handled: true,
            message: response.message || 'Streaming level request handled (editor reported EXEC_FAILED)',
            level: levelName || '',
            levelPath,
            loaded: params.shouldBeLoaded,
            visible: shouldBeVisible
          };

          if (response.warnings) {
            handledResult.warnings = response.warnings;
          }
          if (response.details) {
            handledResult.details = response.details;
          }

          return handledResult as StandardActionResponse;
        }

        return {
          success: false,
          error: response.error || response.message || 'Streaming level update failed',
          level: levelName || '',
          levelPath: levelPath,
          loaded: params.shouldBeLoaded,
          visible: shouldBeVisible
        } as StandardActionResponse;
      }

      const result: Record<string, unknown> = {
        success: true,
        message: response.message || 'Streaming level updated',
        level: levelName || '',
        levelPath,
        loaded: params.shouldBeLoaded,
        visible: shouldBeVisible
      };

      if (response.warnings) {
        result.warnings = response.warnings;
      }
      if (response.details) {
        result.details = response.details;
      }

      return result as StandardActionResponse;
    } catch (_error) {
      // Fallback to console command
      const levelIdentifier = levelName ?? levelPath ?? '';
      const simpleName = levelIdentifier.split('/').filter(Boolean).pop() || levelIdentifier;
      const loadCmd = params.shouldBeLoaded ? 'Load' : 'Unload';
      const visCmd = shouldBeVisible ? 'Show' : 'Hide';
      const command = `StreamLevel ${simpleName} ${loadCmd} ${visCmd}`;
      return this.bridge.executeConsoleCommand(command);
    }
  }

  async setupWorldComposition(params: {
    enableComposition: boolean;
    tileSize?: number;
    distanceStreaming?: boolean;
    streamingDistance?: number;
  }): Promise<StandardActionResponse> {
    const commands: string[] = [];

    if (params.enableComposition) {
      commands.push('EnableWorldComposition');
      if (params.tileSize) {
        commands.push(`SetWorldTileSize ${params.tileSize}`);
      }
      if (params.distanceStreaming) {
        commands.push(`EnableDistanceStreaming ${params.streamingDistance || 5000}`);
      }
    } else {
      commands.push('DisableWorldComposition');
    }

    await this.bridge.executeConsoleCommands(commands);

    return { success: true, message: 'World composition configured' };
  }

  async editLevelBlueprint(params: {
    eventType: 'BeginPlay' | 'EndPlay' | 'Tick' | 'Custom';
    customEventName?: string;
    nodes?: Array<{
      nodeType: string;
      position: [number, number];
      connections?: string[];
    }>;
  }): Promise<StandardActionResponse> {
    const command = `OpenLevelBlueprint ${params.eventType}`;
    return this.bridge.executeConsoleCommand(command);
  }

  async createSubLevel(params: {
    name: string;
    type: 'Persistent' | 'Streaming' | 'Lighting' | 'Gameplay';
    parent?: string;
  }): Promise<StandardActionResponse> {
    const command = `CreateSubLevel ${params.name} ${params.type} ${params.parent || 'None'}`;
    return this.bridge.executeConsoleCommand(command);
  }

  async setWorldSettings(params: {
    gravity?: number;
    worldScale?: number;
    gameMode?: string;
    defaultPawn?: string;
    killZ?: number;
  }): Promise<StandardActionResponse> {
    const commands: string[] = [];

    if (params.gravity !== undefined) {
      commands.push(`SetWorldGravity ${params.gravity}`);
    }
    if (params.worldScale !== undefined) {
      commands.push(`SetWorldToMeters ${params.worldScale}`);
    }
    if (params.gameMode) {
      commands.push(`SetGameMode ${params.gameMode}`);
    }
    if (params.defaultPawn) {
      commands.push(`SetDefaultPawn ${params.defaultPawn}`);
    }
    if (params.killZ !== undefined) {
      commands.push(`SetKillZ ${params.killZ}`);
    }

    await this.bridge.executeConsoleCommands(commands);

    return { success: true, message: 'World settings updated' };
  }

  async setLevelBounds(params: {
    min: [number, number, number];
    max: [number, number, number];
  }): Promise<StandardActionResponse> {
    const command = `SetLevelBounds ${params.min.join(',')} ${params.max.join(',')}`;
    return this.bridge.executeConsoleCommand(command);
  }

  async buildNavMesh(params: {
    rebuildAll?: boolean;
    selectedOnly?: boolean;
  }): Promise<StandardActionResponse> {
    try {
      const response = await this.sendAutomationRequest<LevelResponse>('build_navigation_mesh', {
        rebuildAll: params.rebuildAll ?? false,
        selectedOnly: params.selectedOnly ?? false
      }, {
        timeoutMs: 120000
      });

      if (response.success === false) {
        return {
          success: false,
          error: response.error || response.message || 'Failed to build navigation'
        };
      }

      const result: Record<string, unknown> = {
        success: true,
        message: response.message || (params.rebuildAll ? 'Navigation rebuild started' : 'Navigation update started')
      };

      if (params.rebuildAll !== undefined) {
        result.rebuildAll = params.rebuildAll;
      }
      if (params.selectedOnly !== undefined) {
        result.selectedOnly = params.selectedOnly;
      }
      if (response.selectionCount !== undefined) {
        result.selectionCount = response.selectionCount;
      }
      if (response.warnings) {
        result.warnings = response.warnings;
      }
      if (response.details) {
        result.details = response.details;
      }

      return result as StandardActionResponse;
    } catch (error) {
      return {
        success: false,
        error: `Navigation build not available: ${error instanceof Error ? error.message : String(error)}. Please ensure a NavMeshBoundsVolume exists in the level.`
      };
    }
  }

  async setLevelVisibility(params: {
    levelName: string;
    visible: boolean;
  }): Promise<StandardActionResponse> {
    const command = `SetLevelVisibility ${params.levelName} ${params.visible}`;
    return this.bridge.executeConsoleCommand(command);
  }

  async setWorldOrigin(params: {
    location: [number, number, number];
  }): Promise<StandardActionResponse> {
    const command = `SetWorldOriginLocation ${params.location.join(' ')}`;
    return this.bridge.executeConsoleCommand(command);
  }

  async createStreamingVolume(params: {
    levelName: string;
    position: [number, number, number];
    size: [number, number, number];
    streamingDistance?: number;
  }): Promise<StandardActionResponse> {
    const command = `CreateStreamingVolume ${params.levelName} ${params.position.join(' ')} ${params.size.join(' ')} ${params.streamingDistance || 0}`;
    return this.bridge.executeConsoleCommand(command);
  }

  async setLevelLOD(params: {
    levelName: string;
    lodLevel: number;
    distance: number;
  }): Promise<StandardActionResponse> {
    const command = `SetLevelLOD ${params.levelName} ${params.lodLevel} ${params.distance}`;
    return this.bridge.executeConsoleCommand(command);
  }
}
