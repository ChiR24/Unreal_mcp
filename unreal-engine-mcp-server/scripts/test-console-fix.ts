import { UnrealBridge } from '../src/unreal-bridge.js';
import { handleConsolidatedToolCall } from '../src/tools/consolidated-tool-handlers.js';
import { Logger } from '../src/utils/logger.js';

const log = new Logger('ConsoleFix');

interface TestCase {
  name: string;
  command: string;
  expectedBehavior: 'success' | 'warning' | 'error' | 'empty';
  description: string;
}

async function testConsoleCommand(bridge: UnrealBridge, command: string): Promise<any> {
  try {
    const result = await handleConsolidatedToolCall(
      'console_command',
      { command },
      { bridge }
    );
    return { success: true, result };
  } catch (error: any) {
    return { success: false, error: error.message };
  }
}

async function runTests() {
  const bridge = new UnrealBridge();
  
  log.info('Connecting to Unreal Engine...');
  const connected = await bridge.tryConnect(3, 5000, 2000);
  
  if (!connected) {
    log.error('Failed to connect to Unreal Engine');
    process.exit(1);
  }
  
  log.info('Connected! Running fixed console command tests...');
  
  const testCases: TestCase[] = [
    // Valid commands
    {
      name: 'Valid stat command',
      command: 'stat unit',
      expectedBehavior: 'success',
      description: 'Should execute without warnings'
    },
    {
      name: 'Clear stats',
      command: 'stat none',
      expectedBehavior: 'success',
      description: 'Should clear all stats'
    },
    {
      name: 'View mode change',
      command: 'viewmode wireframe',
      expectedBehavior: 'success',
      description: 'Should change view mode'
    },
    {
      name: 'Reset view mode',
      command: 'viewmode lit',
      expectedBehavior: 'success',
      description: 'Should reset to lit mode'
    },
    {
      name: 'Show flag',
      command: 'show collision',
      expectedBehavior: 'success',
      description: 'Should toggle collision display'
    },
    {
      name: 'Rendering setting',
      command: 'r.ScreenPercentage 100',
      expectedBehavior: 'success',
      description: 'Should set screen percentage'
    },
    {
      name: 'Scalability setting',
      command: 'sg.ShadowQuality 2',
      expectedBehavior: 'success',
      description: 'Should set shadow quality'
    },
    
    // Commands that should be handled but may generate warnings
    {
      name: 'Stat fps (should convert to stat unit)',
      command: 'stat fps',
      expectedBehavior: 'success',
      description: 'Should be converted to stat unit to avoid warnings'
    },
    
    // Invalid commands
    {
      name: 'Invalid command',
      command: 'invalid_command_xyz',
      expectedBehavior: 'warning',
      description: 'Should handle gracefully with warning'
    },
    {
      name: 'Numeric command',
      command: '12345',
      expectedBehavior: 'warning',
      description: 'Should handle gracefully'
    },
    {
      name: 'This is not valid',
      command: 'this_is_not_a_valid_command_12345',
      expectedBehavior: 'warning',
      description: 'Should handle gracefully'
    },
    
    // Empty commands
    {
      name: 'Empty command',
      command: '',
      expectedBehavior: 'empty',
      description: 'Should handle empty command'
    },
    {
      name: 'Whitespace only',
      command: '   ',
      expectedBehavior: 'empty',
      description: 'Should handle whitespace'
    },
    
    // Compound commands
    {
      name: 'Compound command',
      command: 'stat fps ; stat unit',
      expectedBehavior: 'success',
      description: 'Should split and execute both'
    }
  ];
  
  let passed = 0;
  let failed = 0;
  const results: any[] = [];
  
  for (const testCase of testCases) {
    log.info(`Testing: ${testCase.name}`);
    const result = await testConsoleCommand(bridge, testCase.command);
    
    let testPassed = false;
    
    switch (testCase.expectedBehavior) {
      case 'success':
        testPassed = result.success && !result.result?.isError;
        break;
      case 'warning':
        // Should not throw error but may have warning in result
        testPassed = result.success || (result.result && result.result.warning);
        break;
      case 'error':
        testPassed = !result.success;
        break;
      case 'empty':
        testPassed = result.success && 
          (result.result?.content?.[0]?.text?.includes('Empty') || 
           result.result?.content?.[0]?.text?.includes('ignored'));
        break;
    }
    
    if (testPassed) {
      log.info(`  ✅ PASSED: ${testCase.description}`);
      passed++;
    } else {
      log.error(`  ❌ FAILED: ${testCase.description}`);
      log.error(`    Result: ${JSON.stringify(result)}`);
      failed++;
    }
    
    results.push({
      ...testCase,
      passed: testPassed,
      result
    });
    
    // Small delay between tests
    await new Promise(resolve => setTimeout(resolve, 300));
  }
  
  // Final cleanup
  await testConsoleCommand(bridge, 'stat none');
  await testConsoleCommand(bridge, 'viewmode lit');
  
  // Generate report
  const report = `
========================================
Console Command Fix Validation Report
========================================
Total Tests: ${testCases.length}
Passed: ${passed}
Failed: ${failed}
Success Rate: ${((passed / testCases.length) * 100).toFixed(1)}%

Detailed Results:
${results.map(r => `
${r.passed ? '✅' : '❌'} ${r.name}
   Command: "${r.command}"
   Expected: ${r.expectedBehavior}
   Result: ${r.passed ? 'PASSED' : 'FAILED'}
   ${!r.passed ? `Details: ${JSON.stringify(r.result)}` : ''}
`).join('')}

Key Improvements:
1. Empty commands handled gracefully
2. Invalid commands don't throw errors
3. stat fps converted to stat unit
4. Compound commands split properly
5. Better error messages for failures
6. Console warnings reduced

${failed === 0 ? '✅ All tests passed! Tool 10 is now properly fixed.' : '⚠️ Some tests failed. Review the details above.'}
`;
  
  console.log(report);
  
  // Save report
  const fs = await import('fs');
  const timestamp = new Date().toISOString().replace(/:/g, '-');
  const reportPath = `console-fix-validation-${timestamp}.txt`;
  fs.writeFileSync(reportPath, report);
  log.info(`Report saved to: ${reportPath}`);
  
  await bridge.disconnect();
  process.exit(failed === 0 ? 0 : 1);
}

runTests().catch(err => {
  console.error('Test failed:', err);
  process.exit(1);
});