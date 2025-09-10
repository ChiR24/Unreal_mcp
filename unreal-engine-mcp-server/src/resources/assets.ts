import { UnrealBridge } from '../unreal-bridge.js';

export class AssetResources {
  constructor(private bridge: UnrealBridge) {}

  async list(dir = '/Game', recursive = true) {
    // Try multiple methods to get assets
    
    // Method 1: Try the search API endpoint
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
        return searchResult.Assets;
      }
    } catch (err) {
      // Continue to fallback
    }
    
    // Method 2: Try EditorAssetLibrary (might not work via Remote Control)
    try {
      const res = await this.bridge.call({
        objectPath: '/Script/UnrealEd.Default__EditorAssetLibrary',
        functionName: 'ListAssets',
        parameters: { 
          DirectoryPath: dir, 
          bRecursive: recursive,
          bIncludeFolder: false
        }
      });
      
      if (res?.Result) {
        return res.Result;
      }
    } catch (err) {
      // Continue to fallback
    }
    
    // Method 3: Try to get content browser assets
    try {
      const contentResult = await this.bridge.httpCall('/remote/object/call', 'PUT', {
        objectPath: '/Script/ContentBrowser.Default__ContentBrowserFunctionLibrary',
        functionName: 'GetAssetRegistry',
        parameters: {},
        generateTransaction: false
      });
      
      if (contentResult?.Result) {
        return contentResult.Result;
      }
    } catch (err) {
      // Continue
    }
    
    // If all methods fail, return empty array with info
    return { 
      assets: [], 
      note: 'Asset listing requires proper Remote Control setup. Ensure HTTP API is enabled and asset registry is accessible.' 
    };
  }

  async find(assetPath: string) {
    const res = await this.bridge.call({
      objectPath: '/Script/UnrealEd.Default__EditorAssetLibrary',
      functionName: 'DoesAssetExist',
      parameters: { AssetPath: assetPath }
    });
    return res?.Result ?? res;
  }
}
