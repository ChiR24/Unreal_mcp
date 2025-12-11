import { BaseTool } from './base-tool.js';
import { ILevelTools } from '../types/tool-interfaces.js';

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
  private listCache?: { result: { success: true; message: string; count: number; levels: any[] }; timestamp: number };
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

  async listLevels() {
    // Try to get actual levels from UE via automation bridge
    try {
      const response = await this.sendAutomationRequest('list_levels', {}, {
        timeoutMs: 10000
      });

      if (response && response.success !== false) {
        const result: Record<string, unknown> = {
          success: true,
          message: 'Levels listed from Unreal Engine',
          currentMap: response.currentMap,
          currentMapPath: response.currentMapPath,
          currentWorldLevels: response.currentWorldLevels || [],
          currentWorldLevelCount: response.currentWorldLevelCount || 0,
          allMaps: response.allMaps || [],
          allMapsCount: response.allMapsCount || 0,
          ...response
        };

        // Also include managed levels for backwards compatibility
        const managed = this.listManagedLevels();
        result.managedLevels = managed.levels;
        result.managedLevelCount = managed.count;

        return result;
      }
    } catch {
      // Fall back to managed levels if automation bridge fails
    }

    // Fallback to locally managed levels
    return this.listManagedLevels();
  }

  async getLevelSummary(levelPath?: string) {
    const resolved = this.resolveLevelPath(levelPath);
    if (!resolved) {
      return { success: false, error: 'No level specified' };
    }
    return this.summarizeLevel(resolved);
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

  async exportLevel(params: { levelPath?: string; exportPath: string; note?: string }) {
    const resolved = this.resolveLevelPath(params.levelPath);
    if (!resolved) {
      return { success: false, error: 'No level specified for export' };
    }

    try {
      const res = await this.sendAutomationRequest('manage_level', {
        action: 'export_level',
        levelPath: resolved,
        exportPath: params.exportPath
      }, { timeoutMs: 60000 });

      return {
        success: true,
        message: `Level exported to ${params.exportPath}`,
        levelPath: resolved,
        exportPath: params.exportPath,
        details: res
      };
    } catch (e: any) {
      return { success: false, error: `Export failed: ${e.message}` };
    }
  }

  async importLevel(params: { packagePath: string; destinationPath?: string; streaming?: boolean }) {
    const destination = params.destinationPath
      ? this.normalizeLevelPath(params.destinationPath)
      : this.normalizeLevelPath(`/Game/Maps/Imported_${Math.floor(Date.now() / 1000)}`);

    try {
      const res = await this.sendAutomationRequest('manage_level', {
        action: 'import_level',
        packagePath: params.packagePath,
        destinationPath: destination.path
      }, { timeoutMs: 60000 });

      return {
        success: true,
        message: `Level imported to ${destination.path}`,
        levelPath: destination.path,
        partitioned: true,
        streaming: Boolean(params.streaming),
        details: res
      };
    } catch (e: any) {
      return { success: false, error: `Import failed: ${e.message}` };
    }
  }

  async saveLevelAs(params: { sourcePath?: string; targetPath: string }) {
    const source = this.resolveLevelPath(params.sourcePath);
    const target = this.normalizeLevelPath(params.targetPath);

    // Delegate to automation bridge
    try {
      const response = await this.sendAutomationRequest('manage_level', {
        action: 'save_level_as',
        savePath: target.path
      }, {
        timeoutMs: 60000
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
      };
    } catch (error) {
      return { success: false, error: `Failed to save level as: ${error instanceof Error ? error.message : String(error)}` };
    }
  }

  async deleteLevels(params: { levelPaths: string[] }) {
    const removed: string[] = [];
    for (const path of params.levelPaths) {
      const normalized = this.normalizeLevelPath(path).path;
      if (this.managedLevels.has(normalized)) {
        this.removeRecord(normalized);
        removed.push(normalized);
      }
    }

    return {
      success: true,
      message: removed.length ? `Deleted ${removed.length} managed level(s)` : 'No managed levels removed',
      removed
    };
  }

  async loadLevel(params: {
    levelPath: string;
    streaming?: boolean;
    position?: [number, number, number];
  }) {
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
        };
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
        const response = await this.sendAutomationRequest('manage_level', {
          action: 'load',
          levelPath: params.levelPath
        }, { timeoutMs: 30000 });

        if (response.success) {
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
            streaming: false,
            ...response
          };
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
            const existsResp: any = await automation.sendAutomationRequest('execute_editor_function', {
              functionName: 'ASSET_EXISTS_SIMPLE',
              path: targetPath
            }, {
              timeoutMs: 5000
            });
            const result = existsResp?.result ?? existsResp ?? {};
            const exists = Boolean(result.exists);

            if (!exists) {
              const message = typeof result.message === 'string' ? result.message : 'Level not found';
              return {
                success: false,
                error: 'not_found',
                message,
                level: normalizedPath
              };
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
        };
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
  }) {
    try {
      if (params.savePath && !params.savePath.startsWith('/Game/')) {
        throw new Error(`Invalid save path: ${params.savePath}`);
      }

      const action = params.savePath ? 'save_level_as' : 'save';
      const payload: Record<string, unknown> = { action };
      if (params.savePath) {
        payload.savePath = params.savePath;
      }

      const response = await this.sendAutomationRequest('manage_level', payload, {
        timeoutMs: 60000
      });

      if (response.success === false) {
        return { success: false, error: response.error || response.message || 'Failed to save level' };
      }

      const result: Record<string, unknown> = {
        success: true,
        message: response.message || 'Level saved',
        ...response
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

      return result;
    } catch (error) {
      return { success: false, error: `Failed to save level: ${error instanceof Error ? error.message : String(error)}` };
    }
  }

  async createLevel(params: {
    levelName: string;
    template?: 'Empty' | 'Default' | 'VR' | 'TimeOfDay';
    savePath?: string;
  }) {
    const basePath = params.savePath || '/Game/Maps';
    const isPartitioned = true; // default to World Partition for UE5
    const fullPath = `${basePath}/${params.levelName}`;

    try {
      const response = await this.sendAutomationRequest('create_new_level', {
        levelPath: fullPath,
        useWorldPartition: isPartitioned
      }, {
        timeoutMs: 60000
      });

      if (response.success === false) {
        return {
          success: false,
          error: response.error || response.message || 'Failed to create level',
          path: fullPath,
          partitioned: isPartitioned
        };
      }

      const result: Record<string, unknown> = {
        success: true,
        message: response.message || 'Level created',
        path: response.levelPath || fullPath,
        packagePath: response.packagePath ?? fullPath,
        objectPath: response.objectPath,
        partitioned: isPartitioned,
        ...response
      };

      if (response.warnings) {
        result.warnings = response.warnings;
      }
      if (response.details) {
        result.details = response.details;
      }

      return result;
    } catch (error) {
      return {
        success: false,
        error: `Failed to create level: ${error instanceof Error ? error.message : String(error)}`,
        path: fullPath,
        partitioned: isPartitioned
      };
    }
  }

  async streamLevel(params: {
    levelPath?: string;
    levelName?: string;
    shouldBeLoaded: boolean;
    shouldBeVisible?: boolean;
    position?: [number, number, number];
  }) {
    const rawPath = typeof params.levelPath === 'string' ? params.levelPath.trim() : '';
    const levelPath = rawPath.length > 0 ? rawPath : undefined;
    const providedName = typeof params.levelName === 'string' ? params.levelName.trim() : '';
    const derivedName = providedName.length > 0
      ? providedName
      : (levelPath ? levelPath.split('/').filter(Boolean).pop() ?? '' : '');
    const levelName = derivedName.length > 0 ? derivedName : undefined;
    const shouldBeVisible = params.shouldBeVisible ?? params.shouldBeLoaded;

    try {
      const response = await this.sendAutomationRequest('stream_level', {
        levelPath: levelPath || '',
        levelName: levelName || '',
        shouldBeLoaded: params.shouldBeLoaded,
        shouldBeVisible
      }, {
        timeoutMs: 60000
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

          return handledResult;
        }

        return {
          success: false,
          error: response.error || response.message || 'Streaming level update failed',
          level: levelName || '',
          levelPath: levelPath,
          loaded: params.shouldBeLoaded,
          visible: shouldBeVisible
        };
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

      return result;
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
  }) {
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
  }) {
    const command = `OpenLevelBlueprint ${params.eventType}`;
    return this.bridge.executeConsoleCommand(command);
  }

  async createSubLevel(params: {
    name: string;
    type: 'Persistent' | 'Streaming' | 'Lighting' | 'Gameplay';
    parent?: string;
  }) {
    const command = `CreateSubLevel ${params.name} ${params.type} ${params.parent || 'None'}`;
    return this.bridge.executeConsoleCommand(command);
  }

  async setWorldSettings(params: {
    gravity?: number;
    worldScale?: number;
    gameMode?: string;
    defaultPawn?: string;
    killZ?: number;
  }) {
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
  }) {
    const command = `SetLevelBounds ${params.min.join(',')} ${params.max.join(',')}`;
    return this.bridge.executeConsoleCommand(command);
  }

  async buildNavMesh(params: {
    rebuildAll?: boolean;
    selectedOnly?: boolean;
  }) {
    try {
      const response = await this.sendAutomationRequest('build_navigation_mesh', {
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

      return result;
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
  }) {
    const command = `SetLevelVisibility ${params.levelName} ${params.visible}`;
    return this.bridge.executeConsoleCommand(command);
  }

  async setWorldOrigin(params: {
    location: [number, number, number];
  }) {
    const command = `SetWorldOriginLocation ${params.location.join(' ')}`;
    return this.bridge.executeConsoleCommand(command);
  }

  async createStreamingVolume(params: {
    levelName: string;
    position: [number, number, number];
    size: [number, number, number];
    streamingDistance?: number;
  }) {
    const command = `CreateStreamingVolume ${params.levelName} ${params.position.join(' ')} ${params.size.join(' ')} ${params.streamingDistance || 0}`;
    return this.bridge.executeConsoleCommand(command);
  }

  async setLevelLOD(params: {
    levelName: string;
    lodLevel: number;
    distance: number;
  }) {
    const command = `SetLevelLOD ${params.levelName} ${params.lodLevel} ${params.distance}`;
    return this.bridge.executeConsoleCommand(command);
  }
}
