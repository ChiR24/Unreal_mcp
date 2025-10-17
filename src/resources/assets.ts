import { UnrealBridge } from '../unreal-bridge.js';
import { coerceString } from '../utils/result-helpers.js';

export class AssetResources {
  constructor(private bridge: UnrealBridge) {}

  // Simple in-memory cache for asset listing
  private cache = new Map<string, { timestamp: number; data: any }>();
  private get ttlMs(): number { return Number(process.env.ASSET_LIST_TTL_MS || 10000); }
  private makeKey(dir: string, recursive: boolean, page?: number) { 
    return page !== undefined ? `${dir}::${recursive ? 1 : 0}::${page}` : `${dir}::${recursive ? 1 : 0}`; 
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

  async list(dir = '/Game', _recursive = false, limit = 50) {
    // ALWAYS use non-recursive listing to show only immediate children
    // This prevents timeouts and makes navigation clearer
    _recursive = false; // Force non-recursive
    
    // Normalize directory first
    dir = this.normalizeDir(dir);

    // Cache fast-path
    try {
      const key = this.makeKey(dir, false);
      const entry = this.cache.get(key);
      const now = Date.now();
      if (entry && (now - entry.timestamp) < this.ttlMs) {
        return entry.data;
      }
    } catch {}
    
    // Check if bridge is connected
    if (!this.bridge.isConnected) {
      return {
        assets: [],
        warning: 'Unreal Engine is not connected. Please ensure Unreal Engine is running with Remote Control enabled.',
        connectionStatus: 'disconnected'
      };
    }
    
    // Always use directory-only listing (immediate children)
    return this.listDirectoryOnly(dir, false, limit);
    // End of list method - all logic is now in listDirectoryOnly
  }

  /**
   * List assets with pagination support
   * @param dir Directory to list assets from
   * @param page Page number (0-based)
   * @param pageSize Number of assets per page (max 50 to avoid socket failures)
   */
  async listPaged(dir = '/Game', page = 0, pageSize = 30, recursive = false) {
    // Ensure pageSize doesn't exceed safe limit
    const safePageSize = Math.min(pageSize, 50);
    const offset = page * safePageSize;
    
    // Normalize directory and check cache for this specific page
    dir = this.normalizeDir(dir);
    const cacheKey = this.makeKey(dir, recursive, page);
    const cached = this.cache.get(cacheKey);
    if (cached && (Date.now() - cached.timestamp) < this.ttlMs) {
      return cached.data;
    }
    
    if (!this.bridge.isConnected) {
      return {
        assets: [],
        page,
        pageSize: safePageSize,
        warning: 'Unreal Engine is not connected.',
        connectionStatus: 'disconnected'
      };
    }
    
    try {
      // Use search API with pagination
      // Use the same directory listing approach but with pagination
      const allAssets = await this.listDirectoryOnly(dir, false, 1000);
      
      // Paginate the results
      const start = offset;
      const end = offset + safePageSize;
      const pagedAssets = allAssets.assets ? allAssets.assets.slice(start, end) : [];
      
      const result = {
        assets: pagedAssets,
        page,
        pageSize: safePageSize,
        count: pagedAssets.length,
        totalCount: allAssets.assets ? allAssets.assets.length : 0,
        hasMore: end < (allAssets.assets ? allAssets.assets.length : 0),
        method: 'directory_listing_paged'
      };
      
      this.cache.set(cacheKey, { timestamp: Date.now(), data: result });
      return result;
    } catch (err: any) {
      console.warn(`Asset listing page ${page} failed:`, err.message);
    }
    
    return {
      assets: [],
      page,
      pageSize: safePageSize,
      error: 'Failed to fetch page'
    };
  }

  /**
   * Directory-based listing of immediate children using AssetRegistry.
   * Returns both subfolders and assets at the given path.
   */
  private async listDirectoryOnly(dir: string, _recursive: boolean, limit: number) {
    // Always return only immediate children to avoid timeout and improve navigation
    try {
      // Use the native C++ plugin's list action instead of Python
      const automationBridge = (this.bridge as any).automationBridge;
      if (automationBridge && typeof automationBridge.sendAutomationRequest === 'function') {
        try {
          const normalizedDir = this.normalizeDir(dir);
          const response = await automationBridge.sendAutomationRequest(
            'list',
            { directory: normalizedDir, limit, recursive: false },
            { timeoutMs: 30000 }
          );

          if (response.success !== false && response.result) {
            const payload = response.result;
            
            const foldersArr = Array.isArray(payload.folders_list)
              ? payload.folders_list.map((f: any) => ({
                  Name: coerceString(f?.n) ?? '',
                  Path: coerceString(f?.p) ?? '',
                  Class: 'Folder',
                  isFolder: true
                }))
              : [];

            const assetsArr = Array.isArray(payload.assets)
              ? payload.assets.map((a: any) => ({
                  Name: coerceString(a?.n) ?? '',
                  Path: coerceString(a?.p) ?? '',
                  Class: coerceString(a?.c) ?? 'Object'
                }))
              : [];

            const result = {
              assets: [...foldersArr, ...assetsArr],
              count: foldersArr.length + assetsArr.length,
              folders: foldersArr.length,
              files: assetsArr.length,
              path: normalizedDir,
              recursive: false,
              method: 'automation_bridge',
              cached: false
            };

            const key = this.makeKey(dir, false);
            this.cache.set(key, { timestamp: Date.now(), data: result });
            return result;
          }
        } catch {}
      }
      
      // No fallback available
    } catch (err: any) {
      const errorMessage = err?.message ? String(err.message) : 'Asset registry request failed';
      console.warn('Engine asset listing failed:', errorMessage);
      return {
        success: false,
        path: this.normalizeDir(dir),
        summary: { total: 0, folders: 0, assets: 0 },
        foldersList: [],
        assets: [],
        error: errorMessage,
        warning: 'AssetRegistry query failed. Ensure the MCP Automation Bridge is connected.',
  transport: 'automation_bridge',
  method: 'asset_registry_alternate'
      };
    }

    return {
      success: false,
      path: this.normalizeDir(dir),
      summary: { total: 0, folders: 0, assets: 0 },
      foldersList: [],
      assets: [],
      error: 'Asset registry returned no payload.',
      warning: 'No items returned from AssetRegistry request.',
      transport: 'automation_bridge',
      method: 'asset_registry_empty'
    };
  }

  async find(assetPath: string) {
    // Guard against invalid paths (trailing slash, empty, whitespace)
    if (!assetPath || typeof assetPath !== 'string' || assetPath.trim() === '' || assetPath.endsWith('/')) {
      return false;
    }

    try {
      const automationBridge = (this.bridge as any).automationBridge;
      if (!automationBridge || typeof automationBridge.sendAutomationRequest !== 'function') {
        return false;
      }

      const normalizedPath = this.normalizeDir(assetPath);
      const response = await automationBridge.sendAutomationRequest(
        'asset_exists',
        { asset_path: normalizedPath }
      );

      return response?.success !== false && response?.result?.exists === true;
    } catch {
      return false;
    }
  }
}
