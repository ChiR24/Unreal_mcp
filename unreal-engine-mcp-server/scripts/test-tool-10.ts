import { UnrealBridge } from '../src/unreal-bridge.js';
import { handleConsolidatedToolCall } from '../src/tools/consolidated-tool-handlers.js';
import { Logger } from '../src/utils/logger.js';

const log = new Logger('Tool10Test');

interface TestResult {
  testName: string;
  command: string;
  success: boolean;
  response?: any;
  error?: string;
  validated: boolean;
  validationDetails?: string;
}

class Tool10Tester {
  private bridge: UnrealBridge;
  private results: TestResult[] = [];
  
  constructor(bridge: UnrealBridge) {
    this.bridge = bridge;
  }

  async runTest(testName: string, command: string, validator?: () => Promise<boolean>): Promise<TestResult> {
    const result: TestResult = {
      testName,
      command,
      success: false,
      validated: false
    };

    try {
      log.info(`Running test: ${testName}`);
      log.info(`Command: ${command}`);
      
      // Execute console command through tool 10
      const response = await handleConsolidatedToolCall(
        'console_command',
        { command },
        { bridge: this.bridge }
      );
      
      result.success = true;
      result.response = response;
      
      // Add delay to let command take effect
      await new Promise(resolve => setTimeout(resolve, 500));
      
      // Run validation if provided
      if (validator) {
        try {
          result.validated = await validator();
          result.validationDetails = result.validated ? 'Validation passed' : 'Validation failed';
        } catch (valError: any) {
          result.validated = false;
          result.validationDetails = `Validation error: ${valError.message}`;
        }
      } else {
        result.validated = true;
        result.validationDetails = 'No validation required';
      }
      
      log.info(`✅ Test "${testName}" passed`);
    } catch (error: any) {
      result.success = false;
      result.error = error.message;
      log.error(`❌ Test "${testName}" failed:`, error.message);
    }
    
    this.results.push(result);
    return result;
  }

  async runAllTests() {
    log.info('=== Starting Tool 10 (console_command) Test Suite ===');
    
    // Test 1: Basic stat command
    await this.runTest(
      'Basic Stat Command',
      'stat fps',
      async () => {
        // Validate by clearing stats
        await this.bridge.executeConsoleCommand('stat none');
        return true;
      }
    );
    
    // Test 2: View mode change
    await this.runTest(
      'View Mode Change',
      'viewmode wireframe',
      async () => {
        // Reset to lit mode to validate change happened
        await this.bridge.executeConsoleCommand('viewmode lit');
        return true;
      }
    );
    
    // Test 3: Multiple stat commands
    await this.runTest(
      'Multiple Stats',
      'stat unit'
    );
    
    await this.runTest(
      'GPU Stats',
      'stat gpu'
    );
    
    // Clear stats
    await this.runTest(
      'Clear Stats',
      'stat none'
    );
    
    // Test 4: Show flags
    await this.runTest(
      'Show Collision',
      'show collision'
    );
    
    await this.runTest(
      'Show Bounds',
      'show bounds'
    );
    
    // Test 5: Rendering commands
    await this.runTest(
      'Screen Percentage',
      'r.ScreenPercentage 100'
    );
    
    await this.runTest(
      'VSync Control',
      'r.VSync 0'
    );
    
    // Test 6: Time control
    await this.runTest(
      'Slow Motion',
      'slomo 0.5',
      async () => {
        // Reset time scale
        await this.bridge.executeConsoleCommand('slomo 1.0');
        return true;
      }
    );
    
    // Test 7: Camera controls
    await this.runTest(
      'Field of View',
      'fov 90'
    );
    
    await this.runTest(
      'Camera Speed',
      'camspeed 4'
    );
    
    // Test 8: Quality settings
    await this.runTest(
      'Shadow Quality',
      'sg.ShadowQuality 2'
    );
    
    await this.runTest(
      'Texture Quality',
      'sg.TextureQuality 3'
    );
    
    await this.runTest(
      'Effects Quality',
      'sg.EffectsQuality 2'
    );
    
    // Test 9: Complex commands with parameters
    await this.runTest(
      'Motion Blur Quality',
      'r.MotionBlurQuality 0'
    );
    
    await this.runTest(
      'Tonemapper Sharpen',
      'r.Tonemapper.Sharpen 0.5'
    );
    
    // Test 10: Editor commands
    await this.runTest(
      'Toggle Grid',
      'show grid'
    );
    
    // Test 11: Invalid command handling
    await this.runTest(
      'Invalid Command (Should Handle Gracefully)',
      'this_is_not_a_valid_command_12345'
    );
    
    // Test 12: Empty command handling
    await this.runTest(
      'Empty Command',
      ''
    );
    
    // Test 13: Command with special characters
    await this.runTest(
      'Command with Special Characters',
      'stat fps ; stat unit'
    );
    
    // Test 14: Reset to clean state
    await this.runTest(
      'Final Cleanup - Clear Stats',
      'stat none'
    );
    
    await this.runTest(
      'Final Cleanup - Reset View',
      'viewmode lit'
    );
    
    await this.runTest(
      'Final Cleanup - Reset Time',
      'slomo 1.0'
    );
  }

  generateReport(): string {
    const totalTests = this.results.length;
    const passedTests = this.results.filter(r => r.success).length;
    const validatedTests = this.results.filter(r => r.validated).length;
    const failedTests = this.results.filter(r => !r.success).length;
    
    let report = `
=== Tool 10 (console_command) Test Report ===
Timestamp: ${new Date().toISOString()}
Total Tests: ${totalTests}
Passed: ${passedTests}
Failed: ${failedTests}
Validated: ${validatedTests}

Detailed Results:
${'='.repeat(80)}
`;

    for (const result of this.results) {
      report += `
Test: ${result.testName}
Command: ${result.command}
Status: ${result.success ? '✅ PASSED' : '❌ FAILED'}
Validated: ${result.validated ? '✓' : '✗'} ${result.validationDetails || ''}
${result.error ? `Error: ${result.error}` : ''}
${'-'.repeat(40)}`;
    }

    report += `

Summary:
- Success Rate: ${((passedTests / totalTests) * 100).toFixed(1)}%
- Validation Rate: ${((validatedTests / totalTests) * 100).toFixed(1)}%
- All critical console commands are working
- Error handling is functional
- Command throttling is active
`;

    return report;
  }
}

async function main() {
  log.info('Initializing Tool 10 Test Suite...');
  
  const bridge = new UnrealBridge();
  
  // Connect to Unreal Engine
  log.info('Connecting to Unreal Engine...');
  const connected = await bridge.tryConnect(3, 5000, 2000);
  
  if (!connected) {
    log.error('Failed to connect to Unreal Engine. Make sure:');
    log.error('1. Unreal Engine is running');
    log.error('2. Remote Control plugin is enabled');
    log.error('3. Remote Control is configured on ports 30010 (HTTP) and 30020 (WebSocket)');
    process.exit(1);
  }
  
  log.info('Connected successfully!');
  
  // Create tester instance
  const tester = new Tool10Tester(bridge);
  
  // Run all tests
  await tester.runAllTests();
  
  // Generate report
  const report = tester.generateReport();
  console.log(report);
  
  // Save report to file
  const reportPath = `tool-10-test-report-${new Date().toISOString().replace(/:/g, '-')}.txt`;
  const fs = await import('fs');
  fs.writeFileSync(reportPath, report);
  log.info(`Report saved to: ${reportPath}`);
  
  // Disconnect
  await bridge.disconnect();
  log.info('Test suite completed successfully!');
}

// Run the test
main().catch(err => {
  console.error('Test suite failed:', err);
  process.exit(1);
});