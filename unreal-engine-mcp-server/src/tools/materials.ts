import { UnrealBridge } from '../unreal-bridge.js';

export class MaterialTools {
  constructor(private bridge: UnrealBridge) {}

  async createMaterial(name: string, path: string) {
    try {
      // Use Python API to create material
      const materialPath = `${path}/${name}`;
      // Use the correct Unreal Engine 5 Python API
      const pythonCode = `
import unreal

try:
    # Get the AssetTools
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    
    # Create a MaterialFactoryNew
    factory = unreal.MaterialFactoryNew()
    
    # Create the material asset at the specified path
    # The path should be: /Game/FolderName and asset name separately
    asset = asset_tools.create_asset(
        asset_name='${name}',
        package_path='${path}',
        asset_class=unreal.Material,
        factory=factory
    )
    
    if asset:
        print("Material created successfully: ${materialPath}")
        # Save the package
        unreal.EditorAssetLibrary.save_asset('${materialPath}')
        print("Material saved: ${materialPath}")
    else:
        print("Failed to create material")
except Exception as e:
    print(f"Error creating material: {e}")
`.trim();
      
      const pyResult = await this.bridge.executePython(pythonCode);

      // Verify creation using EditorAssetLibrary
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
      if (!exists) {
        return { success: false, error: `Material not found after creation at ${materialPath}. Check Output Log for Python errors.`, debug: pyResult };
      }
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
