
import { BaseTool } from './base-tool.js';
import { IAssetTools } from '../types/tool-interfaces.js';
import { wasmIntegration } from '../wasm/index.js';

export class AssetTools extends BaseTool implements IAssetTools {
  async importAsset(params: { sourcePath: string; destinationPath: string; overwrite?: boolean; save?: boolean }) {
    return this.sendRequest('import', {
      ...params
    }, 'import');
  }

  async duplicateAsset(params: { sourcePath: string; destinationPath: string }) {
    return this.sendRequest('duplicate', {
      ...params
    }, 'duplicate');
  }

  async renameAsset(params: { sourcePath: string; destinationPath: string }) {
    return this.sendRequest('rename', {
      ...params
    }, 'rename');
  }

  async moveAsset(params: { sourcePath: string; destinationPath: string }) {
    return this.sendRequest('move', {
      ...params
    }, 'move');
  }

  async deleteAssets(params: { paths: string[]; fixupRedirectors?: boolean; timeoutMs?: number }) {
    const assetPaths = Array.isArray(params.paths) ? params.paths : [];
    return this.sendRequest('bulk_delete', {
      assetPaths,
      fixupRedirectors: params.fixupRedirectors
    }, 'bulk_delete');
  }

  async searchAssets(params: { classNames?: string[]; packagePaths?: string[]; recursivePaths?: boolean; recursiveClasses?: boolean; limit?: number }) {
    return this.sendRequest('search_assets', {
      ...params,
      subAction: 'search_assets'
    }, 'search_assets');
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
            { timeoutMs: 30000 }
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
    }, 'create_folder');
  }

  async getDependencies(params: { assetPath: string; recursive?: boolean }) {
    return this.sendRequest('get_dependencies', {
      ...params,
      subAction: 'get_dependencies'
    }, 'get_dependencies');
  }

  async getSourceControlState(params: { assetPath: string }) {
    return this.sendRequest('get_source_control_state', {
      ...params,
      subAction: 'get_source_control_state'
    }, 'get_source_control_state');
  }

  async analyzeGraph(params: { assetPath: string; maxDepth?: number }) {
    const maxDepth = params.maxDepth ?? 3;
    const graph: Record<string, string[]> = {};
    const visited = new Set<string>();
    const queue: { path: string; depth: number }[] = [{ path: params.assetPath, depth: 0 }];

    visited.add(params.assetPath);

    while (queue.length > 0) {
      const next = queue.shift();
      if (!next) {
        break;
      }

      const { path, depth } = next;

      if (depth >= maxDepth) {
        // If we hit max depth, we don't fetch children, but we should record the node
        if (!graph[path]) graph[path] = [];
        continue;
      }

      try {
        // Fetch immediate dependencies
        const depsResponse = await this.getDependencies({ assetPath: path, recursive: false });

        // If fetch fails or no dependencies, treat as leaf
        if (!depsResponse || !depsResponse.success || !Array.isArray(depsResponse.dependencies)) {
          graph[path] = [];
          continue;
        }

        const deps = depsResponse.dependencies as string[];
        graph[path] = deps;

        for (const dep of deps) {
          if (!visited.has(dep)) {
            visited.add(dep);
            queue.push({ path: dep, depth: depth + 1 });
          }
        }
      } catch (err) {
        console.warn(`Failed to fetch dependencies for ${path}:`, err);
        graph[path] = [];
      }
    }

    // Use WASM for analysis on the constructed graph
    try {
      const analysis = await wasmIntegration.resolveDependencies(
        params.assetPath,
        graph,
        { maxDepth }
      );
      return { success: true, analysis };
    } catch (e: any) {
      return { success: false, error: `WASM analysis failed: ${e.message}` };
    }
  }

  async createThumbnail(params: { assetPath: string; width?: number; height?: number }) {
    return this.sendRequest('generate_thumbnail', {
      ...params
    }, 'generate_thumbnail');
  }

  async setTags(params: { assetPath: string; tags: string[] }) {
    return this.sendRequest('set_tags', {
      ...params
    }, 'set_tags');
  }

  async generateReport(params: { directory: string; reportType?: string; outputPath?: string }) {
    return this.sendRequest('generate_report', {
      ...params
    }, 'generate_report');
  }

  async validate(params: { assetPath: string }) {
    return this.sendRequest('validate', {
      ...params
    }, 'validate');
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
