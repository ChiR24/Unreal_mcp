import { coreToolsTests } from './core-tools.test.mjs';
import { worldToolsTests } from './world-tools.test.mjs';
import { authoringToolsTests } from './authoring-tools.test.mjs';
import { gameplayToolsTests } from './gameplay-tools.test.mjs';
import { utilityToolsTests } from './utility-tools.test.mjs';

export const allCategoryTests = [
  ...coreToolsTests,
  ...worldToolsTests,
  ...authoringToolsTests,
  ...gameplayToolsTests,
  ...utilityToolsTests
];

export function getToolTests(toolName) {
  return allCategoryTests.filter((testCase) => testCase.toolName === toolName);
}
