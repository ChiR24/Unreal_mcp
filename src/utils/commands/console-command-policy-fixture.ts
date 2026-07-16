export const CONSOLE_COMMAND_POLICY_BUCKETS = [
  'equivalent-block',
  'equivalent-allow',
  'typescript-only',
  'native-only',
  'typescript-over-block',
] as const;

export type ConsoleCommandPolicyBucket =
  (typeof CONSOLE_COMMAND_POLICY_BUCKETS)[number];

export type ConsoleCommandPolicyCase = {
  readonly id: string;
  readonly command: string;
  readonly bucket: ConsoleCommandPolicyBucket;
};

export type ConsoleCommandPolicyExpectedOutcomes = {
  readonly typescriptBlocked: boolean;
  readonly nativeBlocked: boolean;
};

export type ConsoleCommandPolicyTally = {
  readonly totalCases: number;
  readonly typescriptOnly: number;
  readonly nativeOnly: number;
  readonly typescriptOverBlock: number;
  readonly equivalentBlocks: number;
  readonly equivalentAllows: number;
  readonly intendedUnionBlocks: number;
};

export const CONSOLE_COMMAND_POLICY_CASES = [
  { id: 'equivalent-block-01', command: 'quit', bucket: 'equivalent-block' },
  { id: 'equivalent-block-02', command: 'exit', bucket: 'equivalent-block' },
  { id: 'equivalent-block-03', command: 'crash', bucket: 'equivalent-block' },
  { id: 'equivalent-block-04', command: 'kill', bucket: 'equivalent-block' },
  { id: 'equivalent-block-05', command: 'QUIT', bucket: 'equivalent-block' },
  { id: 'equivalent-block-06', command: 'Quit', bucket: 'equivalent-block' },
  { id: 'equivalent-block-07', command: 'py', bucket: 'equivalent-block' },
  { id: 'equivalent-block-08', command: 'python', bucket: 'equivalent-block' },
  { id: 'equivalent-block-09', command: 'python print("x")', bucket: 'equivalent-block' },
  { id: 'equivalent-block-10', command: 'delete', bucket: 'equivalent-block' },
  { id: 'equivalent-block-11', command: 'destroy', bucket: 'equivalent-block' },
  { id: 'equivalent-block-12', command: 'obj garbage', bucket: 'equivalent-block' },
  { id: 'equivalent-block-13', command: 'obj list', bucket: 'equivalent-block' },
  { id: 'equivalent-block-14', command: 'memreport', bucket: 'equivalent-block' },
  { id: 'equivalent-block-15', command: 'viewmode visualizebuffer basecolor', bucket: 'equivalent-block' },
  { id: 'equivalent-block-16', command: 'buildpaths', bucket: 'equivalent-block' },
  { id: 'equivalent-block-17', command: 'rebuildnavigation', bucket: 'equivalent-block' },
  { id: 'equivalent-block-18', command: 'rm', bucket: 'equivalent-block' },
  { id: 'equivalent-block-19', command: 'del', bucket: 'equivalent-block' },
  { id: 'equivalent-block-20', command: 'format', bucket: 'equivalent-block' },
  { id: 'equivalent-block-21', command: 'copy', bucket: 'equivalent-block' },
  { id: 'equivalent-block-22', command: 'move', bucket: 'equivalent-block' },
  { id: 'equivalent-block-23', command: 'start "cmd"', bucket: 'equivalent-block' },
  { id: 'equivalent-block-24', command: 'shutdown', bucket: 'equivalent-block' },
  { id: 'equivalent-block-25', command: 'import os', bucket: 'equivalent-block' },
  { id: 'equivalent-block-26', command: 'import sys', bucket: 'equivalent-block' },
  { id: 'equivalent-block-27', command: 'import subprocess', bucket: 'equivalent-block' },
  { id: 'equivalent-block-28', command: 'os.system', bucket: 'equivalent-block' },
  { id: 'equivalent-block-29', command: 'subprocess.', bucket: 'equivalent-block' },
  { id: 'equivalent-block-30', command: 'exec(', bucket: 'equivalent-block' },
  { id: 'equivalent-block-31', command: 'eval(', bucket: 'equivalent-block' },
  { id: 'equivalent-block-32', command: 'open(', bucket: 'equivalent-block' },
  { id: 'equivalent-block-33', command: 'write(', bucket: 'equivalent-block' },
  { id: 'equivalent-block-34', command: 'read(', bucket: 'equivalent-block' },
  { id: 'equivalent-block-35', command: 'debug crash', bucket: 'equivalent-block' },
  { id: 'equivalent-block-36', command: 'debug break', bucket: 'equivalent-block' },
  { id: 'equivalent-block-37', command: 'assert false', bucket: 'equivalent-block' },
  { id: 'equivalent-block-38', command: 'check(false)', bucket: 'equivalent-block' },
  { id: 'equivalent-block-39', command: 'with open', bucket: 'equivalent-block' },
  { id: 'equivalent-block-40', command: 'a\nb', bucket: 'equivalent-block' },
  { id: 'equivalent-block-41', command: 'a\rb', bucket: 'equivalent-block' },
  { id: 'equivalent-block-42', command: 'a&&b', bucket: 'equivalent-block' },
  { id: 'equivalent-block-43', command: 'a||b', bucket: 'equivalent-block' },
  { id: 'equivalent-block-44', command: 'a;b', bucket: 'equivalent-block' },
  { id: 'equivalent-block-45', command: 'a|b', bucket: 'equivalent-block' },
  { id: 'equivalent-block-46', command: 'a`b', bucket: 'equivalent-block' },
  { id: 'equivalent-allow-01', command: 'stat fps', bucket: 'equivalent-allow' },
  { id: 'equivalent-allow-02', command: 'viewmode lit', bucket: 'equivalent-allow' },
  { id: 'equivalent-allow-03', command: 'help', bucket: 'equivalent-allow' },
  { id: 'equivalent-allow-04', command: 'show', bucket: 'equivalent-allow' },
  { id: 'equivalent-allow-05', command: 'quitter', bucket: 'equivalent-allow' },
  { id: 'equivalent-allow-06', command: 'rmdebug', bucket: 'equivalent-allow' },
  { id: 'typescript-only-01', command: 'reboot', bucket: 'typescript-only' },
  { id: 'typescript-only-02', command: 'rmdir', bucket: 'typescript-only' },
  { id: 'typescript-only-03', command: 'mklink', bucket: 'typescript-only' },
  { id: 'typescript-only-04', command: 'import importlib', bucket: 'typescript-only' },
  { id: 'typescript-only-05', command: 'import shutil', bucket: 'typescript-only' },
  { id: 'typescript-only-06', command: 'from os import x', bucket: 'typescript-only' },
  { id: 'typescript-only-07', command: 'exec (', bucket: 'typescript-only' },
  { id: 'typescript-only-08', command: 'open (', bucket: 'typescript-only' },
  { id: 'typescript-only-09', command: 'write (', bucket: 'typescript-only' },
  { id: 'typescript-only-10', command: 'read (', bucket: 'typescript-only' },
  { id: 'typescript-only-11', command: 'system (', bucket: 'typescript-only' },
  { id: 'typescript-only-12', command: 'import  os', bucket: 'typescript-only' },
  { id: 'typescript-only-13', command: 'import\tos', bucket: 'typescript-only' },
  { id: 'native-only-01', command: 'recompileglobalshaders', bucket: 'native-only' },
  { id: 'native-only-02', command: 'deriveddatacache', bucket: 'native-only' },
  { id: 'native-only-03', command: 'ubt', bucket: 'native-only' },
  { id: 'native-only-04', command: 'unrealbuildtool', bucket: 'native-only' },
  { id: 'native-only-05', command: 'debugbreak', bucket: 'native-only' },
  { id: 'typescript-over-block-01', command: 'foo quit', bucket: 'typescript-over-block' },
  { id: 'typescript-over-block-02', command: 'stat quit', bucket: 'typescript-over-block' },
  { id: 'typescript-over-block-03', command: 'echo rm', bucket: 'typescript-over-block' },
] as const satisfies readonly ConsoleCommandPolicyCase[];

function assertNever(value: never): never {
  throw new Error(`Unhandled console-command policy bucket: ${String(value)}`);
}

export function expectedConsoleCommandPolicyOutcomes(
  bucket: ConsoleCommandPolicyBucket,
): ConsoleCommandPolicyExpectedOutcomes {
  switch (bucket) {
    case 'equivalent-block':
      return { typescriptBlocked: true, nativeBlocked: true };
    case 'equivalent-allow':
      return { typescriptBlocked: false, nativeBlocked: false };
    case 'typescript-only':
    case 'typescript-over-block':
      return { typescriptBlocked: true, nativeBlocked: false };
    case 'native-only':
      return { typescriptBlocked: false, nativeBlocked: true };
    default:
      return assertNever(bucket);
  }
}

export function tallyConsoleCommandPolicyCases(
  cases: readonly ConsoleCommandPolicyCase[] = CONSOLE_COMMAND_POLICY_CASES,
): ConsoleCommandPolicyTally {
  const count = (bucket: ConsoleCommandPolicyBucket): number =>
    cases.filter((testCase) => testCase.bucket === bucket).length;
  const intendedUnionBlocks = cases.filter((testCase) => {
    const expected = expectedConsoleCommandPolicyOutcomes(testCase.bucket);
    return expected.typescriptBlocked || expected.nativeBlocked;
  }).length;
  return {
    totalCases: cases.length,
    typescriptOnly: count('typescript-only'),
    nativeOnly: count('native-only'),
    typescriptOverBlock: count('typescript-over-block'),
    equivalentBlocks: count('equivalent-block'),
    equivalentAllows: count('equivalent-allow'),
    intendedUnionBlocks,
  };
}

export function formatConsoleCommandPolicyReport(): string {
  const tally = tallyConsoleCommandPolicyCases();
  return [
    `TS-only blocks (${tally.typescriptOnly})`,
    `Native-only blocks (${tally.nativeOnly})`,
    `TS over-blocks (${tally.typescriptOverBlock})`,
    `Equivalent blocks (${tally.equivalentBlocks})`,
    `Intended fail-closed union blocks (${tally.intendedUnionBlocks})`,
    `Corpus cases (${tally.totalCases})`,
    `Equivalent allows (${tally.equivalentAllows})`,
  ].join('\n');
}

export function serializeConsoleCommandPolicyFixture(): string {
  const cases = [...CONSOLE_COMMAND_POLICY_CASES].sort((left, right) =>
    left.id.localeCompare(right.id),
  );
  return JSON.stringify({
    schema: 'unreal.console-command-policy-fixture.v1',
    tally: tallyConsoleCommandPolicyCases(cases),
    cases,
  });
}
