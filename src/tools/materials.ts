import { UnrealBridge } from '../unreal-bridge.js';
import { coerceBoolean, interpretStandardResult } from '../utils/result-helpers.js';
import { escapePythonString } from '../utils/python.js';

export class MaterialTools {
  constructor(private bridge: UnrealBridge) {}

  async createMaterial(name: string, path: string) {
    try {
      if (!name || name.trim() === '') {
        return { success: false, error: 'Material name cannot be empty' };
      }

      if (name.length > 100) {
        return { success: false, error: `Material name too long (${name.length} chars). Maximum is 100 characters.` };
      }

      const invalidChars = /[\s./<>|{}[\]()@#\\]/;
      if (invalidChars.test(name)) {
        const foundChars = name.match(invalidChars);
        return { success: false, error: `Material name contains invalid characters: '${foundChars?.[0]}'. Avoid spaces, dots, slashes, backslashes, brackets, and special symbols.` };
      }

      if (typeof path !== 'string') {
        return { success: false, error: `Invalid path type: expected string, got ${typeof path}` };
      }

      const trimmedPath = path.trim();
      const effectivePath = trimmedPath.length === 0 ? '/Game' : trimmedPath;
      const cleanPath = effectivePath.replace(/\/$/, '');

      if (!cleanPath.startsWith('/Game') && !cleanPath.startsWith('/Engine')) {
        return { success: false, error: `Invalid path: must start with /Game or /Engine, got ${cleanPath}` };
      }

      const normalizedPath = cleanPath.toLowerCase();
      const restrictedPrefixes = ['/engine/restricted', '/engine/generated', '/engine/transient'];
      if (restrictedPrefixes.some(prefix => normalizedPath.startsWith(prefix))) {
        const errorMessage = `Destination path is read-only and cannot be used for material creation: ${cleanPath}`;
        return { success: false, error: errorMessage, message: errorMessage };
      }

    const materialPath = `${cleanPath}/${name}`;
    const payload = { name, cleanPath, materialPath };
    const escapedName = escapePythonString(name);
      const pythonCode = `
import unreal, json

payload = json.loads(r'''${JSON.stringify(payload)}''')
result = {
    'success': False,
    'message': '',
    'error': '',
    'warnings': [],
    'details': [],
    'name': payload.get('name') or "${escapedName}",
    'path': payload.get('materialPath')
}

material_path = result['path']
clean_path = payload.get('cleanPath') or '/Game'

try:
    if unreal.EditorAssetLibrary.does_asset_exist(material_path):
        result['success'] = True
        result['exists'] = True
        result['message'] = f"Material already exists at {material_path}"
    else:
        asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
        factory = unreal.MaterialFactoryNew()
        asset = asset_tools.create_asset(
            asset_name=payload.get('name'),
            package_path=clean_path,
            asset_class=unreal.Material,
            factory=factory
        )
        if asset:
            unreal.EditorAssetLibrary.save_asset(material_path)
            result['success'] = True
            result['created'] = True
            result['message'] = f"Material created at {material_path}"
        else:
            result['error'] = 'Failed to create material'
            result['message'] = result['error']
except Exception as exc:
    result['error'] = str(exc)
    if not result['message']:
        result['message'] = result['error']

print('RESULT:' + json.dumps(result))
`.trim();

      const pyResult = await this.bridge.executePython(pythonCode);
      const interpreted = interpretStandardResult(pyResult, {
        successMessage: `Material ${name} processed`,
        failureMessage: 'Failed to create material'
      });

      if (interpreted.success) {
        const exists = coerceBoolean(interpreted.payload.exists, false) === true;
        const created = coerceBoolean(interpreted.payload.created, false) === true;
        if (exists) {
          return { success: true, path: materialPath, message: `Material ${name} already exists at ${materialPath}` };
        }
        if (created) {
          return { success: true, path: materialPath, message: `Material ${name} created at ${materialPath}` };
        }
        return { success: true, path: materialPath, message: interpreted.message };
      }

      if (interpreted.error) {
        const exists = await this.assetExists(materialPath);
        if (exists) {
          return {
            success: true,
            path: materialPath,
            message: `Material ${name} created at ${materialPath}`,
            warnings: interpreted.warnings,
            details: interpreted.details
          };
        }
        return {
          success: false,
          error: interpreted.error,
          warnings: interpreted.warnings,
          details: interpreted.details
        };
      }

      const exists = await this.assetExists(materialPath);
      if (exists) {
        return {
          success: true,
          path: materialPath,
          message: `Material ${name} created at ${materialPath}`,
          warnings: interpreted.warnings,
          details: interpreted.details
        };
      }

      return {
        success: false,
        error: interpreted.message,
        warnings: interpreted.warnings,
        details: interpreted.details
      };
    } catch (err) {
      return { success: false, error: `Failed to create material: ${err}` };
    }
  }

  async applyMaterialToActor(actorPath: string, materialPath: string, slotIndex = 0) {
    try {
      await this.bridge.httpCall('/remote/object/property', 'PUT', {
        objectPath: actorPath,
        propertyName: `StaticMeshComponent.Materials[${slotIndex}]`,
        propertyValue: materialPath
      });
      return { success: true, message: 'Material applied' };
    } catch (err) {
      return { success: false, error: `Failed to apply material: ${err}` };
    }
  }

  private async assetExists(assetPath: string): Promise<boolean> {
    try {
      const response = await this.bridge.call({
        objectPath: '/Script/EditorScriptingUtilities.Default__EditorAssetLibrary',
        functionName: 'DoesAssetExist',
        parameters: {
          AssetPath: assetPath
        }
      });
      return coerceBoolean(response?.ReturnValue ?? response?.Result ?? response, false) === true;
    } catch {
      return false;
    }
  }
}
