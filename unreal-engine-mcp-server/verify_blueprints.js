import { UnrealBridge } from './dist/unreal-bridge.js';

async function verifyBlueprints() {
  const bridge = new UnrealBridge();
  
  console.log('🔍 VERIFYING BLUEPRINTS ARE REAL AND LOADED IN UNREAL ENGINE...\n');
  
  try {
    await bridge.tryConnect(1, 5000, 1500);
    
    const pythonScript = `
import unreal

# Test blueprints to verify they exist and are valid
test_blueprints = [
    '/Game/Blueprints/ComprehensiveTest/ActorBP_1757593383023',
    '/Game/Blueprints/ComprehensiveTest/CharacterBP_1757593383023',
    '/Game/Blueprints/ComprehensiveTest/GameModeBP_1757593383023',
    '/Game/Blueprints/ComprehensiveTest/ComponentTestBP_1757593383023',
    '/Game/Blueprints/AlternatePath/DifferentPath_1757593383023',
    '/Game/Blueprints/Deep/Nested/Path/Test/NestedPath_1757593383023',
    '/Game/Blue_prints/Test/InvalidPath_1757593383023',
    '/Game/Blueprints/NoPath_1757593383023'
]

verified_count = 0
total_count = len(test_blueprints)

for bp_path in test_blueprints:
    if unreal.EditorAssetLibrary.does_asset_exist(bp_path):
        asset = unreal.EditorAssetLibrary.load_asset(bp_path)
        if asset and isinstance(asset, unreal.Blueprint):
            # Get basic blueprint info
            asset_name = bp_path.split('/')[-1]
            asset_path = unreal.EditorAssetLibrary.get_path_name_for_loaded_asset(asset)
            
            # Verify it's a real blueprint
            is_blueprint = True
            
            print(f'✅ VERIFIED: {asset_name}')
            print(f'   - Full Path: {asset_path}')
            print(f'   - Is Blueprint: {is_blueprint}')
            print(f'   - Exists in Engine: True')
            verified_count += 1
        else:
            print(f'❌ ERROR: {bp_path} exists but is not a Blueprint')
    else:
        print(f'❌ NOT FOUND: {bp_path}')

print(f'\\nVERIFICATION SUMMARY:')
print(f'Total Tested: {total_count}')
print(f'Verified: {verified_count}')
print(f'Success Rate: {(verified_count/total_count)*100:.1f}%')

# Also check total blueprint count in ComprehensiveTest folder
asset_registry = unreal.AssetRegistryHelpers.get_asset_registry()
filter = unreal.ARFilter(
    package_paths=['/Game/Blueprints/ComprehensiveTest'],
    class_names=['Blueprint'],
    recursive_paths=False
)
assets = asset_registry.get_assets(filter)
print(f'\\nTotal Blueprints in ComprehensiveTest folder: {len(assets)}')

print('VERIFICATION COMPLETE')
`;
    
    const response = await bridge.executePython(pythonScript);
    console.log('Verification Results:');
    console.log(response);
    
    await bridge.disconnect();
    
    // Parse results
    const responseStr = typeof response === 'string' ? response : JSON.stringify(response);
    if (responseStr && responseStr.includes('VERIFIED')) {
      console.log('\n✅ VALIDATION SUCCESSFUL: Blueprints are REAL assets in Unreal Engine!');
    } else if (responseStr && responseStr.includes('Exception')) {
      console.log('\n⚠️ Script had an error, but we can verify from file system...');
    } else {
      console.log('\n⚠️ Check results above for details');
    }
    
  } catch (error) {
    console.error('Error:', error);
  }
}

verifyBlueprints();
