#!/usr/bin/env node
// scripts/mine-invented-names.mjs
// The names a model invented for an unreal call (the gateway refused them UNKNOWN_ACTION, UNKNOWN_CAPABILITY or
// UNKNOWN_TOOL), each paired with the call that then worked, counted over Claude Code session transcripts. A name
// models keep inventing for one capability is an alias that capability lacks: the output is a proposal for a person
// to review, never applied here.
//
// Usage: node scripts/mine-invented-names.mjs <transcript.jsonl | folder>... [--min 2] [--json]
// Folders are read recursively (subagent transcripts included). Transcripts stay on this machine; only names, counts,
// capability ids and model names are printed.

import { createReadStream, readdirSync, readFileSync, statSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { createInterface } from 'node:readline';
import { fileURLToPath } from 'node:url';

const LOOKAHEAD = 6; // describe/execute calls after a refusal searched for the one that worked
// The gateway's own refusals; a handler's "Unknown <tool> action: x" is a stale build, not an invented name.
const REFUSAL = /Error \[UNKNOWN_(ACTION|CAPABILITY|TOOL)\]: Unknown (action|capability|tool) '/u;
// Words too common to show that two action names mean the same thing.
const STOP = new Set(['get', 'set', 'add', 'list', 'create', 'run', 'make', 'edit', 'info', 'new', 'to', 'of', 'by', 'from', 'all']);

const args = process.argv.slice(2);
const minIndex = args.indexOf('--min');
const minCount = minIndex >= 0 ? Number(args[minIndex + 1]) : 2;
const inputs = args.filter((arg, index) => !arg.startsWith('--') && args[index - 1] !== '--min');

function transcripts(path) {
  if (!statSync(path).isDirectory()) return [path];
  return readdirSync(path, { withFileTypes: true }).flatMap((entry) => {
    const full = join(path, entry.name);
    if (entry.isDirectory()) return transcripts(full);
    return entry.name.endsWith('.jsonl') ? [full] : [];
  });
}

// The catalog as generated now: what a name resolves to today (id, alias, legacy tool + action, namespace).
const registryPath = join(dirname(fileURLToPath(import.meta.url)), '../src/tools/catalog/capabilities/generated/canonical-registry.generated.json');
const { records } = JSON.parse(readFileSync(registryPath, 'utf8'));
const byName = new Map();
const aliasOwners = new Map();
const parents = new Set(records.map((record) => record.legacyIds[0]?.tool));
const namespaceParents = new Map();
for (const record of records) {
  byName.set(record.id, record.id);
  for (const pair of record.legacyIds) byName.set(`${pair.tool}.${pair.action}`, record.id);
  for (const alias of record.aliases) aliasOwners.set(alias, [...(aliasOwners.get(alias) ?? []), record.id]);
  const namespace = record.id.split('.')[0];
  if (!parents.has(namespace)) namespaceParents.set(namespace, new Set([...(namespaceParents.get(namespace) ?? []), record.legacyIds[0]?.tool]));
}
function resolve(name) {
  if (byName.has(name)) return byName.get(name);
  const owners = aliasOwners.get(name);
  if (owners?.length === 1) return owners[0];
  const [namespace, ...rest] = name.split('.');
  const parent = namespaceParents.get(namespace);
  return parent?.size === 1 ? byName.get(`${[...parent][0]}.${rest.join('.')}`) : undefined;
}

/** The name a call used: its capability, else tool.action. */
const nameOf = (input) => String(input?.capability ?? (input?.tool && input?.action ? `${input.tool}.${input.action}` : ''));
const actionWords = (name) => new Set(name.split('.').pop().split('_').map((word) => word.replace(/s$/u, '')).filter((word) => word && !STOP.has(word)));
const related = (a, b) => [...actionWords(a)].some((word) => actionWords(b).has(word));

function resultText(block) {
  return typeof block.content === 'string'
    ? block.content
    : (block.content ?? []).map((part) => (typeof part?.text === 'string' ? part.text : '')).join('\n');
}

async function callsOf(file) {
  const calls = [];
  const byId = new Map();
  const lines = createInterface({ input: createReadStream(file), crlfDelay: Infinity });
  for await (const line of lines) {
    if (!line.includes('mcp__unreal') && !line.includes('tool_result')) continue;
    let record;
    try { record = JSON.parse(line); } catch { continue; }
    const content = Array.isArray(record?.message?.content) ? record.message.content : [];
    for (const block of content) {
      const op = block?.input?.operation;
      if (block?.type === 'tool_use' && String(block.name).startsWith('mcp__unreal') && (op === 'describe' || op === 'execute')) {
        const call = { name: nameOf(block.input), model: String(record.message.model ?? 'unknown'), invented: false, ok: false };
        byId.set(block.id, call);
        calls.push(call);
      } else if (block?.type === 'tool_result' && byId.has(block.tool_use_id)) {
        const text = resultText(block);
        const call = byId.get(block.tool_use_id);
        call.invented = REFUSAL.test(text);
        call.ok = !block.is_error && !text.startsWith('Error [');
      }
    }
  }
  return calls.filter((call) => call.name);
}

// A folded family runs the member its old name pins; an alias only pins one whose last word is that member's action.
const folded = new Map(records.flatMap((record) => record.legacyIds.filter((pair) => pair.folded).map((pair) => [`${pair.tool}.${pair.action}`, record.id])));

const files = inputs.flatMap(transcripts);
const invented = new Map(); // name -> { count, runs, models, targets: Map(target -> { count, runs }) }
for (const file of files) {
  const calls = await callsOf(file);
  calls.forEach((call, index) => {
    if (!call.invented) return;
    const entry = invented.get(call.name) ?? { count: 0, runs: new Set(), models: new Set(), targets: new Map() };
    entry.count += 1;
    entry.runs.add(file);
    entry.models.add(call.model);
    const fixed = calls.slice(index + 1, index + 1 + LOOKAHEAD).find((next) => next.ok && next.name !== call.name && related(call.name, next.name));
    // A folded member is named by the call that worked (an alias to its family would run the family's primary).
    const target = !fixed ? '(nothing that worked)' : folded.has(fixed.name) ? `${fixed.name} (folded member)` : resolve(fixed.name) ?? `${fixed.name} (gone)`;
    const seen = entry.targets.get(target) ?? { count: 0, runs: new Set() };
    seen.count += 1;
    seen.runs.add(file);
    entry.targets.set(target, seen);
    invented.set(call.name, entry);
  });
}

const rows = [...invented.entries()].map(([name, entry]) => {
  const outcomes = [...entry.targets.entries()].sort((a, b) => b[1].count - a[1].count);
  const resolved = outcomes.filter(([target]) => !target.startsWith('(') && !target.endsWith('(gone)'));
  const [target, best] = resolved[0] ?? outcomes[0];
  const resolvedCount = resolved.reduce((sum, [, seen]) => sum + seen.count, 0);
  const resolvesNow = resolve(name);
  const agreed = !resolvesNow && !aliasOwners.has(name) && resolved.length > 0 && best.runs.size >= minCount && best.count * 3 >= resolvedCount * 2;
  return {
    invented: name,
    target,
    count: entry.count,
    agreed: best.count,
    agreedRuns: best.runs.size,
    runs: entry.runs.size,
    models: [...entry.models].sort(),
    others: outcomes.filter(([other]) => other !== target).map(([other, seen]) => `${other} x${seen.count}`),
    resolvesNow: resolvesNow ?? null,
    // Two or more runs agree on one capability, and no other capability took a third of the calls that worked. A
    // folded member needs its family's pin, which an alias does not carry.
    propose: agreed && !target.endsWith('(folded member)'),
    needsPin: agreed && target.endsWith('(folded member)'),
  };
}).sort((a, b) => Number(b.propose) - Number(a.propose) || Number(b.needsPin) - Number(a.needsPin) || b.agreed - a.agreed ||
  (a.invented < b.invented ? -1 : a.invented > b.invented ? 1 : 0));

if (args.includes('--json')) {
  console.log(JSON.stringify({ files: files.length, minCount, rows }, null, 2));
} else {
  console.log(`${files.length} transcript(s), ${rows.reduce((sum, row) => sum + row.count, 0)} invented call(s). Proposed: ${minCount}+ runs agreeing on one capability that shares a word with the name (two thirds or more of the calls that worked), the name resolving nothing today.`);
  for (const row of rows) {
    const state = row.propose ? 'PROPOSE' : row.needsPin ? 'FOLDED ' : row.resolvesNow ? 'now ok ' : '       ';
    const now = row.resolvesNow ? ` [resolves now: ${row.resolvesNow}]` : '';
    const others = row.others.length ? ` [also: ${row.others.join(', ')}]` : '';
    console.log(`${state} ${row.invented} -> ${row.target} (${row.agreed} of ${row.count} call(s), ${row.agreedRuns} of ${row.runs} run(s), ${row.models.join(' ')})${now}${others}`);
  }
}
