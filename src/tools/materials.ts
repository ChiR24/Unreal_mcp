import { UnrealBridge } from '../unreal-bridge.js';

export class MaterialTools {
  constructor(private bridge: UnrealBridge) {}

  async createMaterial(name: string, path: string) {
    try {
      // Validate name
      if (!name || name.trim() === '') {
        return { success: false, error: 'Material name cannot be empty' };
      }
      
      // Check name length (Unreal has 260 char path limit)
      if (name.length > 100) {
        return { success: false, error: `Material name too long (${name.length} chars). Maximum is 100 characters.` };
      }
      
      // Validate name doesn't contain invalid characters
      // Unreal Engine doesn't allow: spaces, dots, slashes, backslashes, pipes, angle brackets, 
      // curly braces, square brackets, parentheses, @, #, etc.
      const invalidChars = /[\s\.\/\<>\|\{\}\[\]\(\)@#\\]/;
      if (invalidChars.test(name)) {
        const foundChars = name.match(invalidChars);
        return { success: false, error: `Material name contains invalid characters: '${foundChars?.[0]}'. Avoid spaces, dots, slashes, backslashes, brackets, and special symbols.` };
      }
      
      // Validate path type
      if (typeof path !== 'string') {
        return { success: false, error: `Invalid path type: expected string, got ${typeof path}` };
      }
      
      // Clean up path - remove trailing slashes
      const cleanPath = path.replace(/\/$/, '');
      
      // Validate path starts with /Game or /Engine
      if (!cleanPath.startsWith('/Game') && !cleanPath.startsWith('/Engine')) {
        return { success: false, error: `Invalid path: must start with /Game or /Engine, got ${cleanPath}` };
      }
      
      // Use Python API to create material
      const materialPath = `${cleanPath}/${name}`;
      // Use the correct Unreal Engine 5 Python API
      const pythonCode = `
import unreal
import json

try:
    # Check if material already exists
    material_path = '${materialPath}'
    if unreal.EditorAssetLibrary.does_asset_exist(material_path):
        print(json.dumps({"success": True, "exists": True, "path": material_path}))
    else:
        # Get the AssetTools
        asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
        
        # Create a MaterialFactoryNew
        factory = unreal.MaterialFactoryNew()
        
        # Clean up the path - remove trailing slashes
        clean_path = '${cleanPath}'.rstrip('/')
        
        # Create the material asset at the specified path
        # The path should be: /Game/FolderName and asset name separately
        asset = asset_tools.create_asset(
            asset_name='${name}',
            package_path=clean_path,
            asset_class=unreal.Material,
            factory=factory
        )
        
        if asset:
            # Save the package
            unreal.EditorAssetLibrary.save_asset(material_path)
            print(json.dumps({"success": True, "created": True, "path": material_path}))
        else:
            print(json.dumps({"success": False, "error": "Failed to create material"}))
except Exception as e:
    print(json.dumps({"success": False, "error": str(e)}))
`.trim();
      
      const pyResult = await this.bridge.executePython(pythonCode);
      
      // Parse the Python response
      let responseStr = '';
      if (pyResult?.LogOutput && Array.isArray(pyResult.LogOutput)) {
        responseStr = pyResult.LogOutput.map((log: any) => log.Output || '').join('');
      } else if (typeof pyResult === 'string') {
        responseStr = pyResult;
      } else {
        responseStr = JSON.stringify(pyResult);
      }
      
      // Try to extract JSON response
      try {
        // Look for JSON in the output
        const jsonMatch = responseStr.match(/\{.*\}/s);
        if (jsonMatch) {
          const result = JSON.parse(jsonMatch[0]);
          if (result.success) {
            if (result.exists) {
              return { success: true, path: materialPath, message: `Material ${name} already exists at ${path}` };
            } else if (result.created) {
              return { success: true, path: materialPath, message: `Material ${name} created at ${path}` };
            }
          } else {
            return { success: false, error: result.error || 'Failed to create material' };
          }
        }
      } catch (parseErr) {
        // JSON parsing failed, fall back to verification
      }

      // Fallback: Verify creation using EditorAssetLibrary
      let verify: any = {};
      try {
        verify = await this.bridge.call({
          objectPath: '/Script/EditorScriptingUtilities.Default__EditorAssetLibrary',
          functionName: 'DoesAssetExist',
          parameters: {
            AssetPath: materialPath
          }
        });
      } catch {}

      const exists = verify?.ReturnValue === true || verify?.Result === true;
      if (exists) {
        return { success: true, path: materialPath, message: `Material ${name} created at ${path}` };
      } else {
        return { success: false, error: `Material creation may have failed. Check Output Log for details.`, debug: responseStr };
      }
    } catch (err) {
      return { success: false, error: `Failed to create material: ${err}` };
    }
  }

  async applyMaterialToActor(actorPath: string, materialPath: string, slotIndex = 0) {
    try {
      const res = await this.bridge.httpCall('/remote/object/property', 'PUT', {
        objectPath: actorPath,
        propertyName: `StaticMeshComponent.Materials[${slotIndex}]`,
        propertyValue: materialPath
      });
      return { success: true, message: 'Material applied' };
    } catch (err) {
      return { success: false, error: `Failed to apply material: ${err}` };
    }
  }
}
