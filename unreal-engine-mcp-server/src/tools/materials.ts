import { UnrealBridge } from '../unreal-bridge.js';

export class MaterialTools {
  constructor(private bridge: UnrealBridge) {}

  async createMaterial(name: string, path: string) {
    try {
      // Use Python API to create material
      const materialPath = `${path}/${name}`;
      const pythonCode = `
import unreal

# Create material using AssetTools
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
factory = unreal.MaterialFactoryNew()

# Create the material asset
package_path = '${path}'
asset_name = '${name}'

try:
    material = asset_tools.create_asset(asset_name, package_path, unreal.Material, factory)
    if material:
        unreal.EditorAssetLibrary.save_asset('${materialPath}')
        print(f"Material created successfully: ${materialPath}")
    else:
        print(f"Failed to create material: ${materialPath}")
except Exception as e:
    print(f"Error creating material: {str(e)}")
`.trim();
      
      const result = await this.bridge.executePython(pythonCode);
      return { success: true, path: materialPath, message: `Material ${name} created at ${path}` };
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
