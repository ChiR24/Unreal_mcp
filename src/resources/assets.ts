import { UnrealBridge } from '../unreal-bridge.js';

export class AssetResources {
  constructor(private bridge: UnrealBridge) {}

  // Simple in-memory cache for asset listing
  private cache = new Map<string, { timestamp: number; data: any }>();
  private get ttlMs(): number { return Number(process.env.ASSET_LIST_TTL_MS || 10000); }
  private makeKey(dir: string, recursive: boolean, page?: number) { 
    return page !== undefined ? `${dir}::${recursive ? 1 : 0}::${page}` : `${dir}::${recursive ? 1 : 0}`; 
  }

  async list(dir = '/Game', _recursive = false, limit = 50) {
    // ALWAYS use non-recursive listing to show only immediate children
    // This prevents timeouts and makes navigation clearer
    recursive = false; // Force non-recursive
    
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
    
    // Check cache for this specific page
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
   * Directory-based listing for paths with too many assets
   * Shows only immediate children (folders and files) to avoid timeouts
   */
  private async listDirectoryOnly(dir: string, _recursive: boolean, limit: number) {
    // Always return only immediate children to avoid timeout and improve navigation
    try {
      const py = `
import unreal
import json

_dir = r"${dir}"

try:
    # ALWAYS non-recursive - get only immediate children
    all_paths = unreal.EditorAssetLibrary.list_assets(_dir, False, False)
    
    # Organize into immediate children only
    immediate_folders = set()
    immediate_assets = []
    
    for path in all_paths:
        # Remove the base directory to get relative path
        relative = path.replace(_dir, '').strip('/')
        if not relative:
            continue
            
        # Split to check depth
        parts = relative.split('/')
        
        if len(parts) == 1:
            # This is an immediate child asset
            immediate_assets.append(path)
        elif len(parts) > 1:
            # This indicates a subfolder exists
            immediate_folders.add(parts[0])
    
    result = []
    
    # Add folders first
    for folder in sorted(immediate_folders):
        result.append({
            'n': folder,
            'p': _dir + '/' + folder,
            'c': 'Folder',
            'isFolder': True
        })
    
    # Add immediate assets (limit to prevent socket issues)
    for asset_path in immediate_assets[:min(${limit}, len(immediate_assets))]:
        name = asset_path.split('/')[-1].split('.')[0]
        result.append({
            'n': name,
            'p': asset_path,
            'c': 'Asset'
        })
    
    # Always showing immediate children only
    note = f'Showing immediate children of {_dir} ({len(immediate_folders)} folders, {len(immediate_assets)} files)'
    
    print("RESULT:" + json.dumps({
        'success': True, 
        'assets': result, 
        'count': len(result),
        'folders': len(immediate_folders),
        'files': len(immediate_assets),
        'note': note
    }))
except Exception as e:
    print("RESULT:" + json.dumps({'success': False, 'error': str(e), 'assets': []}))
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
            // Transform to standard format
            const assets = parsed.assets.map((a: any) => ({
              Name: a.n,
              Path: a.p,
              Class: a.c,
              isFolder: a.isFolder || false
            }));
            
            return {
              assets,
              count: parsed.count,
              folders: parsed.folders,
              files: parsed.files,
              note: parsed.note,
              method: 'directory_listing'
            };
          }
        } catch {}
      }
    } catch (err: any) {
      console.warn('Engine asset listing failed:', err.message);
    }
    
    // Fallback: return empty with explanation
    return {
      assets: [],
      warning: 'Directory contains too many assets. Showing immediate children only.',
      suggestion: 'Navigate to specific subdirectories for detailed listings.',
      method: 'directory_timeout_fallback'
    };
  }

  async find(assetPath: string) {
    // Guard against invalid paths (trailing slash, empty, whitespace)
    if (!assetPath || typeof assetPath !== 'string' || assetPath.trim() === '' || assetPath.endsWith('/')) {
      return false;
    }

    const py = `
import unreal
apath = r"${assetPath}"
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
