import { UnrealBridge } from '../unreal-bridge.js';
import { AutomationBridge } from '../automation-bridge.js';
import { coerceBoolean, interpretStandardResult } from '../utils/result-helpers.js';
import { allowPythonFallbackFromEnv } from '../utils/env.js';
import { escapePythonString } from '../utils/python.js';

export class MaterialTools {
  constructor(private bridge: UnrealBridge, private automationBridge?: AutomationBridge) {}

  setAutomationBridge(automationBridge?: AutomationBridge) {
    this.automationBridge = automationBridge;
  }

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
    // Try plugin-first transport if available
    if (this.automationBridge && typeof this.automationBridge.sendAutomationRequest === 'function') {
      try {
        const resp: any = await this.automationBridge.sendAutomationRequest('create_material', {
          name,
          destinationPath: cleanPath
        });
        if (resp && resp.success !== false) {
          return { success: true, path: resp.path || resp.result?.path || materialPath, message: resp.message || `Material ${name} created at ${materialPath}`, warnings: resp.warnings } as any;
        }
        const errTxt = String(resp?.error ?? resp?.message ?? '');
        if (!(errTxt.toLowerCase().includes('unknown') || errTxt.includes('UNKNOWN_PLUGIN_ACTION'))) {
          return { success: false, error: resp?.error ?? resp?.message ?? 'CREATE_MATERIAL_FAILED' } as any;
        }
      } catch (_e) {
        // Fall back to Python path below
      }
    }
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

  const pyResult = await (this.bridge as any).executeEditorPython(pythonCode, { allowPythonFallback: allowPythonFallbackFromEnv() });
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
      const result = await this.bridge.setObjectProperty({
        objectPath: actorPath,
        propertyName: `StaticMeshComponent.Materials[${slotIndex}]`,
        value: materialPath
      });

      if (result.success) {
        return {
          success: true,
          message: 'Material applied',
          transport: result.transport,
          warnings: Array.isArray(result.warnings) ? result.warnings : undefined
        };
      }

      return {
        success: false,
        error: result.error ?? `Failed to apply material to ${actorPath}`,
        transport: result.transport
      };
    } catch (err) {
      return { success: false, error: `Failed to apply material: ${err}` };
    }
  }

  async createMaterialInstance(name: string, path: string, parentMaterial: string, parameters?: Record<string, any>) {
    try {
      if (!name || name.trim() === '') {
        return { success: false, error: 'Material instance name cannot be empty' };
      }
      const cleanPath = (path || '/Game').replace(/\/$/, '');
      if (!cleanPath.startsWith('/Game') && !cleanPath.startsWith('/Engine')) {
        return { success: false, error: `Invalid path: must start with /Game or /Engine, got ${cleanPath}` };
      }

      // Plugin-first attempt
      if (this.automationBridge && typeof this.automationBridge.sendAutomationRequest === 'function') {
        try {
          const resp: any = await this.automationBridge.sendAutomationRequest('create_material_instance', {
            name,
            destinationPath: cleanPath,
            parentMaterial,
            parameters
          });
          if (resp && resp.success !== false) {
            return { success: true, path: resp.path || resp.result?.path || `${cleanPath}/${name}`, message: resp.message || `Material instance ${name} created at ${cleanPath}/${name}`, warnings: resp.warnings } as any;
          }
          const errTxt = String(resp?.error ?? resp?.message ?? '');
          if (!(errTxt.toLowerCase().includes('unknown') || errTxt.includes('UNKNOWN_PLUGIN_ACTION'))) {
            return { success: false, error: resp?.error ?? resp?.message ?? 'CREATE_MATERIAL_INSTANCE_FAILED' } as any;
          }
        } catch (_e) {
          // fall back to python path below
        }
      }

      // Fallback: use the centralized editor function template for material instance
      const createParams = {
        asset_name: name,
        package_path: cleanPath,
        factory_class: 'MaterialInstanceConstantFactoryNew',
        asset_class: 'MaterialInstanceConstant',
        parent_material: parentMaterial,
        parameters: parameters || {}
      };

  const allowPythonFallback = allowPythonFallbackFromEnv();

      // Try a plugin-native asset creation path via executeEditorFunction which will
      // call into plugin handlers first and only run the Python template when
      // explicitly allowed by environment. If the plugin returns UNKNOWN_PLUGIN_ACTION
      // this will preserve explicit failure semantics so callers can migrate.
      try {
        const res = await this.bridge.executeEditorFunction('CREATE_ASSET', createParams, { allowPythonFallback });
        if (res && res.success !== false) {
          const createdPath = res.path || res.result?.path || `${cleanPath}/${name}`;
          // Attempt to set parent via plugin-native setObjectProperty when available
          if (parentMaterial) {
            try {
              await this.bridge.setObjectProperty({ objectPath: createdPath, propertyName: 'parent', value: parentMaterial, markDirty: true });
            } catch (_) {
              // Ignore failure to set parent - template may have already set it
            }
          }
          // Save asset (plugin-first via SAVE_ASSET template)
          try {
            await this.bridge.executeEditorFunction('SAVE_ASSET', { path: createdPath }, { allowPythonFallback });
          } catch (_) {}
          return { success: true, path: createdPath, message: `Material instance ${name} created at ${createdPath}` } as any;
        }
        // If plugin indicated unknown action and python fallback not allowed, surface explicit failure
        const errTxt = String((res as any)?.error ?? (res as any)?.message ?? '');
        if (errTxt.toLowerCase().includes('unknown') || errTxt.includes('UNKNOWN_PLUGIN_ACTION')) {
          return { success: false, error: 'UNKNOWN_PLUGIN_ACTION', message: 'Automation plugin does not implement create_material_instance' } as any;
        }
        return { success: false, error: (res as any)?.error ?? (res as any)?.message ?? 'CREATE_MATERIAL_INSTANCE_FAILED' } as any;
      } catch (err) {
        // If executeEditorFunction threw due to bridge not connected or python disabled,
        // preserve previous behavior by returning structured failure.
        return { success: false, error: String(err) || 'CREATE_MATERIAL_INSTANCE_FAILED' } as any;
      }
    } catch (err) {
      return { success: false, error: `Failed to create material instance: ${err}` };
    }
  }

  private async assetExists(assetPath: string): Promise<boolean> {
    try {
      return await this.bridge.assetExists(assetPath);
    } catch {
      // ignored, fall through to false
    }

    return false;
  }
}
