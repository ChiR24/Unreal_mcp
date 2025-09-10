import { UnrealBridge } from '../unreal-bridge.js';

export class AssetTools {
  constructor(private bridge: UnrealBridge) {}

  async importAsset(sourcePath: string, destinationPath: string) {
    try {
      // Sanitize destination path (remove trailing slash)
      const cleanDest = destinationPath.replace(/\/$/, '');

      // Use Python API to import asset
      const pythonCode = `
import unreal

# Set up the import task
task = unreal.AssetImportTask()
task.filename = r'${sourcePath}'
task.destination_path = '${cleanDest}'
task.automated = True
task.save = True
task.replace_existing = True

# Use AssetTools to import
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
try:
    asset_tools.import_asset_tasks([task])
    if task.imported_object_paths:
        print(f"RESULT:{'{'}'success': True, 'imported': { '{'}'count': len(task.imported_object_paths), 'paths': task.imported_object_paths{'}' }{'}'}")
    else:
        print(f"RESULT:{'{'}'success': False, 'error': 'No assets imported', 'source': task.filename{'}'}")
except Exception as e:
    print(f"RESULT:{'{'}'success': False, 'error': str(e), 'source': task.filename{'}'}")
`.trim();
      
      const pyResp = await this.bridge.executePython(pythonCode);

      // Parse Python output
      let outputStr = '';
      if (typeof pyResp === 'object' && pyResp !== null) {
        if (pyResp.LogOutput && Array.isArray(pyResp.LogOutput)) {
          outputStr = pyResp.LogOutput.map((l: any) => l.Output || '').join('');
        } else if ('ReturnValue' in pyResp) {
          outputStr = String((pyResp as any).ReturnValue);
        } else {
          outputStr = JSON.stringify(pyResp);
        }
      } else {
        outputStr = String(pyResp || '');
      }

      const match = outputStr.match(/RESULT:({.*})/);
      if (match) {
        try {
          const parsed = JSON.parse(match[1].replace(/'/g, '"'));
          if (parsed.success) {
            const count = parsed.imported?.count ?? 0;
            const paths = parsed.imported?.paths ?? [];
            return { success: true, message: `Imported ${count} assets to ${cleanDest}`, paths };
          } else {
            return { error: `Import failed: ${parsed.error || 'Unknown error'} (source: ${parsed.source || sourcePath})` };
          }
        } catch {
          // Fall through
        }
      }

      // If unable to parse, return generic attempt result
      return { error: `Import did not report success for source ${sourcePath}` };
    } catch (err) {
      return { error: `Failed to import asset: ${err}` };
    }
  }

  async duplicateAsset(sourcePath: string, destinationPath: string) {
    try {
      const res = await this.bridge.call({
        objectPath: '/Script/EditorScriptingUtilities.Default__EditorAssetLibrary',
        functionName: 'DuplicateAsset',
        parameters: {
          SourceAssetPath: sourcePath,
          DestinationAssetPath: destinationPath
        }
      });
      return res?.Result ?? res;
    } catch (err) {
      return { error: `Failed to duplicate asset: ${err}` };
    }
  }

  async deleteAsset(assetPath: string) {
    try {
      const res = await this.bridge.call({
        objectPath: '/Script/EditorScriptingUtilities.Default__EditorAssetLibrary',
        functionName: 'DeleteAsset',
        parameters: {
          AssetPathToDelete: assetPath
        }
      });
      return res?.Result ?? res;
    } catch (err) {
      return { error: `Failed to delete asset: ${err}` };
    }
  }

  async saveAsset(assetPath: string) {
    try {
      const res = await this.bridge.call({
        objectPath: '/Script/EditorScriptingUtilities.Default__EditorAssetLibrary',
        functionName: 'SaveAsset',
        parameters: {
          AssetToSave: assetPath
        }
      });
      return res?.Result ?? res;
    } catch (err) {
      return { error: `Failed to save asset: ${err}` };
    }
  }
}
