import { UnrealBridge } from '../unreal-bridge.js';

export class AssetTools {
  constructor(private bridge: UnrealBridge) {}

  async importAsset(sourcePath: string, destinationPath: string) {
    try {
      // Use Python API to import asset
      const pythonCode = `
import unreal

# Set up the import task
task = unreal.AssetImportTask()
task.filename = r'${sourcePath}'
task.destination_path = '${destinationPath}'
task.automated = True
task.save = True
task.replace_existing = True

# Use AssetTools to import
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
try:
    asset_tools.import_asset_tasks([task])
    if task.imported_object_paths:
        print(f"Successfully imported: {task.imported_object_paths}")
    else:
        print(f"No assets imported from: {task.filename}")
except Exception as e:
    print(f"Error importing asset: {str(e)}")
`.trim();
      
      const result = await this.bridge.executePython(pythonCode);
      return { success: true, message: `Import attempted from ${sourcePath} to ${destinationPath}` };
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
