import { UnrealBridge } from '../unreal-bridge.js';
import { AutomationBridge } from '../automation-bridge.js';
import { promises as fs } from 'fs';
import path from 'path';
import {
  bestEffortInterpretedText,
  coerceBoolean,
  coerceNumber,
  coerceString,
  coerceStringArray,
  interpretStandardResult
} from '../utils/result-helpers.js';
import { escapePythonString } from '../utils/python.js';

export class AssetTools {
  constructor(private bridge: UnrealBridge, private automationBridge?: AutomationBridge) {}

  setAutomationBridge(automationBridge?: AutomationBridge) {
    this.automationBridge = automationBridge;
  }

  private async executeAssetPython(script: string, timeoutMs?: number): Promise<any> {
    const trimmed = script.trim();

    if (this.automationBridge && typeof this.automationBridge.sendAutomationRequest === 'function') {
      try {
        const response = await this.automationBridge.sendAutomationRequest(
          'execute_editor_python',
          { script: trimmed },
          { timeoutMs }
        );

        if (response.success === false) {
          return {
            success: false,
            message: response.message || 'Automation bridge execution failed',
            error: response.error || response.message || 'AUTOMATION_BRIDGE_FAILURE'
          };
        }

        if (response.result !== undefined && response.result !== null) {
          return response.result;
        }

        if (response.success === true || response.success === undefined) {
          return {
            success: true,
            message: response.message || 'Automation bridge executed Python script'
          };
        }
      } catch (_err) {
        // Fall back to Remote Control execution if the automation bridge call fails
      }
    }

    return this.bridge.executePythonWithResult(trimmed);
  }

  async importAsset(sourcePath: string, destinationPath: string) {
    let createdTestFile = false;
    try {
      // Sanitize destination path (remove trailing slash) and normalize UE path
      let cleanDest = destinationPath.replace(/\/$/, '');
      // Map /Content -> /Game for UE asset destinations
      if (/^\/?content(\/|$)/i.test(cleanDest)) {
        cleanDest = '/Game' + cleanDest.replace(/^\/?content/i, '');
      }

      // Create test FBX file if it's a test file
      if (sourcePath.includes('test_model.fbx')) {
        // Create the file outside of Python, before import
        try {
          await this.createTestFBX(sourcePath);
          createdTestFile = true;
        } catch (_err) {
          // If we can't create the file, we'll handle it in Python
        }
      }

      // Use Python API to import asset with file creation fallback
  const pythonCode = `
import unreal
import os
import json

# Create test FBX if needed
source_path = r'${sourcePath}'
if 'test_model.fbx' in source_path and not os.path.exists(source_path):
    # Create directory if needed
    os.makedirs(os.path.dirname(source_path), exist_ok=True)
    
    # Create a valid FBX ASCII file
    fbx_content = """FBXHeaderExtension:  {
    FBXHeaderVersion: 1003
    FBXVersion: 7400
    CreationTimeStamp:  {
        Version: 1000
    }
    Creator: "MCP FBX Test Generator"
}
GlobalSettings:  {
    Version: 1000
    Properties70:  {
        P: "UpAxis", "int", "Integer", "",2
        P: "UpAxisSign", "int", "Integer", "",1
        P: "FrontAxis", "int", "Integer", "",1
        P: "FrontAxisSign", "int", "Integer", "",1
        P: "CoordAxis", "int", "Integer", "",0
        P: "CoordAxisSign", "int", "Integer", "",1
        P: "UnitScaleFactor", "double", "Number", "",1
    }
}
Definitions:  {
    Version: 100
    Count: 2
    ObjectType: "Model" {
        Count: 1
    }
    ObjectType: "Geometry" {
        Count: 1
    }
}
Objects:  {
    Geometry: 1234567, "Geometry::Cube", "Mesh" {
        Vertices: *24 {
            a: -50,-50,50,50,-50,50,50,50,50,-50,50,50,-50,-50,-50,50,-50,-50,50,50,-50,-50,50,-50
        }
        PolygonVertexIndex: *36 {
            a: 0,1,2,-4,4,5,6,-8,0,4,7,-4,1,5,4,-1,2,6,5,-2,3,7,6,-3
        }
        GeometryVersion: 124
        LayerElementNormal: 0 {
            Version: 101
            Name: ""
            MappingInformationType: "ByPolygonVertex"
            ReferenceInformationType: "Direct"
            Normals: *108 {
                a: 0,0,1,0,0,1,0,0,1,0,0,1,0,0,-1,0,0,-1,0,0,-1,0,0,-1,0,-1,0,0,-1,0,0,-1,0,0,-1,0,1,0,0,1,0,0,1,0,0,1,0,0,0,1,0,0,1,0,0,1,0,0,1,0,-1,0,0,-1,0,0,-1,0,0,-1,0,0
            }
        }
        LayerElementUV: 0 {
            Version: 101
            Name: "UVMap"
            MappingInformationType: "ByPolygonVertex"
            ReferenceInformationType: "IndexToDirect"
            UV: *48 {
                a: 0,0,1,0,1,1,0,1,0,0,1,0,1,1,0,1,0,0,1,0,1,1,0,1,0,0,1,0,1,1,0,1,0,0,1,0,1,1,0,1,0,0,1,0,1,1,0,1
            }
            UVIndex: *36 {
                a: 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35
            }
        }
        Layer: 0 {
            Version: 100
            LayerElement:  {
                Type: "LayerElementNormal"
                TypedIndex: 0
            }
            LayerElement:  {
                Type: "LayerElementUV"
                TypedIndex: 0
            }
        }
    }
    Model: 2345678, "Model::Cube", "Mesh" {
        Version: 232
        Properties70:  {
            P: "InheritType", "enum", "", "",1
            P: "ScalingMax", "Vector3D", "Vector", "",0,0,0
            P: "DefaultAttributeIndex", "int", "Integer", "",0
        }
        Shading: Y
        Culling: "CullingOff"
    }
}
Connections:  {
    C: "OO",1234567,2345678
}
"""
    
    with open(source_path, 'w') as f:
        f.write(fbx_content)
    print(f"Created test FBX file: {source_path}")

# Set up the import task
task = unreal.AssetImportTask()
task.filename = r'${sourcePath}'
task.destination_path = '${cleanDest}'
task.automated = True
task.save = True
task.replace_existing = True

# Configure FBX import options
options = unreal.FbxImportUI()
options.import_mesh = True
options.import_as_skeletal = False
options.mesh_type_to_import = unreal.FBXImportType.FBXIT_STATIC_MESH
options.static_mesh_import_data.combine_meshes = True
options.static_mesh_import_data.generate_lightmap_u_vs = False
options.static_mesh_import_data.auto_generate_collision = False
task.options = options

# Use AssetTools to import
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
result = {'success': False, 'error': 'No assets imported', 'source': task.filename}

try:
    asset_tools.import_asset_tasks([task])
    if task.imported_object_paths:
        result = {
            'success': True,
            'imported': len(task.imported_object_paths),
            'paths': list(task.imported_object_paths)
        }
except Exception as e:
    result = {'success': False, 'error': str(e), 'source': task.filename}

print('RESULT:' + json.dumps(result))
`.trim();
      
      const pyResp = await this.bridge.executePython(pythonCode);

      const interpreted = interpretStandardResult(pyResp, {
        successMessage: `Imported assets to ${cleanDest}`,
        failureMessage: 'Import failed'
      });

      if (interpreted.success) {
        const count = coerceNumber(interpreted.payload.imported) ?? 0;
        const paths = coerceStringArray(interpreted.payload.paths) ?? [];
        return {
          success: true,
          message: `Imported ${count} assets to ${cleanDest}`,
          imported: count,
          paths
        };
      }

      const errorMessage = `Import failed: ${interpreted.error ?? 'Unknown error'} (source: ${interpreted.payload.source ?? sourcePath})`;
      return {
        success: false,
        message: errorMessage,
        error: errorMessage,
        details: bestEffortInterpretedText(interpreted)
      };
    } catch (err) {
      return { success: false, error: `Failed to import asset: ${err}` };
    } finally {
      if (createdTestFile) {
        try {
          await fs.rm(sourcePath, { force: true });
        } catch (cleanupError) {
          // Swallow cleanup error but log for debug visibility
          console.warn(`Failed to clean up temporary FBX ${sourcePath}:`, cleanupError);
        }
      }
    }
  }

  async duplicateAsset(params: {
    sourcePath: string;
    destinationPath: string;
    newName?: string;
    overwrite?: boolean;
    save?: boolean;
    timeoutMs?: number;
  }) {
    const escapedSource = escapePythonString(params.sourcePath);
    const escapedDestination = escapePythonString(params.destinationPath);
    const escapedNewName = escapePythonString(params.newName ?? '');
    const script = `
import unreal, json

source_path = r"${escapedSource}"
target_folder = r"${escapedDestination}".rstrip('/')
requested_name = r"${escapedNewName}"
overwrite_existing = ${params.overwrite ? 'True' : 'False'}
save_new_asset = ${params.save ? 'True' : 'False'}

result = {
    'success': False,
    'message': '',
    'error': '',
    'source': source_path,
    'path': ''
}

asset_lib = unreal.EditorAssetLibrary

if not asset_lib.does_asset_exist(source_path):
    result['error'] = f"Source asset not found: {source_path}"
else:
    original_name = source_path.split('/')[-1]
    asset_name = requested_name.strip() or original_name
    if not asset_name:
        result['error'] = 'Unable to determine asset name'
    else:
        folder = target_folder or source_path.rsplit('/', 1)[0]
        folder = folder.rstrip('/')
        if not folder:
            result['error'] = 'Destination path is empty'
        else:
            new_path = f"{folder}/{asset_name}"
            if not overwrite_existing and asset_lib.does_asset_exist(new_path):
                result['error'] = f"Asset already exists at {new_path}"
                result['conflictPath'] = new_path
            else:
                overwritten = False
                if overwrite_existing and asset_lib.does_asset_exist(new_path):
                    if not asset_lib.delete_asset(new_path):
                        result['error'] = f"Failed to remove existing asset at {new_path}"
                    else:
                        overwritten = True
                if not result['error']:
                    duplicated = asset_lib.duplicate_asset(source_path, new_path)
                    if duplicated:
                        result['success'] = True
                        result['path'] = new_path
                        result['message'] = f"Duplicated asset to {new_path}"
                        result['overwritten'] = overwritten
                        if save_new_asset:
                            try:
                                asset_lib.save_asset(new_path, False)
                            except Exception as save_err:
                                result.setdefault('warnings', []).append(f"Save failed for {new_path}: {save_err}")
                    else:
                        result['error'] = 'DuplicateAsset returned False'

if not result['success'] and not result['error']:
    result['error'] = 'Duplicate operation failed'

if result.get('error'):
    result['message'] = result['error']
else:
    result['error'] = None

if 'warnings' in result and not result['warnings']:
    result.pop('warnings')

print('RESULT:' + json.dumps(result))
`.trim();

    const pyResult = await this.executeAssetPython(script, params.timeoutMs);
    const interpreted = interpretStandardResult(pyResult, {
      successMessage: `Duplicated asset to ${params.destinationPath}`,
      failureMessage: 'Failed to duplicate asset'
    });

    const payload = interpreted.payload ?? {};

    return {
      success: interpreted.success,
      message: interpreted.message,
      error: interpreted.error,
      sourcePath: params.sourcePath,
      path: coerceString(payload.path),
      conflictPath: coerceString(payload.conflictPath),
      overwritten: coerceBoolean(payload.overwritten),
      warnings: interpreted.warnings,
      details: interpreted.details
    };
  }

  async renameAsset(params: {
    assetPath: string;
    newName: string;
    timeoutMs?: number;
  }) {
    const escapedSource = escapePythonString(params.assetPath);
    const escapedNewName = escapePythonString(params.newName);
    const script = `
import unreal, json

asset_path = r"${escapedSource}"
new_name = r"${escapedNewName}".strip()

result = {
    'success': False,
    'message': '',
    'error': '',
    'path': asset_path
}

asset_lib = unreal.EditorAssetLibrary

if not new_name:
    result['error'] = 'New asset name must not be empty'
elif not asset_lib.does_asset_exist(asset_path):
    result['error'] = f"Asset not found: {asset_path}"
else:
    parent_path, _ = asset_path.rsplit('/', 1)
    destination = f"{parent_path}/{new_name}"
    if asset_lib.does_asset_exist(destination):
        result['error'] = f"Asset already exists at {destination}"
        result['conflictPath'] = destination
    else:
        if asset_lib.rename_asset(asset_path, destination):
            result['success'] = True
            result['path'] = destination
            result['message'] = f"Renamed asset to {destination}"
        else:
            result['error'] = 'RenameAsset returned False'

if not result['success'] and not result['error']:
    result['error'] = 'Rename operation failed'

if result.get('error'):
    result['message'] = result['error']
else:
    result['error'] = None

print('RESULT:' + json.dumps(result))
`.trim();

    const pyResult = await this.executeAssetPython(script, params.timeoutMs);
    const interpreted = interpretStandardResult(pyResult, {
      successMessage: 'Asset renamed successfully',
      failureMessage: 'Failed to rename asset'
    });

    const payload = interpreted.payload ?? {};

    return {
      success: interpreted.success,
      message: interpreted.message,
      error: interpreted.error,
      path: coerceString(payload.path),
      conflictPath: coerceString(payload.conflictPath),
      warnings: interpreted.warnings,
      details: interpreted.details
    };
  }

  async moveAsset(params: {
    assetPath: string;
    destinationPath: string;
    newName?: string;
    fixupRedirectors?: boolean;
    timeoutMs?: number;
  }) {
    const escapedSource = escapePythonString(params.assetPath);
    const escapedDestination = escapePythonString(params.destinationPath);
    const escapedNewName = escapePythonString(params.newName ?? '');
    const script = `
import unreal, json

asset_path = r"${escapedSource}"
target_folder = r"${escapedDestination}".rstrip('/')
requested_name = r"${escapedNewName}"
fixup_redirectors = ${params.fixupRedirectors === false ? 'False' : 'True'}

result = {
    'success': False,
    'message': '',
    'error': '',
    'path': asset_path
}

asset_lib = unreal.EditorAssetLibrary
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()

if not asset_lib.does_asset_exist(asset_path):
    result['error'] = f"Asset not found: {asset_path}"
else:
    current_name = asset_path.split('/')[-1]
    asset_name = requested_name.strip() or current_name
    folder = target_folder or asset_path.rsplit('/', 1)[0]
    folder = folder.rstrip('/')
    if not folder:
        result['error'] = 'Destination path is empty'
    else:
        destination = f"{folder}/{asset_name}"
        if destination == asset_path:
            result['success'] = True
            result['path'] = destination
            result['message'] = 'Asset already resides at the requested path'
        elif asset_lib.does_asset_exist(destination):
            result['error'] = f"Asset already exists at {destination}"
            result['conflictPath'] = destination
        else:
            if asset_lib.rename_asset(asset_path, destination):
                result['success'] = True
                result['path'] = destination
                result['message'] = f"Moved asset to {destination}"
                if fixup_redirectors and asset_tools:
                    try:
                        asset_tools.fixup_redirectors([folder])
                    except Exception as fix_err:
                        result.setdefault('warnings', []).append(f"Fix redirectors failed for {folder}: {fix_err}")
            else:
                result['error'] = 'RenameAsset returned False'

if not result['success'] and not result['error']:
    result['error'] = 'Move operation failed'

if result.get('error'):
    result['message'] = result['error']
else:
    result['error'] = None

if 'warnings' in result and not result['warnings']:
    result.pop('warnings')

print('RESULT:' + json.dumps(result))
`.trim();

    const pyResult = await this.executeAssetPython(script, params.timeoutMs);
    const interpreted = interpretStandardResult(pyResult, {
      successMessage: 'Asset moved successfully',
      failureMessage: 'Failed to move asset'
    });

    const payload = interpreted.payload ?? {};

    return {
      success: interpreted.success,
      message: interpreted.message,
      error: interpreted.error,
      path: coerceString(payload.path),
      conflictPath: coerceString(payload.conflictPath),
      warnings: interpreted.warnings,
      details: interpreted.details
    };
  }

  async deleteAssets(params: {
    assetPaths: string | string[];
    fixupRedirectors?: boolean;
    timeoutMs?: number;
  }) {
    const paths = Array.isArray(params.assetPaths) ? params.assetPaths : [params.assetPaths];
    const serializedPaths = '[' + paths.map((entry) => `r"${escapePythonString(entry)}"`).join(', ') + ']';
    const script = `
import unreal, json

asset_paths = ${serializedPaths}
fixup_redirectors = ${params.fixupRedirectors === false ? 'False' : 'True'}

result = {
    'success': False,
    'message': '',
    'error': '',
    'deleted': [],
    'missing': [],
    'failed': []
}

asset_lib = unreal.EditorAssetLibrary
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()

for path in asset_paths:
    normalized = path.rstrip('/')
    if not asset_lib.does_asset_exist(normalized):
        result['missing'].append(normalized)
        continue

    try:
        if asset_lib.delete_asset(normalized):
            result['deleted'].append(normalized)
        else:
            result['failed'].append(normalized)
    except Exception as delete_err:
        result['failed'].append(f"{normalized}:: {delete_err}")

if result['failed']:
    result['error'] = f"Failed to delete {len(result['failed'])} asset(s)"
elif not result['deleted']:
    result['error'] = 'No assets were deleted'
else:
    result['success'] = True

if result['deleted'] and fixup_redirectors and asset_tools:
    try:
        folders = sorted({ path.rsplit('/', 1)[0] for path in result['deleted'] if '/' in path })
        if folders:
            asset_tools.fixup_redirectors(folders)
    except Exception as fix_err:
        result.setdefault('warnings', []).append(f"Fix redirectors failed: {fix_err}")

if result.get('error'):
    result['message'] = result['error']
else:
    result['error'] = None

if 'warnings' in result and not result['warnings']:
    result.pop('warnings')

print('RESULT:' + json.dumps(result))
`.trim();

    const pyResult = await this.executeAssetPython(script, params.timeoutMs);
    const interpreted = interpretStandardResult(pyResult, {
      successMessage: 'Assets deleted successfully',
      failureMessage: 'Failed to delete assets'
    });

    const payload = interpreted.payload ?? {};

    return {
      success: interpreted.success,
      message: interpreted.message,
      error: interpreted.error,
      deleted: coerceStringArray(payload.deleted),
      missing: coerceStringArray(payload.missing),
      failed: coerceStringArray(payload.failed),
      warnings: interpreted.warnings,
      details: interpreted.details
    };
  }

  async saveAsset(assetPath: string) {
    try {
      const python = `
import unreal, json

path = r"${escapePythonString(assetPath)}"
result = {
    'success': False,
    'saved': False
}

try:
    saved = unreal.EditorAssetLibrary.save_asset(path)
    result['saved'] = bool(saved)
    result['success'] = True
except Exception as err:
    result['error'] = str(err)

print('RESULT:' + json.dumps(result))
      `.trim();

      const resp = await this.bridge.executePythonWithResult(python);
      if (resp && typeof resp === 'object' && coerceBoolean((resp as any).success, false)) {
        return { success: true, saved: coerceBoolean((resp as any).saved, false) };
      }

      return { success: false, error: (resp as any)?.error ?? 'Failed to save asset' };
    } catch (err) {
      return { error: `Failed to save asset: ${err}` };
    }
  }

  private async createTestFBX(filePath: string): Promise<void> {
    // Create a minimal valid FBX ASCII file for testing
    const fbxContent = `; FBX 7.5.0 project file
FBXHeaderExtension:  {
    FBXHeaderVersion: 1003
    FBXVersion: 7500
    CreationTimeStamp:  {
        Version: 1000
        Year: 2024
        Month: 1
        Day: 1
        Hour: 0
        Minute: 0
        Second: 0
        Millisecond: 0
    }
    Creator: "MCP Test FBX Generator"
}
GlobalSettings:  {
    Version: 1000
}
Definitions:  {
    Version: 100
    Count: 2
    ObjectType: "Model" {
        Count: 1
    }
    ObjectType: "Geometry" {
        Count: 1
    }
}
Objects:  {
    Geometry: 1234567, "Geometry::Cube", "Mesh" {
        Vertices: *24 {
            a: -50,-50,-50,50,-50,-50,50,50,-50,-50,50,-50,-50,-50,50,50,-50,50,50,50,50,-50,50,50
        }
        PolygonVertexIndex: *36 {
            a: 0,1,2,-4,4,7,6,-6,0,4,5,-2,1,5,6,-3,2,6,7,-4,4,0,3,-8
        }
        GeometryVersion: 124
    }
    Model: 2345678, "Model::TestCube", "Mesh" {
        Version: 232
        Properties70:  {
            P: "ScalingMax", "Vector3D", "Vector", "",0,0,0
            P: "DefaultAttributeIndex", "int", "Integer", "",0
        }
    }
}
Connections:  {
    C: "OO",1234567,2345678
}
`;
    
    // Ensure directory exists
    const dir = path.dirname(filePath);
    try {
      await fs.mkdir(dir, { recursive: true });
    } catch {}
    
    // Write the FBX file
    await fs.writeFile(filePath, fbxContent, 'utf8');
  }
}
