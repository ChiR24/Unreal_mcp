
import { BaseTool } from './base-tool.js';
import { IAssetTools } from '../types/tool-interfaces.js';
import { wasmIntegration } from '../wasm/index.js';

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
    return normalized.replace(/\/+/g, '/');
  }

  async importAsset(params: { sourcePath: string; destinationPath: string; overwrite?: boolean; save?: boolean }) {
    const res = await this.sendRequest('import', {
      ...params
    }, 'import', { timeoutMs: 120000 });
    if (res && res.success) {
      return { ...res, asset: this.normalizeAssetPath(params.destinationPath), source: params.sourcePath };
    }
    return res;
  }

  async duplicateAsset(params: { sourcePath: string; destinationPath: string }) {
    const sourcePath = this.normalizeAssetPath(params.sourcePath);
    const destinationPath = this.normalizeAssetPath(params.destinationPath);

    const res = await this.sendRequest('duplicate', {
      sourcePath,
      destinationPath
    }, 'duplicate', { timeoutMs: 60000 });
    if (res && res.success) {
      return { ...res, asset: destinationPath, source: sourcePath };
    }
    return res;
  }

  async renameAsset(params: { sourcePath: string; destinationPath: string }) {
    const sourcePath = this.normalizeAssetPath(params.sourcePath);
    const destinationPath = this.normalizeAssetPath(params.destinationPath);

    const res = await this.sendRequest('rename', {
      sourcePath,
      destinationPath
    }, 'rename', { timeoutMs: 60000 });
    if (res && res.success) {
      return { ...res, asset: destinationPath, oldName: sourcePath };
    }
    return res;
  }

  async moveAsset(params: { sourcePath: string; destinationPath: string }) {
    const sourcePath = this.normalizeAssetPath(params.sourcePath);
    const destinationPath = this.normalizeAssetPath(params.destinationPath);

    const res = await this.sendRequest('move', {
      sourcePath,
      destinationPath
    }, 'move', { timeoutMs: 60000 });
    if (res && res.success) {
      return { ...res, asset: destinationPath, from: sourcePath };
    }
    return res;
  }

  async findByTag(params: { tag: string; value?: string }) {
    // tag searches don't usually involve paths, but if they did we'd normalize.
    // preserving existing logic for findByTag as it takes 'tag' and 'value'.
    return this.sendRequest('asset_query', {
      ...params,
      subAction: 'find_by_tag'
    }, 'asset_query', { timeoutMs: 60000 });
  }

  async deleteAssets(params: { paths: string[]; fixupRedirectors?: boolean; timeoutMs?: number }) {
    const assetPaths = (Array.isArray(params.paths) ? params.paths : [])
      .map(p => this.normalizeAssetPath(p));

    return this.sendRequest('bulk_delete', {
      assetPaths,
      fixupRedirectors: params.fixupRedirectors
    }, 'bulk_delete', { timeoutMs: 120000 });
  }

  async searchAssets(params: { classNames?: string[]; packagePaths?: string[]; recursivePaths?: boolean; recursiveClasses?: boolean; limit?: number }) {
    // Normalize package paths if provided
    const packagePaths = params.packagePaths
      ? params.packagePaths.map(p => this.normalizeAssetPath(p))
      : ['/Game'];

    // Route via asset_query action with subAction 'search_assets'
    const response = await this.sendRequest('asset_query', {
      ...params,
      packagePaths,
      subAction: 'search_assets'
    }, 'asset_query', { timeoutMs: 60000 });

    if (!response.success) {
      const errorMsg = response.error || `Failed to search assets. Raw response: ${JSON.stringify(response)}`;
      return { success: false, error: errorMsg };
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

  async saveAsset(assetPath: string) {
    const normalizedPath = this.normalizeAssetPath(assetPath);
    try {
      // Try Automation Bridge first
      const bridge = this.getAutomationBridge();
      if (bridge && typeof bridge.sendAutomationRequest === 'function') {
        try {
          const response: any = await bridge.sendAutomationRequest(
            'save_asset',
            { assetPath: normalizedPath },
            { timeoutMs: 60000 }
          );

          if (response && response.success !== false) {
            return {
              success: true,
              saved: response.saved ?? true,
              message: response.message || 'Asset saved',
              ...response
            };
          }
        } catch (_err) {
          // Fall through to executeEditorFunction
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
      return { success: false, error: `Failed to save asset: ${err}` };
    }
  }

  async createFolder(folderPath: string) {
    // Folders are paths too
    const path = this.normalizeAssetPath(folderPath);
    return this.sendRequest('create_folder', {
      path
    }, 'create_folder', { timeoutMs: 60000 });
  }

  async getDependencies(params: { assetPath: string; recursive?: boolean }) {
    return this.sendRequest('get_dependencies', {
      ...params,
      assetPath: this.normalizeAssetPath(params.assetPath),
      subAction: 'get_dependencies'
    }, 'get_dependencies');
  }

  async getSourceControlState(params: { assetPath: string }) {
    return this.sendRequest('asset_query', {
      ...params,
      assetPath: this.normalizeAssetPath(params.assetPath),
      subAction: 'get_source_control_state'
    }, 'asset_query');
  }

  async getMetadata(params: { assetPath: string }) {
    const response = await this.sendRequest('get_metadata', {
      ...params,
      assetPath: this.normalizeAssetPath(params.assetPath)
    }, 'get_metadata');

    // BaseTool unwraps the result, so 'response' is likely the payload itself.
    // However, if the result was null, 'response' might be the wrapper.
    // We handle both cases to be robust.
    const resultObj = (response.result || response) as Record<string, any>;
    return {
      success: true,
      message: 'Metadata retrieved',
      ...resultObj
    };
  }

  async analyzeGraph(params: { assetPath: string; maxDepth?: number }) {
    const maxDepth = params.maxDepth ?? 3;
    const assetPath = this.normalizeAssetPath(params.assetPath);

    try {
      // Offload the heavy graph traversal to C++
      const response: any = await this.sendRequest('get_asset_graph', {
        assetPath,
        maxDepth,
        subAction: 'get_asset_graph'
      }, 'get_asset_graph', { timeoutMs: 60000 });

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
    } catch (e: any) {
      return { success: false, error: `Analysis failed: ${e.message}` };
    }
  }

  async createThumbnail(params: { assetPath: string; width?: number; height?: number }) {
    return this.sendRequest('generate_thumbnail', {
      ...params,
      assetPath: this.normalizeAssetPath(params.assetPath)
    }, 'generate_thumbnail', { timeoutMs: 60000 });
  }

  async setTags(params: { assetPath: string; tags: string[] }) {
    return this.sendRequest('set_tags', {
      ...params,
      assetPath: this.normalizeAssetPath(params.assetPath)
    }, 'set_tags', { timeoutMs: 60000 });
  }

  async generateReport(params: { directory: string; reportType?: string; outputPath?: string }) {
    return this.sendRequest('generate_report', {
      ...params,
      directory: this.normalizeAssetPath(params.directory)
    }, 'generate_report', { timeoutMs: 300000 });
  }

  async validate(params: { assetPath: string }) {
    return this.sendRequest('validate', {
      ...params,
      assetPath: this.normalizeAssetPath(params.assetPath)
    }, 'validate', { timeoutMs: 300000 });
  }

  async generateLODs(params: { assetPath: string; lodCount: number }) {
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
      const response: any = await automation.sendAutomationRequest('generate_lods', {
        assetPaths: [assetPath],
        numLODs: lodCount
      }, { timeoutMs: 120000 });

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
        error: `Failed to generate LODs: ${error instanceof Error ? error.message : String(error)}`
      };
    }
  }
}
