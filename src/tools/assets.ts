import { BaseTool } from './base-tool.js';
import { IAssetTools, StandardActionResponse, SourceControlState } from '../types/tool-interfaces.js';
import { wasmIntegration } from '../wasm/index.js';
import { Logger } from '../utils/logger.js';
import { AssetResponse } from '../types/automation-responses.js';
import { sanitizePath } from '../utils/path-security.js';
import {
  DEFAULT_ASSET_OP_TIMEOUT_MS,
  EXTENDED_ASSET_OP_TIMEOUT_MS,
  LONG_RUNNING_OP_TIMEOUT_MS
} from '../constants.js';

const log = new Logger('AssetTools');

export class AssetTools extends BaseTool implements IAssetTools {
  private normalizeAssetPath(path: string): string {
    if (!path) return '';
    let normalized = path.replace(/\\/g, '/').trim();

    // Handle typical prefixes if missing leading slash
    if (!normalized.startsWith('/')) {
      if (normalized.startsWith('Game/')) normalized = '/' + normalized;
      else if (normalized.startsWith('Engine/')) normalized = '/' + normalized;
      else if (normalized.startsWith('Script/')) normalized = '/' + normalized;
      // Default to Game content if no known prefix
      else normalized = '/Game/' + normalized;
    }

    // Remove double slashes just in case
    normalized = normalized.replace(/\/+/g, '/');

    // Security check
    return sanitizePath(normalized);
  }

  async importAsset(params: { sourcePath: string; destinationPath: string; overwrite?: boolean; save?: boolean }): Promise<StandardActionResponse> {
    const res = await this.sendRequest<AssetResponse>('manage_asset', {
      ...params,
      subAction: 'import'
    }, 'manage_asset', { timeoutMs: EXTENDED_ASSET_OP_TIMEOUT_MS });
    if (res && res.success) {
      return { ...res, asset: this.normalizeAssetPath(params.destinationPath), source: params.sourcePath };
    }
    return res;
  }

  async duplicateAsset(params: { sourcePath: string; destinationPath: string; overwrite?: boolean }): Promise<StandardActionResponse> {
    const sourcePath = this.normalizeAssetPath(params.sourcePath);
    const destinationPath = this.normalizeAssetPath(params.destinationPath);

    const res = await this.sendRequest<AssetResponse>('manage_asset', {
      sourcePath,
      destinationPath,
      overwrite: params.overwrite ?? false,
      subAction: 'duplicate'
    }, 'manage_asset', { timeoutMs: DEFAULT_ASSET_OP_TIMEOUT_MS });
    if (res && res.success) {
      return { ...res, asset: destinationPath, source: sourcePath };
    }
    return res;
  }

  async renameAsset(params: { sourcePath: string; destinationPath: string }): Promise<StandardActionResponse> {
    const sourcePath = this.normalizeAssetPath(params.sourcePath);
    const destinationPath = this.normalizeAssetPath(params.destinationPath);

    const res = await this.sendRequest<AssetResponse>('manage_asset', {
      sourcePath,
      destinationPath,
      subAction: 'rename'
    }, 'manage_asset', { timeoutMs: DEFAULT_ASSET_OP_TIMEOUT_MS });
    if (res && res.success) {
      return { ...res, asset: destinationPath, oldName: sourcePath };
    }
    return res;
  }

  async moveAsset(params: { sourcePath: string; destinationPath: string }): Promise<StandardActionResponse> {
    const sourcePath = this.normalizeAssetPath(params.sourcePath);
    const destinationPath = this.normalizeAssetPath(params.destinationPath);

    const res = await this.sendRequest<AssetResponse>('manage_asset', {
      sourcePath,
      destinationPath,
      subAction: 'move'
    }, 'manage_asset', { timeoutMs: DEFAULT_ASSET_OP_TIMEOUT_MS });
    if (res && res.success) {
      return { ...res, asset: destinationPath, from: sourcePath };
    }
    return res;
  }

  async findByTag(params: { tag: string; value?: string }): Promise<StandardActionResponse> {
    // tag searches don't usually involve paths, but if they did we'd normalize.
    // preserving existing logic for findByTag as it takes 'tag' and 'value'.
    return this.sendRequest<AssetResponse>('asset_query', {
      ...params,
      subAction: 'find_by_tag'
    }, 'asset_query', { timeoutMs: DEFAULT_ASSET_OP_TIMEOUT_MS });
  }

  async deleteAssets(params: { paths: string[]; fixupRedirectors?: boolean; timeoutMs?: number }): Promise<StandardActionResponse> {
    const assetPaths = (Array.isArray(params.paths) ? params.paths : [])
      .map(p => this.normalizeAssetPath(p));

    return this.sendRequest<AssetResponse>('manage_asset', {
      assetPaths,
      fixupRedirectors: params.fixupRedirectors,
      subAction: 'delete'
    }, 'manage_asset', { timeoutMs: params.timeoutMs || EXTENDED_ASSET_OP_TIMEOUT_MS });
  }

  async searchAssets(params: { classNames?: string[]; packagePaths?: string[]; recursivePaths?: boolean; recursiveClasses?: boolean; limit?: number }): Promise<StandardActionResponse> {
    // Normalize package paths if provided
    const packagePaths = params.packagePaths
      ? params.packagePaths.map(p => this.normalizeAssetPath(p))
      : ['/Game'];

    // Route via asset_query action with subAction 'search_assets'
    const response = await this.sendRequest<AssetResponse>('asset_query', {
      ...params,
      packagePaths,
      subAction: 'search_assets'
    }, 'asset_query', { timeoutMs: DEFAULT_ASSET_OP_TIMEOUT_MS });

    if (!response.success) {
      const errorMsg = typeof response.error === 'string' ? response.error : JSON.stringify(response.error);
      return { success: false, error: errorMsg || 'Failed to search assets' };
    }

    const assetsRaw = response.assets || response.data || response.result;
    const assets = Array.isArray(assetsRaw) ? assetsRaw : [];

    return {
      success: true,
      message: `Found ${assets.length} assets`,
      assets,
      count: assets.length
    };
  }

  async saveAsset(assetPath: string): Promise<StandardActionResponse> {
    const normalizedPath = this.normalizeAssetPath(assetPath);
    try {
      // Try Automation Bridge first
      const bridge = this.getAutomationBridge();
      if (bridge && typeof bridge.sendAutomationRequest === 'function') {
        try {
          const response: any = await bridge.sendAutomationRequest(
            'manage_asset',
            { assetPath: normalizedPath, subAction: 'save_asset' }, // 'save_asset' isn't explicitly in HandleAssetAction but usually falls back or Editor handles it?
            // Wait, HandleAssetAction does NOT have 'save'.
            // But 'execute_editor_function' usually handles SAVE_ASSET.
            // Let's check fallback. The original code tried 'save_asset' command which likely failed.
            // Actually, keep safe fallback to 'executeEditorFunction'.
            // But if we want to add save support, we should assume 'save_asset' command failed implies we need fallback.
            // Let's stick to the existing fallback logic but maybe fix the command if known?
            // Since 'save_asset' is not in Subsystem.cpp, it fails.
            // Let's rely on executeEditorFunction below.
            { timeoutMs: DEFAULT_ASSET_OP_TIMEOUT_MS }
          );

          if (response && response.success !== false) {
            return {
              success: true,
              saved: response.saved ?? true,
              message: response.message || 'Asset saved',
              ...response
            };
          }
        } catch (primaryError) {
          // Log the primary method failure before trying fallback
          // This helps debugging when both methods fail
          log.debug('saveAsset primary method failed, trying fallback', primaryError);
          // Fall through to executeEditorFunction fallback
        }
      }

      // Fallback to executeEditorFunction
      const res = await this.bridge.executeEditorFunction('SAVE_ASSET', { path: normalizedPath });
      if (res && typeof res === 'object' && (res.success === true || (res.result && res.result.success === true))) {
        const saved = Boolean(res.saved ?? (res.result && res.result.saved));
        return { success: true, saved, ...res, ...(res.result || {}) };
      }

      return { success: false, error: (res as any)?.error ?? 'Failed to save asset' };
    } catch (err) {
      return { success: false, error: `Failed to save asset: ${err} ` };
    }
  }

  async createFolder(folderPath: string): Promise<StandardActionResponse> {
    // Folders are paths too
    const path = this.normalizeAssetPath(folderPath);
    return this.sendRequest<AssetResponse>('manage_asset', {
      path,
      subAction: 'create_folder'
    }, 'manage_asset', { timeoutMs: DEFAULT_ASSET_OP_TIMEOUT_MS });
  }

  async getDependencies(params: { assetPath: string; recursive?: boolean }): Promise<StandardActionResponse> {
    // get_dependencies is typically an asset query or managed asset action?
    // HandleAssetAction has 'get_dependencies' dispatch.
    return this.sendRequest<AssetResponse>('manage_asset', {
      ...params,
      assetPath: this.normalizeAssetPath(params.assetPath),
      subAction: 'get_dependencies'
    }, 'manage_asset');
  }

  async getSourceControlState(params: { assetPath: string }): Promise<SourceControlState | StandardActionResponse> {
    // Source control state usually via 'asset_query' or 'manage_asset'?
    // It's not in HandleAssetAction explicitly, maybe 'asset_query' subAction?
    // Let's check AssetQueryHandlers.cpp or AssetWorkflowHandlers.cpp dispatch.
    // Assuming 'asset_query' supports it (original code used asset_query).
    return this.sendRequest<AssetResponse>('asset_query', {
      ...params,
      assetPath: this.normalizeAssetPath(params.assetPath),
      subAction: 'get_source_control_state'
    }, 'asset_query');
  }

  async getMetadata(params: { assetPath: string }): Promise<StandardActionResponse> {
    const response = await this.sendRequest<AssetResponse>('manage_asset', {
      ...params,
      assetPath: this.normalizeAssetPath(params.assetPath),
      subAction: 'get_metadata'
    }, 'manage_asset');

    // BaseTool unwraps the result, so 'response' is likely the payload itself.
    // However, if the result was null, 'response' might be the wrapper.
    // We handle both cases to be robust.
    const resultObj = (response.result || response) as Record<string, unknown>;
    return {
      success: true,
      message: 'Metadata retrieved',
      ...resultObj
    };
  }

  async analyzeGraph(params: { assetPath: string; maxDepth?: number }): Promise<StandardActionResponse> {
    const maxDepth = params.maxDepth ?? 3;
    const assetPath = this.normalizeAssetPath(params.assetPath);

    try {
      // Offload the heavy graph traversal to C++
      const response: any = await this.sendRequest<AssetResponse>('manage_asset', {
        assetPath,
        maxDepth,
        subAction: 'get_asset_graph'
      }, 'manage_asset', { timeoutMs: DEFAULT_ASSET_OP_TIMEOUT_MS });

      if (!response.success || !response.graph) {
        return { success: false, error: response.error || 'Failed to retrieve asset graph from engine' };
      }

      const graph: Record<string, string[]> = {};
      // Convert the JSON object (Record<string, any[]>) to string[]
      for (const [key, value] of Object.entries(response.graph)) {
        if (Array.isArray(value)) {
          graph[key] = value.map(v => String(v));
        }
      }

      // Use WASM for analysis on the constructed graph
      const base = await wasmIntegration.resolveDependencies(
        assetPath,
        graph,
        { maxDepth }
      );

      const depth = await wasmIntegration.calculateDependencyDepth(
        assetPath,
        graph,
        { maxDepth }
      );

      const circularDependencies = await wasmIntegration.findCircularDependencies(
        graph,
        { maxDepth }
      );

      const topologicalOrder = await wasmIntegration.topologicalSort(graph);

      const dependenciesList = Array.isArray((base as any).dependencies)
        ? (base as any).dependencies as any[]
        : [];

      const totalDependencyCount =
        (base as any).totalDependencyCount ??
        (base as any).total_dependency_count ??
        dependenciesList.length;

      const analysis = {
        asset: (base as any).asset ?? assetPath,
        dependencies: dependenciesList,
        totalDependencyCount,
        requestedMaxDepth: maxDepth,
        maxDepthUsed: depth,
        circularDependencies,
        topologicalOrder,
        stats: {
          nodeCount: dependenciesList.length,
          leafCount: dependenciesList.filter((d: any) => !d.dependencies || d.dependencies.length === 0).length
        }
      };

      return {
        success: true,
        message: 'graph analyzed',
        analysis
      };
    } catch (e: unknown) {
      return { success: false, error: `Analysis failed: ${e instanceof Error ? e.message : String(e)} ` };
    }
  }

  async createThumbnail(params: { assetPath: string; width?: number; height?: number }): Promise<StandardActionResponse> {
    return this.sendRequest<AssetResponse>('manage_asset', {
      ...params,
      assetPath: this.normalizeAssetPath(params.assetPath),
      subAction: 'generate_thumbnail'
    }, 'manage_asset', { timeoutMs: DEFAULT_ASSET_OP_TIMEOUT_MS });
  }

  async setTags(params: { assetPath: string; tags: string[] }): Promise<StandardActionResponse> {
    return this.sendRequest<AssetResponse>('manage_asset', {
      ...params,
      assetPath: this.normalizeAssetPath(params.assetPath),
      subAction: 'set_tags'
    }, 'manage_asset', { timeoutMs: DEFAULT_ASSET_OP_TIMEOUT_MS });
  }

  async generateReport(params: { directory: string; reportType?: string; outputPath?: string }): Promise<StandardActionResponse> {
    return this.sendRequest<AssetResponse>('manage_asset', {
      ...params,
      directory: this.normalizeAssetPath(params.directory),
      subAction: 'generate_report'
    }, 'manage_asset', { timeoutMs: LONG_RUNNING_OP_TIMEOUT_MS });
  }

  async validate(params: { assetPath: string }): Promise<StandardActionResponse> {
    return this.sendRequest<AssetResponse>('manage_asset', {
      ...params,
      assetPath: this.normalizeAssetPath(params.assetPath),
      subAction: 'validate'
    }, 'manage_asset', { timeoutMs: LONG_RUNNING_OP_TIMEOUT_MS });
  }

  async generateLODs(params: { assetPath: string; lodCount: number }): Promise<StandardActionResponse> {
    const assetPath = this.normalizeAssetPath(String(params.assetPath ?? '').trim());
    const lodCountRaw = Number(params.lodCount);

    if (!assetPath) {
      return { success: false, error: 'assetPath is required' };
    }
    if (!Number.isFinite(lodCountRaw) || lodCountRaw <= 0) {
      return { success: false, error: 'lodCount must be a positive number' };
    }
    const lodCount = Math.floor(lodCountRaw);

    try {
      const automation = this.getAutomationBridge();
      const response: any = await automation.sendAutomationRequest('manage_asset', {
        assetPaths: [assetPath],
        numLODs: lodCount,
        subAction: 'generate_lods'
      }, { timeoutMs: EXTENDED_ASSET_OP_TIMEOUT_MS });

      if (!response || response.success === false) {
        return {
          success: false,
          error: response?.error || response?.message || 'Failed to generate LODs',
          details: response?.result
        };
      }

      const result = (response.result && typeof response.result === 'object') ? response.result as Record<string, unknown> : {};

      return {
        success: true,
        message: response.message || 'LODs generated successfully',
        assetPath: (result.assetPath as string) ?? assetPath,
        lodCount: (result.lodCount as number) ?? lodCount,
        ...result
      };
    } catch (error) {
      return {
        success: false,
        error: `Failed to generate LODs: ${error instanceof Error ? error.message : String(error)} `
      };
    }
  }
}
