import { UnrealBridge } from '../unreal-bridge.js';

export class AssetResources {
  constructor(private bridge: UnrealBridge) {}

  // Simple in-memory cache for asset listing
  private cache = new Map<string, { timestamp: number; data: any }>();
  private get ttlMs(): number { return Number(process.env.ASSET_LIST_TTL_MS || 10000); }
  private makeKey(dir: string, recursive: boolean) { return `${dir}::${recursive ? 1 : 0}`; }

  async list(dir = '/Game', recursive = true) {
    // Cache fast-path
    try {
      const key = this.makeKey(dir, recursive);
      const entry = this.cache.get(key);
      const now = Date.now();
      if (entry && (now - entry.timestamp) < this.ttlMs) {
        return entry.data;
      }
    } catch {}
    // Try multiple methods to get assets
    
    // Method 1: Try the search API endpoint (fast path)
    try {
      const searchResult = await this.bridge.httpCall('/remote/search/assets', 'PUT', {
        Query: '',  // Empty query works, wildcard doesn't
        Filter: {
          PackagePaths: [dir],
          RecursivePaths: recursive,
          ClassNames: [],
          RecursiveClasses: true
        },
        Limit: 1000,
        Start: 0
      });
      
      if (searchResult?.Assets && searchResult.Assets.length > 0) {
        try { this.cache.set(this.makeKey(dir, recursive), { timestamp: Date.now(), data: searchResult.Assets }); } catch {}
        return searchResult.Assets;
      }
      } catch {
      // Continue to fallback
    }
    
    // Method 2: Python-based listing via AssetRegistry (most reliable)
    try {
      const py = `
import unreal
import json

# Inputs
_directory = r"${dir}"
_recursive = ${recursive ? 'True' : 'False'}

try:
    asset_registry = unreal.AssetRegistryHelpers.get_asset_registry()
    filter = unreal.ARFilter(package_paths=[_directory], recursive_paths=_recursive)
    assets = asset_registry.get_assets(filter)
    out = []
    for a in assets:
        try:
            cls = a.asset_class_path.asset_name if hasattr(a, 'asset_class_path') else a.asset_class
        except Exception:
            cls = str(a.asset_class)
        out.append({
            'Name': str(a.asset_name),
            'Path': str(a.package_path) + '/' + str(a.asset_name),
            'Class': str(cls),
            'PackagePath': str(a.package_path)
        })
    print("RESULT:" + json.dumps({'success': True, 'assets': out, 'count': len(out)}))
except Exception as e:
    try:
        # Fallback: EditorAssetLibrary.list_assets
        paths = unreal.EditorAssetLibrary.list_assets(_directory, _recursive, False)
        out = [{'Name': p.split('/')[-1].split('.')[0], 'Path': p, 'PackagePath': '/'.join(p.split('/')[:-1])} for p in paths]
        print("RESULT:" + json.dumps({'success': True, 'assets': out, 'count': len(out), 'method': 'EditorAssetLibrary'}))
    except Exception as e2:
        print("RESULT:" + json.dumps({'success': False, 'error': str(e2), 'assets': []}))
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
            try { this.cache.set(this.makeKey(dir, recursive), { timestamp: Date.now(), data: parsed.assets }); } catch {}
            return parsed.assets;
          }
        } catch {}
      }
      } catch {
      // Continue to fallback
    }
    
    // Method 3: Try to get content browser assets (best-effort)
    try {
      const contentResult = await this.bridge.httpCall('/remote/object/call', 'PUT', {
        objectPath: '/Script/ContentBrowser.Default__ContentBrowserFunctionLibrary',
        functionName: 'GetAssetRegistry',
        parameters: {},
        generateTransaction: false
      });
      
      if (contentResult?.Result) {
        try { this.cache.set(this.makeKey(dir, recursive), { timestamp: Date.now(), data: contentResult.Result }); } catch {}
        return contentResult.Result;
      }
    } catch {
      // Continue
    }
    
    // If all methods fail, return empty array with info
    const empty = { 
      assets: [], 
      note: 'Asset listing requires proper Remote Control setup. Ensure HTTP API is enabled and asset registry is accessible.' 
    };
    try { this.cache.set(this.makeKey(dir, recursive), { timestamp: Date.now(), data: empty }); } catch {}
    return empty;
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
