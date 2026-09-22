export const CONSOLE_COMMAND_POLICY_SURFACES = [
  'typescript',
  'native',
] as const;

export type ConsoleCommandPolicySurface =
  (typeof CONSOLE_COMMAND_POLICY_SURFACES)[number];

export const CONSOLE_COMMAND_POLICY_BLOCK_REASONS = [
  'MALFORMED_INPUT',
  'UNSAFE_SEPARATOR',
  'PYTHON_EXECUTION',
  'DANGEROUS_ENGINE_COMMAND',
  'RESTRICTED_ENGINE_COMMAND',
  'SHELL_COMMAND',
  'UNSAFE_TOKEN',
] as const;

export type ConsoleCommandPolicyBlockReason =
  (typeof CONSOLE_COMMAND_POLICY_BLOCK_REASONS)[number];

export type ConsoleCommandRuleApplicability = ConsoleCommandPolicySurface | 'both';

export type ConsoleCommandRuleMatcher =
  | {
      readonly kind: 'contains-any';
      readonly values: readonly string[];
    }
  | {
      readonly kind: 'first-token';
      readonly values: readonly string[];
    }
  | {
      readonly kind: 'whitespace-bounded-anywhere';
      readonly values: readonly string[];
    }
  | {
      readonly kind: 'pattern';
      readonly source: string;
      readonly flags: 'i';
    };

export type ConsoleCommandPolicyRule = {
  readonly id: string;
  readonly appliesTo: ConsoleCommandRuleApplicability;
  readonly reasonCode: ConsoleCommandPolicyBlockReason;
  readonly matcher: ConsoleCommandRuleMatcher;
};

export const CONSOLE_COMMAND_POLICY_RULES = [
  {
    id: 'shared.unsafe-separator',
    appliesTo: 'both',
    reasonCode: 'UNSAFE_SEPARATOR',
    matcher: {
      kind: 'contains-any',
      values: ['\n', '\r', '&&', '||', ';', '|', '`'],
    },
  },
  {
    id: 'shared.python-first-token',
    appliesTo: 'both',
    reasonCode: 'PYTHON_EXECUTION',
    matcher: { kind: 'first-token', values: ['py', 'python'] },
  },
  {
    // `debug` and `exec` were only ever caught as the literals 'debug crash'
    // and 'debug break'. Unreal's DEBUG command has many more subcommands --
    // assert, fatal, gpf, stackoverflow, eatmem, softlock, recurse,
    // threadedcrash -- and every one of them took the editor down through a
    // plain `write` capability that asks for no consent, losing unsaved work.
    //
    // `exec <file>` is worse than a crash: it runs every line of a text file as
    // a console command, and those lines never reach this policy. Any
    // capability that can write a file was therefore a path to `py` that walks
    // straight past the Python consent gate above.
    //
    // Both are verbs whose entire command space is unsafe, so match the first
    // token rather than chasing subcommand spellings.
    //
    // The rest are NOT debug subcommands, which is why spelling-based blocking
    // kept missing them: UnrealEngine.cpp dispatches CRASH, GPUCRASH, CHECK,
    // GPF, ENSURE, ENSUREALWAYS, FATAL, BUFFEROVERRUN, CRTINVALID, STALL,
    // HITCH and RENDERHITCH as their own top-level commands, and SOFTLOCK and
    // EATMEM likewise, hundreds of lines away from the DEBUG handler. Only
    // 'crash' was caught before; the other thirteen each took the editor down
    // or wedged it. Verified against the UE 5.8 dispatch chain rather than a
    // remembered list -- 'threadedcrash' does not exist in the engine at all.
    //
    // First-token matching is exact, so 'checkpoint' and 'stalls' are
    // unaffected by 'check' and 'stall'.
    id: 'shared.dangerous-first-token',
    appliesTo: 'both',
    reasonCode: 'DANGEROUS_ENGINE_COMMAND',
    matcher: {
      kind: 'first-token',
      values: [
        'debug', 'exec',
        'crash', 'gpucrash', 'check', 'gpf', 'ensure', 'ensurealways',
        'fatal', 'bufferoverrun', 'crtinvalid', 'stall', 'hitch',
        'renderhitch', 'softlock', 'eatmem',
      ],
    },
  },
  {
    id: 'typescript.dangerous-whitespace-bounded',
    appliesTo: 'typescript',
    reasonCode: 'DANGEROUS_ENGINE_COMMAND',
    matcher: {
      kind: 'whitespace-bounded-anywhere',
      values: [
        'quit', 'exit', 'kill', 'crash', 'r.gpucrash', 'r.crash',
        'debug crash', 'forcecrash', 'debug break', 'assert false',
        'check(false)', 'viewmode visualizebuffer basecolor',
        'viewmode visualizebuffer worldnormal', 'buildpaths',
        'rebuildnavigation', 'obj garbage', 'obj list', 'memreport',
        'delete', 'destroy',
      ],
    },
  },
  {
    id: 'typescript.forbidden-substring',
    appliesTo: 'typescript',
    reasonCode: 'UNSAFE_TOKEN',
    matcher: {
      kind: 'contains-any',
      values: [
        'shutdown', 'reboot', 'rmdir', 'mklink', 'import os',
        'import subprocess', 'subprocess.', 'os.system', 'exec(', 'eval(',
        '__import__', 'import sys', 'import importlib', 'with open', 'open(',
        'write(', 'read(',
      ],
    },
  },
  {
    id: 'typescript.shell-name-anywhere',
    appliesTo: 'typescript',
    reasonCode: 'SHELL_COMMAND',
    matcher: { kind: 'pattern', source: String.raw`\b(?:rm|del|format|copy|move|start)\b`, flags: 'i' },
  },
  {
    id: 'typescript.flexible-import',
    appliesTo: 'typescript',
    reasonCode: 'UNSAFE_TOKEN',
    matcher: { kind: 'pattern', source: String.raw`import\s+(?:os|sys|subprocess|importlib|shutil)`, flags: 'i' },
  },
  {
    id: 'typescript.flexible-from-import',
    appliesTo: 'typescript',
    reasonCode: 'UNSAFE_TOKEN',
    matcher: { kind: 'pattern', source: String.raw`from\s+(?:os|sys|subprocess|importlib|shutil)\s+import`, flags: 'i' },
  },
  {
    id: 'typescript.flexible-call',
    appliesTo: 'typescript',
    reasonCode: 'UNSAFE_TOKEN',
    matcher: { kind: 'pattern', source: String.raw`(?:exec|eval|open|write|read|system)\s*\(`, flags: 'i' },
  },
  {
    id: 'typescript.dunder-import',
    appliesTo: 'typescript',
    reasonCode: 'UNSAFE_TOKEN',
    matcher: { kind: 'pattern', source: String.raw`__import__\s*\(`, flags: 'i' },
  },
  {
    id: 'typescript.subprocess-member',
    appliesTo: 'typescript',
    reasonCode: 'UNSAFE_TOKEN',
    matcher: { kind: 'pattern', source: String.raw`subprocess\.`, flags: 'i' },
  },
  {
    id: 'typescript.os-system',
    appliesTo: 'typescript',
    reasonCode: 'UNSAFE_TOKEN',
    matcher: { kind: 'pattern', source: String.raw`os\.system`, flags: 'i' },
  },
  {
    id: 'typescript.start-quoted',
    appliesTo: 'typescript',
    reasonCode: 'SHELL_COMMAND',
    matcher: { kind: 'pattern', source: String.raw`start\s+"`, flags: 'i' },
  },
  {
    id: 'native.blocked-first-token',
    appliesTo: 'native',
    reasonCode: 'DANGEROUS_ENGINE_COMMAND',
    matcher: {
      kind: 'first-token',
      values: [
        'shutdown', 'quit', 'exit', 'kill', 'crash', 'r.gpucrash',
        'r.crash', 'forcecrash', 'debugbreak', 'buildpaths',
        'rebuildnavigation', 'recompileglobalshaders', 'deriveddatacache',
      ],
    },
  },
  {
    id: 'native.restricted-first-token',
    appliesTo: 'native',
    reasonCode: 'RESTRICTED_ENGINE_COMMAND',
    matcher: { kind: 'first-token', values: ['delete', 'destroy', 'unrealbuildtool', 'ubt'] },
  },
  {
    id: 'both.fab-bridge-console',
    appliesTo: 'both',
    reasonCode: 'RESTRICTED_ENGINE_COMMAND',
    matcher: {
      kind: 'first-token',
      values: ['mcp.fab.addtoproject', 'mcp.fab.describecatalogshape'],
    },
  },
  {
    id: 'native.forbidden-name-first-token',
    appliesTo: 'native',
    reasonCode: 'SHELL_COMMAND',
    matcher: { kind: 'first-token', values: ['rm', 'del', 'format', 'copy', 'move', 'start'] },
  },
  {
    id: 'native.forbidden-substring',
    appliesTo: 'native',
    reasonCode: 'UNSAFE_TOKEN',
    matcher: {
      kind: 'contains-any',
      values: [
        'import os', 'import sys', 'import subprocess', 'subprocess.',
        'os.system', 'exec(', 'eval(', '__import__', 'with open', 'open(',
        'write(', 'read(', 'debug crash', 'debug break', 'assert false',
        'check(false)', 'viewmode visualizebuffer basecolor',
        'viewmode visualizebuffer worldnormal', 'obj garbage', 'obj list',
        'memreport',
      ],
    },
  },
] as const satisfies readonly ConsoleCommandPolicyRule[];
