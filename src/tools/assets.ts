
import { BaseTool } from './base-tool.js';
import { IAssetTools } from '../types/tool-interfaces.js';
import { wasmIntegration } from '../wasm/index.js';

export class AssetTools extends BaseTool implements IAssetTools {
  async importAsset(params: { sourcePath: string; destinationPath: string; overwrite?: boolean; save?: boolean }) {
    return this.sendRequest('import', {
      ...params
    }, 'import', { timeoutMs: 120000 });
  }

  async duplicateAsset(params: { sourcePath: string; destinationPath: string }) {
    return this.sendRequest('duplicate', {
      ...params
    }, 'duplicate', { timeoutMs: 60000 });
  }

  async renameAsset(params: { sourcePath: string; destinationPath: string }) {
    return this.sendRequest('rename', {
      ...params
    }, 'rename', { timeoutMs: 60000 });
  }

  async moveAsset(params: { sourcePath: string; destinationPath: string }) {
    return this.sendRequest('move', {
      ...params
    }, 'move', { timeoutMs: 60000 });
  }

  async deleteAssets(params: { paths: string[]; fixupRedirectors?: boolean; timeoutMs?: number }) {
    const assetPaths = Array.isArray(params.paths) ? params.paths : [];
    return this.sendRequest('bulk_delete', {
      assetPaths,
      fixupRedirectors: params.fixupRedirectors
    }, 'bulk_delete', { timeoutMs: 120000 });
  }

  async searchAssets(params: { classNames?: string[]; packagePaths?: string[]; recursivePaths?: boolean; recursiveClasses?: boolean; limit?: number }) {
    // Route via asset_query action with subAction 'search_assets'
    return this.sendRequest('asset_query', {
      ...params,
      subAction: 'search_assets'
    }, 'asset_query', { timeoutMs: 60000 });
  }

  async saveAsset(assetPath: string) {
    try {
      // Try Automation Bridge first
      const bridge = this.getAutomationBridge();
      if (bridge && typeof bridge.sendAutomationRequest === 'function') {
        try {
          const response: any = await bridge.sendAutomationRequest(
            'save_asset',
            { assetPath },
            { timeoutMs: 60000 }
          );

          if (response && response.success !== false) {
            return {
              success: true,
              saved: response.saved ?? true,
              message: response.message || 'Asset saved'
            };
          }
        } catch (_err) {
          // Fall through to executeEditorFunction
        }
      }

      // Fallback to executeEditorFunction
      const res = await this.bridge.executeEditorFunction('SAVE_ASSET', { path: assetPath });
      if (res && typeof res === 'object' && (res.success === true || (res.result && res.result.success === true))) {
        const saved = Boolean(res.saved ?? (res.result && res.result.saved));
        return { success: true, saved };
      }

      return { success: false, error: (res as any)?.error ?? 'Failed to save asset' };
    } catch (err) {
      return { success: false, error: `Failed to save asset: ${err}` };
    }
  }

  async createFolder(folderPath: string) {
    return this.sendRequest('create_folder', {
      path: folderPath
    }, 'create_folder', { timeoutMs: 60000 });
  }

  async getDependencies(params: { assetPath: string; recursive?: boolean }) {
    return this.sendRequest('get_dependencies', {
      ...params,
      subAction: 'get_dependencies'
    }, 'get_dependencies');
  }

  async getSourceControlState(params: { assetPath: string }) {
    return this.sendRequest('asset_query', {
      ...params,
      subAction: 'get_source_control_state'
    }, 'asset_query');
  }

  async analyzeGraph(params: { assetPath: string; maxDepth?: number }) {
    const maxDepth = params.maxDepth ?? 3;

    try {
      // Offload the heavy graph traversal to C++
      const response: any = await this.sendRequest('get_asset_graph', {
        assetPath: params.assetPath,
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
        params.assetPath,
        graph,
        { maxDepth }
      );

      const depth = await wasmIntegration.calculateDependencyDepth(
        params.assetPath,
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
        asset: (base as any).asset ?? params.assetPath,
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
      ...params
    }, 'generate_thumbnail', { timeoutMs: 60000 });
  }

  async setTags(params: { assetPath: string; tags: string[] }) {
    return this.sendRequest('set_tags', {
      ...params
    }, 'set_tags', { timeoutMs: 60000 });
  }

  async generateReport(params: { directory: string; reportType?: string; outputPath?: string }) {
    return this.sendRequest('generate_report', {
      ...params
    }, 'generate_report', { timeoutMs: 300000 });
  }

  async validate(params: { assetPath: string }) {
    return this.sendRequest('validate', {
      ...params
    }, 'validate', { timeoutMs: 300000 });
  }

  async generateLODs(params: { assetPath: string; lodCount: number }) {
    const assetPath = String(params.assetPath ?? '').trim();
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
        assetPath,
        lodCount
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
        lodCount: (result.lodCount as number) ?? lodCount
      };
    } catch (error) {
      return {
        success: false,
        error: `Failed to generate LODs: ${error instanceof Error ? error.message : String(error)}`
      };
    }
  }
}
