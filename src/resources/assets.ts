import { UnrealBridge } from '../unreal-bridge.js';

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
      const py = `
import unreal
import json

_dir = r"${this.normalizeDir(dir)}"

try:
    ar = unreal.AssetRegistryHelpers.get_asset_registry()
    # Immediate subfolders
    sub_paths = ar.get_sub_paths(_dir, False)
    folders_list = []
    for p in sub_paths:
        try:
            name = p.split('/')[-1]
            folders_list.append({'n': name, 'p': p})
        except Exception:
            pass

    # Immediate assets at this path
    assets_data = ar.get_assets_by_path(_dir, False)
    assets = []
    for a in assets_data[:${limit}]:
        try:
            assets.append({
                'n': str(a.asset_name),
                'p': str(a.object_path),
                'c': str(a.asset_class)
            })
        except Exception:
            pass

    print("RESULT:" + json.dumps({
        'success': True,
        'path': _dir,
        'folders': len(folders_list),
        'files': len(assets),
        'folders_list': folders_list,
        'assets': assets
    }))
except Exception as e:
    print("RESULT:" + json.dumps({'success': False, 'error': str(e), 'path': _dir}))
`.trim();

      const resp = await this.bridge.executePython(py);
      let output = '';
      if (resp?.LogOutput && Array.isArray(resp.LogOutput)) {
        output = resp.LogOutput.map((l: any) => l.Output || '').join('');
      } else if (typeof resp === 'string') {
        output = resp;
      } else {
        output = JSON.stringify(resp);
      }
      
      const m = output.match(/RESULT:({.*})/);
      if (m) {
        try {
          const parsed = JSON.parse(m[1]);
          if (parsed.success) {
            // Map folders and assets to a clear response
            const foldersArr = Array.isArray(parsed.folders_list) ? parsed.folders_list.map((f: any) => ({
              Name: f.n,
              Path: f.p,
              Class: 'Folder',
              isFolder: true
            })) : [];

            const assetsArr = Array.isArray(parsed.assets) ? parsed.assets.map((a: any) => ({
              Name: a.n,
              Path: a.p,
              Class: a.c || 'Asset',
              isFolder: false
            })) : [];

            const total = foldersArr.length + assetsArr.length;
            const summary = {
              total,
              folders: foldersArr.length,
              assets: assetsArr.length
            };

            return {
              success: true,
              path: parsed.path || this.normalizeDir(dir),
              summary,
              foldersList: foldersArr,
              assets: assetsArr,
              count: total,
              note: `Immediate children of ${parsed.path || this.normalizeDir(dir)}: ${foldersArr.length} folder(s), ${assetsArr.length} asset(s)`,
              method: 'asset_registry_listing'
            };
          }
        } catch {}
      }
    } catch (err: any) {
      console.warn('Engine asset listing failed:', err.message);
    }
    
    // Fallback: return empty with explanation
    return {
      success: true,
      path: this.normalizeDir(dir),
      summary: { total: 0, folders: 0, assets: 0 },
      foldersList: [],
      assets: [],
      warning: 'No items at this path or failed to query AssetRegistry.',
      method: 'asset_registry_fallback'
    };
  }

  async find(assetPath: string) {
    // Guard against invalid paths (trailing slash, empty, whitespace)
    if (!assetPath || typeof assetPath !== 'string' || assetPath.trim() === '' || assetPath.endsWith('/')) {
      return false;
    }

    // Normalize asset path (support users passing /Content/...)
    const ap = this.normalizeDir(assetPath);
    const py = `
import unreal
apath = r"${ap}"
try:
    exists = unreal.EditorAssetLibrary.does_asset_exist(apath)
    print("RESULT:{'success': True, 'exists': %s}" % ('True' if exists else 'False'))
except Exception as e:
    print("RESULT:{'success': False, 'error': '" + str(e) + "'}")
`.trim();
    const resp = await this.bridge.executePython(py);
    let output = '';
    if (resp?.LogOutput && Array.isArray(resp.LogOutput)) output = resp.LogOutput.map((l: any) => l.Output || '').join('');
    else if (typeof resp === 'string') output = resp; else output = JSON.stringify(resp);
    const m = output.match(/RESULT:({.*})/);
    if (m) {
      try {
        const parsed = JSON.parse(m[1].replace(/'/g, '"'));
        if (parsed.success) return !!parsed.exists;
      } catch {}
    }
    return false;
  }
}
