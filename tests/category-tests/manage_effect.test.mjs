#!/usr/bin/env node
import path from 'node:path';
import { pathToFileURL } from 'node:url';
import { runToolTests } from '../test-runner.mjs';
import { getToolTests } from './tool-test-utils.mjs';

const toolName = 'manage_effect';
const tests = getToolTests(toolName);

const main = async () => {
  if (tests.length === 0) {
    console.log(`No test cases found for ${toolName}`);
    return;
  }
  await runToolTests(toolName, tests);
};

if (process.argv[1] && import.meta.url === pathToFileURL(path.resolve(process.argv[1])).href) {
  main();
}
