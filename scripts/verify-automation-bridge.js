#!/usr/bin/env node
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const repoRoot = path.resolve(__dirname, '..');

function ok(msg) { console.log(`[ok] ${msg}`); }
function warn(msg) { console.warn(`[warn] ${msg}`); }
function fail(msg) { console.error(`[fail] ${msg}`); process.exitCode = 1; }

try {
  const pluginDir = path.resolve(repoRoot, 'plugins', 'McpAutomationBridge');
  if (fs.existsSync(pluginDir)) ok(`Plugin folder found: ${pluginDir}`); else fail(`Plugin folder missing: ${pluginDir}`);
  const uplugin = path.join(pluginDir, 'McpAutomationBridge.uplugin');
  if (fs.existsSync(uplugin)) ok('uplugin present'); else warn('uplugin missing');
  const port = process.env.MCP_AUTOMATION_WS_PORT || '8090';
  ok(`Configured port: ${port}`);
  console.log('Verification complete.');
} catch (e) {
  fail(e?.message || String(e));
}

  summary.pythonFlags = requiredPythonSettings.reduce((acc, setting) => {
    acc[setting] = lower.includes(setting);
    return acc;
  }, {});

  if (!summary.hasBridgeEntry) {
    summary.warnings.push('DefaultEngine.ini does not contain a +Plugins entry for McpAutomationBridge. Enable it via Edit ▸ Plugins or add a Plugins section.');
  }

  for (const [key, present] of Object.entries(summary.remoteControl)) {
    if (!present) {
      summary.warnings.push(`Remote Control setting missing: ${key}`);
    }
  }

  for (const [key, present] of Object.entries(summary.pythonFlags)) {
    if (!present) {
      summary.warnings.push(`Python execution setting missing: ${key}`);
    }
  }

  return summary;
}

function checkEnvironment() {
  const envSummary = {
    mcpAutomationBridgeEnabled: process.env.MCP_AUTOMATION_BRIDGE_ENABLED !== 'false',
    host: process.env.MCP_AUTOMATION_WS_HOST || '127.0.0.1',
    port: process.env.MCP_AUTOMATION_WS_PORT || '8090',
    capabilityTokenConfigured: Boolean(process.env.MCP_AUTOMATION_CAPABILITY_TOKEN),
    ueHost: process.env.UE_HOST || '127.0.0.1',
    ueHttpPort: process.env.UE_RC_HTTP_PORT || '30010',
    ueWsPort: process.env.UE_RC_WS_PORT || '30020',
    warnings: []
  };

  if (process.env.MCP_AUTOMATION_BRIDGE_ENABLED === 'false') {
    envSummary.warnings.push('MCP_AUTOMATION_BRIDGE_ENABLED is set to false. The WebSocket listener will not start.');
  }

  if (!process.env.MCP_AUTOMATION_CAPABILITY_TOKEN) {
    envSummary.warnings.push('No MCP_AUTOMATION_CAPABILITY_TOKEN set. Omit for local testing or define one to require handshakes.');
  }

  return envSummary;
}

function printHumanReport(report) {
  const {
    pluginChecks,
    configCheck,
    environment
  } = report;

  const divider = () => console.log('----------------------------------------');

  console.log('Automation Bridge Verification Report');
  divider();

  if (pluginChecks.length === 0) {
    console.log('No plugin directories inspected (pass --engine and/or --project).');
  } else {
    for (const check of pluginChecks) {
      console.log(`${check.label}: ${check.pluginPath}`);
      if (check.errors.length > 0) {
        for (const error of check.errors) {
          console.log(`  [error] ${error}`);
        }
      } else {
        console.log('  status: OK');
        if (check.descriptor) {
          console.log(`  descriptor: v${check.descriptor.versionName || check.descriptor.version || 'unknown'}`);
        }
        if (check.warnings.length > 0) {
          for (const warn of check.warnings) {
            console.log(`  [warning] ${warn}`);
          }
        }
      }
      divider();
    }
  }

  if (configCheck.path) {
    console.log(`DefaultEngine.ini: ${configCheck.path}`);
    if (configCheck.errors.length > 0) {
      for (const error of configCheck.errors) {
        console.log(`  [error] ${error}`);
      }
    } else {
      console.log(`  has MCPAutomationBridge entry: ${configCheck.hasBridgeEntry ? 'yes' : 'no'}`);
      for (const [setting, present] of Object.entries(configCheck.remoteControl)) {
        console.log(`  Remote Control ${setting}: ${present ? 'yes' : 'missing'}`);
      }
      for (const [setting, present] of Object.entries(configCheck.pythonFlags)) {
        console.log(`  Python ${setting}: ${present ? 'yes' : 'missing'}`);
      }
      for (const warn of configCheck.warnings) {
        console.log(`  [warning] ${warn}`);
      }
    }
    divider();
  }

  console.log('Environment variables:');
  console.log(`  MCP_AUTOMATION_BRIDGE_ENABLED=${environment.mcpAutomationBridgeEnabled}`);
  console.log(`  MCP_AUTOMATION_WS_HOST=${environment.host}`);
  console.log(`  MCP_AUTOMATION_WS_PORT=${environment.port}`);
  console.log(`  MCP_AUTOMATION_CAPABILITY_TOKEN=${environment.capabilityTokenConfigured ? '[set]' : '[not set]'}`);
  console.log(`  UE_HOST=${environment.ueHost}`);
  console.log(`  UE_RC_HTTP_PORT=${environment.ueHttpPort}`);
  console.log(`  UE_RC_WS_PORT=${environment.ueWsPort}`);
  for (const warn of environment.warnings) {
    console.log(`  [warning] ${warn}`);
  }
  divider();

  console.log('Next steps:');
  if (pluginChecks.some((check) => check.errors.length > 0)) {
    console.log('- Re-run scripts/sync-mcp-plugin.js with the appropriate --engine/--project paths.');
  }
  if (configCheck.warnings.length > 0 || configCheck.errors.length > 0) {
    console.log('- Update Config/DefaultEngine.ini and enable the plugin in Edit ▸ Plugins.');
  }
  if (environment.warnings.length > 0) {
    console.log('- Review environment variables (set MCP_AUTOMATION_CAPABILITY_TOKEN for secured deployments).');
  }
  if (
    pluginChecks.every((check) => check.errors.length === 0) &&
    configCheck.errors.length === 0 &&
    environment.warnings.length === 0
  ) {
    console.log('- Everything looks good. Launch Unreal and watch the Output Log for the bridge handshake.');
  }
}

function main() {
  const args = parseArgs(process.argv);
  if (args.help) {
    showHelp();
    return;
  }

  const pluginChecks = [];
  const uniqueLabels = new Set();
  if (args.engine) {
    pluginChecks.push(checkPluginInstall(args.engine, 'Engine plugin'));
    uniqueLabels.add('engine');
  }
  if (args.project) {
    pluginChecks.push(checkPluginInstall(args.project, 'Project plugin'));
    uniqueLabels.add('project');
  }

  const configCheck = parseIniForSettings(args.config);
  const environment = checkEnvironment();

  const report = {
    pluginChecks: pluginChecks.filter(Boolean),
    configCheck,
    environment
  };

  if (args.json) {
    console.log(JSON.stringify(report, null, 2));
  } else {
    printHumanReport(report);
  }
}

try {
  main();
} catch (error) {
  console.error(`Verification failed: ${error.message}`);
  process.exitCode = 1;
}
