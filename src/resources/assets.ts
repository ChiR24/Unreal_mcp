import { BaseTool } from '../tools/base-tool.js';
import { IAssetResources } from '../types/tool-interfaces.js';
import { coerceString } from '../utils/result-helpers.js';
import { AutomationResponse } from '../types/automation-responses.js';
import { Logger } from '../utils/logger.js';
import { ResourceCache } from '../cache/resource-cache.js';

const log = new Logger('AssetResources');

export class AssetResources extends BaseTool implements IAssetResources {
  // Simple in-memory cache for asset listing
  private cache = new ResourceCache<Record<string, unknown>>(Number(process.env.ASSET_LIST_TTL_MS || 10000));
  
  private makeKey(dir: string, recursive: boolean, depth: number | undefined, page?: number) {
    const d = depth !== undefined ? depth : -1;
    const r = recursive ? 1 : 0;
    return page !== undefined ? `${dir}::${r}::${d}::${page}` : `${dir}::${r}::${d}`;
  }

  // Normalize UE content paths:
  // - Map '/Content' -> '/Game'
  // - Ensure forward slashes
  private normalizeDir(dir: string): string {
    try {
      if (!dir || typeof dir !== 'string') return '/Game';
      let d = dir.replace(/\\/g, '/');
      if (!d.startsWith('/')) d = '/' + d;
      if (d.toLowerCase().startsWith('/content')) {
        d = '/Game' + d.substring('/Content'.length);
      }
      // Collapse multiple slashes
      d = d.replace(/\/+/g, '/');
      // Remove trailing slash except root
      if (d.length > 1) d = d.replace(/\/$/, '');
      return d;
    } catch {
      return '/Game';
    }
  }

  clearCache(dir?: string) {
    if (!dir) {
      this.cache.clear();
      return;
    }

    const normalized = this.normalizeDir(dir);
    this.cache.invalidate(`${normalized}::`);
  }

  invalidateAssetPaths(paths: string[]) {
    if (!Array.isArray(paths) || paths.length === 0) {
      return;
    }

    const dirs = new Set<string>();
    for (const rawPath of paths) {
      if (typeof rawPath !== 'string' || rawPath.trim().length === 0) {
        continue;
      }
      const normalized = this.normalizeDir(rawPath);
      dirs.add(normalized);
      const parent = this.parentDirectory(normalized);
      if (parent) {
        dirs.add(parent);
      }
    }

    for (const dir of dirs) {
      this.clearCache(dir);
    }
  }

  async list(dir = '/Game', recursive = false, limit = 50, options?: { refresh?: boolean; depth?: number }) {
    // Normalize directory first
    dir = this.normalizeDir(dir);
    const depth = options?.depth;
    const key = this.makeKey(dir, recursive, depth);

    try {
      return await this.cache.getOrFetch(key, async () => {
        // Check if bridge is connected
        if (!this.bridge.isConnected) {
          throw new Error('NOT_CONNECTED');
        }

        // Use directory-based listing
        const listed = await this.listDirectoryOnly(dir, recursive, limit, depth);
        
        // Ensure success
        const listedObj = listed as Record<string, unknown>;
        if (listed && listedObj.success === false) {
           throw new Error(String(listedObj.error || 'Asset listing failed'));
        }
        
        return { ...listed, success: true };
      }, { refresh: options?.refresh });

    } catch (err: unknown) {
      const errorMessage = err instanceof Error ? err.message : String(err);
      
      if (errorMessage === 'NOT_CONNECTED') {
        return {
          success: false,
          assets: [],
          warning: 'Unreal Engine is not connected. Please ensure Unreal Engine is running with the MCP server enabled.',
          connectionStatus: 'disconnected'
        };
      }

      // Return error object similar to original implementation
      return {
        success: false,
        path: dir,
        summary: { total: 0, folders: 0, assets: 0 },
        foldersList: [],
        assets: [],
        error: errorMessage
      };
    }
  }

  /**
   * List assets with pagination support
   * @param dir Directory to list assets from
   * @param page Page number (0-based)
   * @param pageSize Number of assets per page (max 50 to avoid socket failures)
   */
  async listPaged(dir = '/Game', page = 0, pageSize = 30, recursive = false, options?: { refresh?: boolean; depth?: number }) {
    // Ensure pageSize doesn't exceed safe limit
    const safePageSize = Math.min(pageSize, 50);
    const offset = page * safePageSize;

    // Normalize directory and check cache for this specific page
    dir = this.normalizeDir(dir);
    const depth = options?.depth;
    const cacheKey = this.makeKey(dir, recursive, depth, page);

    try {
      return await this.cache.getOrFetch(cacheKey, async () => {
        if (!this.bridge.isConnected) {
          throw new Error('NOT_CONNECTED');
        }

        // Use search API with pagination
        // Use the same directory listing approach but with pagination
        const allAssets = await this.listDirectoryOnly(dir, recursive, 1000, depth);
        const allAssetsObj = allAssets as unknown as { assets: unknown[] };

        // Paginate the results
        const start = offset;
        const end = offset + safePageSize;
        const pagedAssets = allAssetsObj.assets ? allAssetsObj.assets.slice(start, end) : [];

        return {
          assets: pagedAssets,
          page,
          pageSize: safePageSize,
          count: pagedAssets.length,
          totalCount: allAssetsObj.assets ? allAssetsObj.assets.length : 0,
          hasMore: end < (allAssetsObj.assets ? allAssetsObj.assets.length : 0),
          method: 'directory_listing_paged'
        };
      }, { refresh: options?.refresh });

    } catch (err: unknown) {
       const errorMessage = err instanceof Error ? err.message : String(err);
       
       if (errorMessage === 'NOT_CONNECTED') {
          return {
            assets: [],
            page,
            pageSize: safePageSize,
            warning: 'Unreal Engine is not connected.',
            connectionStatus: 'disconnected'
          };
       }

       log.warn(`Asset listing page ${page} failed: ${errorMessage}`);
       return {
         assets: [],
         page,
         pageSize: safePageSize,
         error: 'Failed to fetch page'
       };
    }
  }

  /**
   * Directory-based listing of immediate children using AssetRegistry.
   * Returns both subfolders and assets at the given path.
   */
  private async listDirectoryOnly(dir: string, recursive: boolean, limit: number, depth?: number) {
    try {
      // Use the native C++ plugin's list action
      const normalizedDir = this.normalizeDir(dir);
      const payload: Record<string, unknown> = { 
        directory: normalizedDir, 
        limit, 
        recursive,
        subAction: 'list'  // Explicitly set subAction for C++ handler to read
      };
      if (depth !== undefined) payload.depth = depth;

      const response = await this.sendAutomationRequest<AutomationResponse>(
        'manage_asset',  // Use proper tool name
        payload,
        { timeoutMs: 30000 }
      );

      if (response.success !== false && response.result) {
        const payload = response.result as Record<string, unknown>;

        const foldersList = payload.folders_list as Array<Record<string, unknown>> | undefined;
        const foldersArr = Array.isArray(foldersList)
          ? foldersList.map((f) => ({
            name: coerceString(f?.n ?? f?.Name ?? f?.name) ?? '',
            path: coerceString(f?.p ?? f?.Path ?? f?.path) ?? '',
            class: 'Folder',
            isFolder: true
          }))
          : [];

        const assetsList = payload.assets as Array<Record<string, unknown>> | undefined;
        const assetsArr = Array.isArray(assetsList)
          ? assetsList.map((a) => ({
            name: coerceString(a?.n ?? a?.Name ?? a?.name) ?? '',
            path: coerceString(a?.p ?? a?.Path ?? a?.path) ?? '',
            class: coerceString(a?.c ?? a?.Class ?? a?.class) ?? 'Object'
          }))
          : [];

        return {
          success: true,
          assets: [...foldersArr, ...assetsArr],
          count: foldersArr.length + assetsArr.length,
          folders: foldersArr.length,
          files: assetsArr.length,
          path: normalizedDir,
          recursive,
          method: 'automation_bridge',
          cached: false
        };
      }
      
      // C++ returned success: false - propagate error instead of masking it
      const errorMsg = String((response as Record<string, unknown>).message || (response as Record<string, unknown>).error || 'Asset listing not implemented in native bridge');
      throw new Error(errorMsg);
    } catch (err) {
      // Re-throw with context instead of silently swallowing
      const message = err instanceof Error ? err.message : String(err);
      throw new Error(`AssetRegistry query failed: ${message}`);
    }
  }

  async find(assetPath: string) {
    // Guard against invalid paths (trailing slash, empty, whitespace)
    if (!assetPath || typeof assetPath !== 'string' || assetPath.trim() === '' || assetPath.endsWith('/')) {
      return false;
    }

    try {
      const normalizedPath = this.normalizeDir(assetPath);
      const response = await this.sendAutomationRequest<AutomationResponse>(
        'asset_exists',
        { asset_path: normalizedPath }
      );

      const resultObj = response?.result as Record<string, unknown> | undefined;
      return response?.success !== false && resultObj?.exists === true;
    } catch {
      return false;
    }
  }

  private parentDirectory(path: string): string | null {
    if (!path || typeof path !== 'string') {
      return null;
    }

    const normalized = this.normalizeDir(path);
    const lastSlash = normalized.lastIndexOf('/');
    if (lastSlash <= 0) {
      return normalized === '/' ? '/' : null;
    }

    const parent = normalized.substring(0, lastSlash);
    return parent.length > 0 ? parent : '/';
  }
}
