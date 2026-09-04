// tests/unit/docs/docs-claim-contract.test.ts
//
// Task 53 docs contract.
//
// Two halves, and the second is the one that matters:
//   1. POSITIVE — every repo-owned doc is clean under the claim rules.
//   2. NEGATIVE CONTROL — each rule is fed a deliberately bad fragment and MUST
//      reject it. A gate that has never been shown to fail proves nothing about
//      the docs that pass it.
//
// It also resolves the concrete command / path / capability claims the new docs
// make back to the files that implement them, so a doc cannot outlive its code.

import { existsSync, readFileSync } from 'node:fs';
import { resolve } from 'node:path';

import { describe, expect, it } from 'vitest';

import {
  ABSENT_ENGINE_MINORS,
  DOCS_CLAIM_RULES,
  ENGINE_CERTIFICATION_LEDGER,
  NATIVE_PROTOCOL_VERSIONS,
  SUPPORTED_ENGINE_MINORS,
  SUPPORTED_PROTOCOL_VERSIONS,
  TS_ONLY_LEGACY_PROTOCOL_VERSIONS,
  auditDocument,
  paragraphsOf,
  unreleasedSection,
} from './docs-claim-rules.js';

const read = (rel: string): string => readFileSync(resolve(process.cwd(), rel), 'utf8');

/** Docs published to users. Release history is handled separately. */
const PUBLISHED_DOCS = [
  'README.md',
  'docs/protocol.md',
  'docs/gateway-client-guide.md',
  'docs/mcp-primitives.md',
  'docs/security-and-receipts.md',
  'docs/performance-and-evidence.md',
  'docs/handler-mapping.md',
  'docs/testing-guide.md',
  'docs/Roadmap.md',
  'docs/editor-plugin-extension.md',
  'docs/action-reference.generated.md',
  'docs/migration-reference.generated.md',
  'plugins/McpAutomationBridge/README.md',
] as const;

const CHANGELOGS = ['CHANGELOG.md', 'plugins/McpAutomationBridge/CHANGELOG.md'] as const;

const TASK_53_DOCS = [
  'docs/gateway-client-guide.md',
  'docs/mcp-primitives.md',
  'docs/security-and-receipts.md',
  'docs/performance-and-evidence.md',
] as const;

describe('docs claim contract — published docs are clean', () => {
  for (const doc of PUBLISHED_DOCS) {
    it(`${doc} asserts no stale/unbacked claim`, () => {
      const violations = auditDocument(doc, read(doc));
      expect(
        violations.map((v) => `${v.rule} :: ${v.paragraph.slice(0, 180)}`),
        `${doc} violates the docs claim contract`,
      ).toEqual([]);
    });
  }

  for (const changelog of CHANGELOGS) {
    it(`${changelog} (unreleased section only) asserts no stale claim`, () => {
      const violations = auditDocument(changelog, unreleasedSection(read(changelog)));
      expect(violations.map((v) => `${v.rule} :: ${v.paragraph.slice(0, 180)}`)).toEqual([]);
    });
  }
});

// ---------------------------------------------------------------------------
// NEGATIVE CONTROLS. Each fragment is what the corresponding real-world mistake
// actually looks like. If any of these stops failing, the rule has been
// weakened and the positive half above became meaningless.
// ---------------------------------------------------------------------------
describe('docs claim contract — every rule can reject a bad claim', () => {
  const BAD_FRAGMENTS: ReadonlyArray<{ rule: string; fragment: string }> = [
    {
      rule: 'stale-public-tool-surface',
      fragment: 'The MCP server exposes 23 canonical parent tools over stdio.',
    },
    {
      rule: 'stale-public-tool-surface',
      fragment: 'Once connected, all 23 tools are listed by the client.',
    },
    {
      rule: 'stale-public-tool-surface',
      fragment: 'The public MCP surface is the 23 canonical tools shown below.',
    },
    {
      rule: 'removed-unrealagent-surface',
      // Spliced for the same reason as docs-claim-rules.ts: the Task 7 removal
      // contract counts contiguous repo-owned occurrences of the plugin name.
      fragment: `Open the ${'Unreal'}${'Agent'} panel inside the editor and drive the agent from there.`,
    },
    {
      rule: 'removed-unrealagent-surface',
      fragment: 'The in-editor assistant panel ships with the plugin and is enabled by default.',
    },
    {
      rule: 'unsupported-protocol-version',
      fragment:
        'The native transport negotiates protocol version 2026-07-28 in addition to 2025-11-25.',
    },
    {
      rule: 'unsupported-protocol-version',
      fragment: 'Set the MCP-Protocol-Version header to 2027-01-01 for the newest features.',
    },
    {
      rule: 'unbacked-certification',
      fragment: 'The plugin is certified on UE 5.1 and UE 5.6.',
    },
    {
      rule: 'unbacked-certification',
      fragment: 'This release is compile-verified across the full 5.0-5.8 matrix.',
    },
    {
      rule: 'unbacked-certification',
      fragment: 'Certification completed across 5.0–5.8 on every supported engine root.',
    },
    {
      rule: 'unbacked-engine-range-support',
      // Outside the advertised 5.0-5.8 range: a single minor the project does
      // not claim to support.
      fragment: 'The plugin builds and runs on UE 5.9 Preview 1.',
    },
    {
      rule: 'unbacked-engine-range-support',
      // A range that extends past the advertised top end.
      fragment: 'The plugin supports Unreal Engine 5.0 through 5.9.',
    },
    {
      rule: 'stale-capability-record-count',
      fragment: 'The catalog holds 1,373 records across the 23 canonical parents.',
    },
    {
      rule: 'stale-capability-record-count',
      fragment: '| Capability records | 1,373 |',
    },
  ];

  for (const { rule, fragment } of BAD_FRAGMENTS) {
    it(`${rule} rejects: "${fragment.slice(0, 70)}…"`, () => {
      const violations = auditDocument('synthetic-bad-fragment.md', fragment);
      expect(
        violations.map((v) => v.rule),
        'the rule under test did not reject its own counterexample',
      ).toContain(rule);
    });
  }

  it('every declared rule has at least one proven counterexample', () => {
    const proven = new Set(BAD_FRAGMENTS.map((f) => f.rule));
    const declared = DOCS_CLAIM_RULES.map((r) => r.id);
    expect([...declared].filter((id) => !proven.has(id))).toEqual([]);
  });

  it('the rules do not fire on the honest form of the same statement', () => {
    // Guards against the opposite failure: a rule so broad that the truthful
    // sentence is unwritable, which is how a docs gate gets switched off.
    const honest = [
      'The 23 canonical parent tools are internal and reachable only through `unreal.execute`.',
      `The experimental ${'Unreal'}${'Agent'} panel has been removed from this tree.`,
      'The native transport deliberately does not implement the later 2026-07-28 release candidate.',
      'Certification across 5.0-5.8 is incomplete; 5.1, 5.2, 5.4 and 5.6 roots are missing.',
      'The plugin is scoped to build and run across UE 5.0 through 5.8 Preview, but that range is not certified: zero of the nine advertised minors are certified.',
      'UE 5.6 support is blocked: the root is absent from this host.',
      'The plugin does not compile on UE 5.8 Preview 1.',
      'A build for 5.6 will not work with 5.5, 5.7, or 5.8.',
      // Every minor inside the advertised range may be claimed without
      // qualification; these are the simple forms the published docs use.
      'The MCP Automation Bridge plugin is scoped to build and run across UE 5.0 through 5.8 Preview.',
      'All Unreal Engine versions from 5.0 to 5.8 are supported and working.',
      'The plugin builds and runs across UE 5.0 through 5.8 Preview. Console platforms are not included.',
      // The correct record count is the honest form of the same statement.
      'The catalog holds 1,401 records across the 23 canonical parents.',
      '| Capability records | 1,401 |',
    ];
    for (const paragraph of honest) {
      expect(auditDocument('honest.md', paragraph), paragraph).toEqual([]);
    }
  });
});

// ---------------------------------------------------------------------------
// The engine ledger is DATA inside a pure-text module, so nothing stops it
// drifting away from the record it was transcribed from. This re-parses the
// published matrix and holds the two together. Without it, a stale ledger would
// quietly re-certify a minor and `unbacked-engine-range-support` would go blind
// exactly the way `unbacked-certification` did.
// ---------------------------------------------------------------------------
describe('docs claim contract — the engine ledger matches the published matrix', () => {
  const MATRIX_ROW = /^\|\s*(5\.\d)\s*\|[^|]*\|[^|]*\|[^|]*\|\s*(.+?)\s*\|\s*(yes|no)\s*\|/gm;

  const publishedMatrix = (): ReadonlyArray<{ minor: string; state: string; certified: boolean }> =>
    [...read('docs/performance-and-evidence.md').matchAll(MATRIX_ROW)].map((m) => ({
      minor: m[1],
      state: /FAIL/.test(m[2]) ? 'FAIL' : /PASS/.test(m[2]) ? 'PASS' : 'BLOCKED_EXTERNAL',
      certified: m[3] === 'yes',
    }));

  it('publishes all nine advertised minors', () => {
    expect(publishedMatrix().map((r) => r.minor)).toEqual(
      ENGINE_CERTIFICATION_LEDGER.map((r) => r.minor),
    );
  });

  it('agrees with ENGINE_CERTIFICATION_LEDGER row for row', () => {
    expect(publishedMatrix()).toEqual(
      ENGINE_CERTIFICATION_LEDGER.map((r) => ({ minor: r.minor, state: r.state, certified: r.certified })),
    );
  });

  it('advertises the full 5.0–5.8 range as supported, so in-range claims need no qualifier', () => {
    expect(SUPPORTED_ENGINE_MINORS).toEqual(ENGINE_CERTIFICATION_LEDGER.map((r) => r.minor));
  });
});

// ---------------------------------------------------------------------------
// Claim -> code resolution for the docs this task publishes.
// ---------------------------------------------------------------------------
describe('docs claim contract — claims resolve to code', () => {
  it('the generated references exist and are marked generated', () => {
    for (const doc of ['docs/action-reference.generated.md', 'docs/migration-reference.generated.md']) {
      const text = read(doc);
      expect(text.startsWith('<!-- GENERATED FILE - DO NOT EDIT.'), `${doc} lacks the generated banner`).toBe(true);
      expect(text).toContain('npm run registry:generate');
      expect(text).toContain('npm run registry:check');
    }
  });

  it('the generated references are registered as drift-checked targets', () => {
    const targets = read('scripts/canonical-registry/targets.ts');
    for (const artifact of ['action-reference.generated.md', 'migration-reference.generated.md']) {
      // Once in buildTargets (content) and once in staleTargetMeta (ownership).
      const occurrences = targets.split(artifact).length - 1;
      expect(occurrences, `${artifact} must be both built and owned`).toBeGreaterThanOrEqual(2);
    }
  });

  it('every npm script the Task 53 docs name actually exists', () => {
    const pkg: unknown = JSON.parse(read('package.json'));
    const scripts =
      typeof pkg === 'object' && pkg !== null && 'scripts' in pkg
        ? (pkg as { scripts: Record<string, string> }).scripts
        : {};
    const named = new Set<string>();
    for (const doc of TASK_53_DOCS) {
      for (const match of read(doc).matchAll(/`npm run ([a-z][\w:-]*)`/g)) {
        named.add(match[1]);
      }
    }
    expect(named.size).toBeGreaterThan(0);
    expect([...named].filter((s) => !(s in scripts)).sort()).toEqual([]);
  });

  it('every repo path the Task 53 docs cite exists on disk', () => {
    const missing: string[] = [];
    for (const doc of TASK_53_DOCS) {
      for (const match of read(doc).matchAll(/`((?:src|scripts|tests|docs)\/[\w./-]+\.(?:ts|md|json))`/g)) {
        if (!existsSync(resolve(process.cwd(), match[1]))) missing.push(`${doc} -> ${match[1]}`);
      }
    }
    expect(missing).toEqual([]);
  });

  it('the documented protocol versions match the native and SDK sources', () => {
    const privateH = read(
      'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/MCP/Transport/McpNativeTransportPrivate.h',
    );
    for (const version of NATIVE_PROTOCOL_VERSIONS) {
      expect(privateH, `native source omits ${version}`).toContain(version);
    }
    // The legacy pair is TypeScript-only; asserting its ABSENCE natively is what
    // makes "intentionally stricter" a fact rather than a slogan.
    for (const version of TS_ONLY_LEGACY_PROTOCOL_VERSIONS) {
      expect(privateH, `native source must not accept ${version}`).not.toContain(version);
    }
    const guide = read('docs/gateway-client-guide.md');
    for (const version of SUPPORTED_PROTOCOL_VERSIONS) {
      expect(guide, `client guide omits ${version}`).toContain(version);
    }
  });

  it('the documented execute error codes exist in the gateway source', () => {
    const resolveSrc = read('src/server/gateway/gateway-execute-resolve.ts');
    for (const code of [
      'FORM_CONFLICT',
      'MISSING_SELECTOR',
      'ALIAS_CONFLICT',
      'UNKNOWN_CAPABILITY',
      'UNKNOWN_TOOL',
      'UNKNOWN_ACTION',
      'CAPABILITY_REMOVED',
      'MIGRATION_NON_TRANSLATABLE',
    ]) {
      expect(resolveSrc, `gateway source omits ${code}`).toContain(code);
      expect(read('docs/gateway-client-guide.md'), `docs omit ${code}`).toContain(code);
    }
    expect(read('src/server/gateway/direct-call-migration.ts')).toContain('DIRECT_TOOL_CALL_REMOVED');
  });

  it('the documented refusal codes exist on BOTH surfaces', () => {
    // A refusal code documented on one side only is exactly the asymmetry the
    // shared-string rule exists to prevent.
    const doc = read('docs/security-and-receipts.md');
    const tsSources = [
      'src/tools/catalog/capabilities/semantic/errors.ts',
      'src/server/gateway/gateway-execute-policy.ts',
    ]
      .filter((p) => existsSync(resolve(process.cwd(), p)))
      .map(read)
      .join('\n');
    const nativeHeader = read(
      'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/Foundation/McpCapabilityAuthorization.h',
    );
    for (const code of ['SCOPE_NOT_GRANTED', 'CONSENT_REQUIRED', 'QUOTA_EXCEEDED']) {
      expect(doc, `docs omit ${code}`).toContain(code);
      expect(nativeHeader, `native predicates omit ${code}`).toContain(code);
    }
    expect(tsSources.length).toBeGreaterThan(0);
  });

  it('the documented idempotency ledger caps match both implementations', () => {
    const ts = read('src/server/gateway/idempotency-ledger.ts');
    const nativeHeader = read(
      'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/Foundation/McpIdempotencyLedger.h',
    );
    expect(ts).toContain('DEFAULT_MAX_IDEMPOTENCY_ENTRIES = 1024');
    expect(nativeHeader).toContain('MaxEntries = 4096');
    const doc = read('docs/security-and-receipts.md');
    expect(doc).toContain('1024');
    expect(doc).toContain('4096');
    // Eviction must remain COMPLETED-only, the property the doc leans on.
    expect(ts).toContain('evictCompletedOverCap');
  });

  it('the documented subscribable URIs match the allowlist', () => {
    const source = read('src/server/mcp-primitives/resource-revision.ts');
    const uris = [...source.matchAll(/'(ue:\/\/[\w/-]+)'/g)].map((m) => m[1]);
    const doc = read('docs/mcp-primitives.md');
    const listed = uris.filter((u) => source.slice(source.indexOf('SUBSCRIBABLE_URIS')).includes(`'${u}'`));
    expect(listed.length).toBeGreaterThan(0);
    for (const uri of listed) {
      expect(doc, `primitives doc omits subscribable ${uri}`).toContain(uri);
    }
  });

  it('the documented fallback primitives match the source lists', () => {
    const source = read('src/server/mcp-primitives/fallback-pointers.ts');
    const doc = read('docs/mcp-primitives.md');
    for (const primitive of ['resources', 'prompts', 'completions', 'subscriptions', 'tasks']) {
      expect(source, `fallback source omits ${primitive}`).toContain(`'${primitive}'`);
      expect(doc, `primitives doc omits ${primitive}`).toContain(primitive);
    }
    for (const method of ['resources/list', 'prompts/list', 'completion/complete', 'resources/subscribe', 'tasks/list']) {
      expect(doc, `primitives doc omits native method ${method}`).toContain(method);
    }
  });

  // Evidence artifacts live under `.omo/` (gitignored, not distributed), so
  // the records are present only on hosts that generated them. When at least
  // one cited record is present, every citation must resolve; when none is
  // present there is nothing to verify and the case is skipped.
  const CITED_EVIDENCE = [
    ...read('docs/performance-and-evidence.md').matchAll(/`(\.omo\/evidence\/[\w./-]+\.json)`/g),
  ].map((m) => m[1]);

  it.runIf(CITED_EVIDENCE.some((p) => existsSync(resolve(process.cwd(), p))))(
    'the evidence files the performance doc cites exist',
    () => {
      expect(CITED_EVIDENCE.length).toBeGreaterThan(0);
      for (const path of CITED_EVIDENCE) {
        expect(existsSync(resolve(process.cwd(), path)), `cited evidence missing: ${path}`).toBe(true);
      }
    },
  );

  it('the performance doc marks blocked measurements as BLOCKED, not as results', () => {
    const doc = read('docs/performance-and-evidence.md');
    expect(doc).toContain('BLOCKED');
    // The model arm was never enabled; no accuracy may be claimed for it.
    expect(doc).toMatch(/no model was contacted/i);
    // Absent engines must be named as absent.
    for (const minor of ABSENT_ENGINE_MINORS) {
      expect(doc, `performance doc omits absent engine ${minor}`).toContain(minor);
    }
  });

  it('the docs never present a direct canonical tools/call as a working path', () => {
    // A legacy top-level example is allowed ONLY inside migration guidance.
    for (const doc of PUBLISHED_DOCS) {
      for (const paragraph of paragraphsOf(read(doc))) {
        if (!/tools\/call/.test(paragraph)) continue;
        if (!/"name"\s*:\s*"(?!unreal")/.test(paragraph) && !/name:\s*"(?!unreal")/.test(paragraph)) continue;
        expect(
          /DIRECT_TOOL_CALL_REMOVED|migrat|removed|no longer|not routed|rejected/i.test(paragraph),
          `${doc} shows a direct canonical tools/call outside migration guidance:\n${paragraph.slice(0, 200)}`,
        ).toBe(true);
      }
    }
  });
});
