import axios from 'axios';

const baseUrl = 'http://127.0.0.1:30010';

async function verifySetup() {
    console.log('🔍 Verifying Unreal Engine MCP Server Setup...\n');
    
    let passed = 0;
    let failed = 0;
    
    // 1. Check server connection
    try {
        const info = await axios.get(`${baseUrl}/remote/info`);
        console.log('✅ Connected to Unreal Engine Remote Control');
        console.log(`   Routes available: ${info.data.HttpRoutes?.length || 0}`);
        passed++;
    } catch (error) {
        console.log('❌ Cannot connect to Unreal Engine');
        console.log('   Make sure Unreal is running with Remote Control enabled');
        failed++;
        return;
    }
    
    // 2. Test console command
    try {
        await axios.put(`${baseUrl}/remote/object/call`, {
            objectPath: '/Script/Engine.Default__KismetSystemLibrary',
            functionName: 'ExecuteConsoleCommand',
            parameters: {
                Command: 'stat fps',
                SpecificPlayer: null
            },
            generateTransaction: false
        });
        console.log('✅ Console commands working');
        passed++;
        
        // Clear the stat
        await axios.put(`${baseUrl}/remote/object/call`, {
            objectPath: '/Script/Engine.Default__KismetSystemLibrary',
            functionName: 'ExecuteConsoleCommand',
            parameters: {
                Command: 'stat none',
                SpecificPlayer: null
            },
            generateTransaction: false
        });
    } catch (error) {
        console.log('❌ Console commands not working');
        console.log('   Enable bAllowConsoleCommandRemoteExecution in RemoteControl.ini');
        failed++;
    }
    
    // 3. Test asset search
    try {
        const assets = await axios.put(`${baseUrl}/remote/search/assets`, {
            Query: '',
            Limit: 5,
            Filter: {}
        });
        console.log(`✅ Asset search working (found ${assets.data.Assets?.length || 0} assets)`);
        passed++;
    } catch (error) {
        console.log('❌ Asset search failed');
        failed++;
    }
    
    // 4. Test object description
    try {
        const desc = await axios.put(`${baseUrl}/remote/object/describe`, {
            objectPath: '/Script/Engine.Default__GameplayStatics'
        });
        console.log(`✅ Object inspection working (${desc.data.Functions?.length || 0} functions)`);
        passed++;
    } catch (error) {
        console.log('❌ Object inspection failed');
        failed++;
    }
    
    // Summary
    console.log('\n' + '='.repeat(50));
    console.log(`RESULTS: ${passed} passed, ${failed} failed`);
    
    if (failed === 0) {
        console.log('\n🎉 MCP Server is READY for use!');
        console.log('You can now use it with Claude Desktop or Cursor.');
    } else {
        console.log('\n⚠️ Some features need configuration.');
        console.log('Check the errors above for details.');
    }
}

verifySetup().catch(console.error);
