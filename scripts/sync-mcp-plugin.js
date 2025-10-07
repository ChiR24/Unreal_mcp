#!/usr/bin/env node
const fs = require('fs');
const path = require('path');

const SOURCE_RELATIVE = ['..', 'Public', 'McpAutomationBridge'];
const sourceDir = path.resolve(__dirname, ...SOURCE_RELATIVE);

function showHelp() {
  const scriptName = path.basename(process.argv[1]);
  console.log(`Usage: node ${scriptName} [--engine <engine_plugins_dir>] [--project <project_plugins_dir>] [--dry-run] [--clean-engine] [--clean-project]`);
  console.log('Copies the MCP Automation Bridge plugin from this repository into Engine and/or Project plugin directories.');
  console.log('--engine <dir>         Path to the Engine/Plugins directory (the script appends McpAutomationBridge automatically).');
  console.log('--project <dir>        Path to the Project/Plugins directory (the script appends McpAutomationBridge automatically).');
  console.log('--dry-run              Log the operations without modifying the filesystem.');
  console.log('--clean-engine         Remove the destination engine plugin directory before copying.');
  console.log('--clean-project        Remove the destination project plugin directory before copying.');
  console.log('--help                 Show this message.');
}

function parseArgs(argv) {
  const args = {
    engine: null,
    project: null,
    dryRun: false,
    cleanEngine: false,
    cleanProject: false,
    help: false
  };

  for (let i = 2; i < argv.length; i += 1) {
    const token = argv[i];
    switch (token) {
      case '--engine':
        args.engine = argv[++i];
        break;
      case '--project':
        args.project = argv[++i];
        break;
      case '--dry-run':
        args.dryRun = true;
        break;
      case '--clean-engine':
        args.cleanEngine = true;
        break;
      case '--clean-project':
        args.cleanProject = true;
        break;
      case '--help':
      case '-h':
        args.help = true;
        break;
      default:
        console.error(`Unknown argument: ${token}`);
        args.help = true;
        break;
    }
  }

  return args;
}

function ensureSourceExists() {
  if (!fs.existsSync(sourceDir) || !fs.statSync(sourceDir).isDirectory()) {
    throw new Error(`Source directory missing: ${sourceDir}`);
  }
}

function resolveDestination(rootDir, label) {
  if (!rootDir) {
    return null;
  }

  const fullPath = path.resolve(rootDir, 'McpAutomationBridge');
  if (!fs.existsSync(path.dirname(fullPath))) {
    throw new Error(`Destination parent directory does not exist for ${label}: ${path.dirname(fullPath)}`);
  }

  return fullPath;
}

function removeDir(targetPath, dryRun) {
  if (!fs.existsSync(targetPath)) {
    return;
  }

  if (dryRun) {
    console.log(`[dry-run] Would remove directory: ${targetPath}`);
    return;
  }

  fs.rmSync(targetPath, { recursive: true, force: true });
  console.log(`Removed existing directory: ${targetPath}`);
}

function copyDir(targetPath, dryRun) {
  if (dryRun) {
    console.log(`[dry-run] Would copy ${sourceDir} -> ${targetPath}`);
    return;
  }

  fs.cpSync(sourceDir, targetPath, { recursive: true });
  console.log(`Copied ${sourceDir} -> ${targetPath}`);
}

function main() {
  const args = parseArgs(process.argv);

  if (args.help || (!args.engine && !args.project)) {
    showHelp();
    return;
  }

  ensureSourceExists();

  if (args.engine) {
    const engineDest = resolveDestination(args.engine, 'engine');
    if (args.cleanEngine) {
      removeDir(engineDest, args.dryRun);
    }
    copyDir(engineDest, args.dryRun);
  }

  if (args.project) {
    const projectDest = resolveDestination(args.project, 'project');
    if (args.cleanProject) {
      removeDir(projectDest, args.dryRun);
    }
    copyDir(projectDest, args.dryRun);
  }

  console.log('Done. Remember to clear the plugin Binaries/Intermediate folders if requested by Unreal.');
}

try {
  main();
} catch (err) {
  console.error(`Error: ${err.message}`);
  process.exitCode = 1;
}
