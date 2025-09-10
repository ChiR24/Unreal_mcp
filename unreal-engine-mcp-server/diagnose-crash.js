// Diagnose which operations cause Unreal Engine to crash
import { createServer } from './dist/index.js';

async function diagnoseCrash() {
  console.log('='.repeat(60));
  console.log('UNREAL ENGINE CRASH DIAGNOSTIC');
  console.log('Testing operations one by one to identify crash triggers');
  console.log('='.repeat(60));
  
  const { server, bridge } = await createServer();
  
  const call = async (tool, args, description) => {
    console.log(`\nTesting: ${description}`);
    console.log('  Tool:', tool);
    console.log('  Args:', JSON.stringify(args, null, 2));
    
    try {
      const h = server._requestHandlers.get('tools/call');
      const result = await h({ method: 'tools/call', params: { name: tool, arguments: args } });
      console.log('  ✅ Success - No crash');
      return true;
    } catch (error) {
      console.log('  ⚠️ Error (but no crash):', error.message?.substring(0, 50));
      return false;
    }
  };
  
  // Wait for connection
  console.log('\nWaiting for connection...');
  await new Promise(resolve => setTimeout(resolve, 2000));
  
  // Test safe operations first
  console.log('\n' + '='.repeat(40));
  console.log('PHASE 1: Testing known safe operations');
  console.log('='.repeat(40));
  
  await call('console_command', { command: 'stat fps' }, 'Simple console command');
  await new Promise(resolve => setTimeout(resolve, 500));
  
  await call('control_actor', { 
    action: 'spawn', 
    classPath: 'StaticMeshActor', 
    location: { x: 0, y: 0, z: 0 } 
  }, 'Spawn static mesh actor');
  await new Promise(resolve => setTimeout(resolve, 500));
  
  // Test potentially problematic operations
  console.log('\n' + '='.repeat(40));
  console.log('PHASE 2: Testing potentially problematic operations');
  console.log('='.repeat(40));
  
  // Test 1: Asset listing (we know this has issues)
  await call('manage_asset', { 
    action: 'list', 
    directory: '/Game', 
    recursive: false 
  }, 'List assets non-recursively');
  await new Promise(resolve => setTimeout(resolve, 1000));
  
  // Test 2: Material creation (failed in comprehensive test)
  console.log('\n⚠️ CAUTION: Next test may crash Unreal');
  await new Promise(resolve => setTimeout(resolve, 2000));
  
  await call('manage_asset', { 
    action: 'create_material', 
    name: 'M_TestCrash', 
    path: '/Game/Materials' 
  }, 'Create material - POTENTIAL CRASH TRIGGER');
  await new Promise(resolve => setTimeout(resolve, 1000));
  
  // Test 3: Level load (caused connection loss)
  console.log('\n⚠️ CAUTION: Next test may crash Unreal');
  await new Promise(resolve => setTimeout(resolve, 2000));
  
  await call('manage_level', { 
    action: 'load', 
    levelPath: '/Game/ThirdPerson/Maps/ThirdPersonMap' 
  }, 'Load level - POTENTIAL CRASH TRIGGER');
  await new Promise(resolve => setTimeout(resolve, 1000));
  
  // Test 4: Debug shape (failed in comprehensive test)
  console.log('\n⚠️ CAUTION: Next test may crash Unreal');
  await new Promise(resolve => setTimeout(resolve, 2000));
  
  await call('create_effect', { 
    action: 'debug_shape', 
    shape: 'Sphere', 
    location: { x: 0, y: 0, z: 200 }, 
    size: 50, 
    color: [1, 0, 0, 1], 
    duration: 5 
  }, 'Draw debug shape - POTENTIAL CRASH TRIGGER');
  await new Promise(resolve => setTimeout(resolve, 1000));
  
  // Test 5: Landscape creation (completely broken)
  console.log('\n⚠️ CAUTION: Next test may crash Unreal');
  await new Promise(resolve => setTimeout(resolve, 2000));
  
  await call('build_environment', { 
    action: 'create_landscape', 
    name: 'TestTerrain', 
    sizeX: 63, 
    sizeY: 63 
  }, 'Create landscape - POTENTIAL CRASH TRIGGER');
  await new Promise(resolve => setTimeout(resolve, 1000));
  
  console.log('\n' + '='.repeat(60));
  console.log('Diagnostic complete - If you see this, no crash occurred');
  console.log('='.repeat(60));
  
  setTimeout(() => {
    bridge.ws?.close();
    process.exit(0);
  }, 1000);
}

// Add crash handler
process.on('uncaughtException', (error) => {
  console.error('\n❌ UNCAUGHT EXCEPTION - Possible UE crash trigger found');
  console.error('Error:', error);
  process.exit(1);
});

process.on('unhandledRejection', (reason, promise) => {
  console.error('\n❌ UNHANDLED REJECTION - Possible UE crash trigger found');
  console.error('Reason:', reason);
  process.exit(1);
});

diagnoseCrash().catch(error => {
  console.error('\n❌ DIAGNOSTIC FAILED');
  console.error('Error:', error);
  process.exit(1);
});
