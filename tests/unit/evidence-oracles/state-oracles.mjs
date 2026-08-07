// @ts-check
// tests/unit/evidence-oracles/state-oracles.mjs
// Task 50 — READ-ONLY, INDEPENDENT observations of Unreal state.
//
// WHY THIS FILE EXISTS. Task 49's live corpus proved a mutation by asking the
// SAME gateway, over the SAME transport, through the SAME subsystem queue,
// whether the mutation had happened. That oracle shares every failure mode with
// the thing it verifies, and it failed exactly that way three times in one run:
//
//   - the harness reported `cleanupClean: true` while two materials sat on disk.
//     The delete had answered INVALID_ARGUMENT; the response was believed. The
//     leak was caught by `find` — a mechanism the harness did not own.
//   - one transport's oracle read the OTHER transport's leftover asset and scored
//     a broken call as proven, because there was no pre-state.
//   - the SSE tests compared the parser to itself and passed against a mutation.
//
// So the rule this module enforces mechanically: AN ORACLE MAY NOT REACH THE
// SUBJECT THROUGH THE SUBJECT. Every function here reads raw bytes from the
// filesystem, /proc, or a socket the driver under test does not own. None of them
// imports a driver, a gateway module, a capability record, or a response parser.
// If the plugin, the gateway and the transport were all simultaneously lying,
// every observation below would still be true.
//
// EVERY observation carries three fields a judgement cannot ignore:
//   mechanism    — the literal thing that was read ("fs:uasset-package")
//   independence — how far it is from the subject (see INDEPENDENCE)
//   conclusive   — whether the read actually produced a reading. An oracle that
//                  could not look is NOT a negative reading. Task 49's absence
//                  assumption is exactly the bug that distinction prevents.

import { createHash } from 'node:crypto';
import { existsSync, readFileSync, readdirSync, readlinkSync } from 'node:fs';
import { request as httpRequest } from 'node:http';
import { join, posix, relative, resolve, sep } from 'node:path';

/**
 * How far an observation is from the thing it observes. Only OUT_OF_BAND is
 * unconditionally trustworthy: it shares no process, no protocol and no code
 * with the subject. The other two are useful corroboration and are ALLOWED, but
 * `requireIndependentProof` in oracle-judgement.mjs will not let them stand alone
 * for a mutation claim.
 */
export const INDEPENDENCE = Object.freeze({
  /** Raw bytes. No MCP server, no plugin, no gateway involved at any point. */
  OUT_OF_BAND: 'out-of-band',
  /** A different transport than the mutation used. Shares the plugin + queue. */
  CROSS_TRANSPORT: 'cross-transport',
  /** A different capability on the SAME transport. Shares almost everything. */
  CROSS_CAPABILITY: 'cross-capability',
});

/** Ordered weakest-first, so a suite can assert it holds at least one strong reading.
 * @type {Readonly<Record<string, number>>} */
export const INDEPENDENCE_RANK = Object.freeze({
  [INDEPENDENCE.CROSS_CAPABILITY]: 0,
  [INDEPENDENCE.CROSS_TRANSPORT]: 1,
  [INDEPENDENCE.OUT_OF_BAND]: 2,
});

/**
 * @typedef {{
 *   kind: string,
 *   mechanism: string,
 *   independence: string,
 *   target: string,
 *   present: boolean|null,
 *   digest: string|null,
 *   conclusive: boolean,
 *   detail: Record<string, unknown>,
 *   observedAt: string,
 * }} Observation
 */

/**
 * Build an observation. `conclusive` defaults to "we got a boolean answer";
 * a caller that could not look passes `present: null` and gets an inconclusive
 * record it cannot accidentally read as "absent".
 * @param {{ kind: string, mechanism: string, independence?: string, target: string,
 *   present: boolean|null, digest?: string|null, conclusive?: boolean,
 *   detail?: Record<string, unknown>, now?: () => Date }} spec
 * @returns {Observation}
 */
export function observation(spec) {
  const now = spec.now ?? (() => new Date());
  return {
    kind: spec.kind,
    mechanism: spec.mechanism,
    independence: spec.independence ?? INDEPENDENCE.OUT_OF_BAND,
    target: spec.target,
    present: spec.present,
    digest: spec.digest ?? null,
    conclusive: spec.conclusive ?? spec.present !== null,
    detail: spec.detail ?? {},
    observedAt: now().toISOString(),
  };
}

/** @param {Buffer|string} data */
export function sha256(data) {
  return createHash('sha256').update(data).digest('hex');
}

// ─────────────────────────────── asset packages ──────────────────────────────

/** Little-endian 0x9E2A83C1 — the magic every UE package file starts with. */
export const UE_PACKAGE_MAGIC = Buffer.from([0xc1, 0x83, 0x2a, 0x9e]);

/** Extensions a `/Game/...` object may exist as on disk. `.umap` first is wrong; assets are commoner. */
const PACKAGE_EXTENSIONS = Object.freeze(['.uasset', '.umap']);

/**
 * Map an Unreal object path to the file that would hold it. This is the only
 * place the /Game -> Content convention is encoded, and it is a pure string
 * transform: it never asks the editor where anything lives, because "ask the
 * subject where it put the thing" is the dependence this file exists to avoid.
 * @param {string} projectRoot absolute path of the .uproject's directory
 * @param {string} objectPath e.g. `/Game/MCPTest/run/M_Thing` or `.../M_Thing.M_Thing`
 * @returns {string|null} absolute path WITHOUT extension, or null if not under /Game
 */
export function packageBasePath(projectRoot, objectPath) {
  const trimmed = String(objectPath).trim();
  // `/Game/A/B.B` is the object form; the package is `/Game/A/B`.
  const withoutObject = trimmed.includes('.') ? trimmed.slice(0, trimmed.lastIndexOf('.')) : trimmed;
  const normalized = withoutObject.replace(/\/+$/u, '');
  if (!normalized.startsWith('/Game/') && normalized !== '/Game') return null;
  const relativePart = normalized === '/Game' ? '' : normalized.slice('/Game/'.length);
  return join(projectRoot, 'Content', ...relativePart.split('/').filter((part) => part.length > 0));
}

/**
 * Locate an FName entry inside a package's name table.
 *
 * A raw substring scan would report `M_Foo` present inside `M_FooBar`, and would
 * match a path fragment in some unrelated import. UE serializes each name as an
 * int32 length (including the terminator) followed by the bytes and a NUL, so we
 * demand that exact framing. Strong enough to be evidence; still just bytes.
 * @param {Buffer} buffer @param {string} name
 * @returns {number} byte offset of the entry, or -1
 */
export function findNameEntry(buffer, name) {
  const needle = Buffer.from(`${name}\0`, 'latin1');
  const declaredLength = needle.length;
  let from = 0;
  for (;;) {
    const at = buffer.indexOf(needle, from);
    if (at < 4) {
      if (at < 0) return -1;
      from = at + 1;
      continue;
    }
    if (buffer.readInt32LE(at - 4) === declaredLength) return at;
    from = at + 1;
  }
}

/**
 * Observe an Unreal asset by reading its package file directly.
 *
 * PRESENT means: the file exists, starts with the UE package magic, and (when a
 * name is expected) carries that name in its name table. A zero-byte or truncated
 * file is reported ABSENT-with-detail rather than present, because "the create
 * succeeded and produced a corrupt package" is a lie this oracle should catch,
 * not launder.
 * @param {{ projectRoot: string, objectPath: string, expectName?: string|null, now?: () => Date }} spec
 * @returns {Observation}
 */
export function observeAssetPackage(spec) {
  const base = packageBasePath(spec.projectRoot, spec.objectPath);
  if (base === null) {
    return observation({
      kind: 'asset', mechanism: 'fs:uasset-package', target: spec.objectPath,
      present: null, conclusive: false, now: spec.now,
      detail: { reason: 'NOT_UNDER_GAME', note: 'only /Game/... objects have a predictable on-disk package' },
    });
  }
  const found = PACKAGE_EXTENSIONS.map((extension) => `${base}${extension}`).find((file) => existsSync(file));
  if (found === undefined) {
    return observation({
      kind: 'asset', mechanism: 'fs:uasset-package', target: spec.objectPath,
      present: false, now: spec.now,
      detail: { searched: PACKAGE_EXTENSIONS.map((extension) => `${base}${extension}`) },
    });
  }
  const bytes = readFileSync(found);
  const magicOk = bytes.length >= 4 && bytes.subarray(0, 4).equals(UE_PACKAGE_MAGIC);
  const expectName = spec.expectName ?? null;
  const nameOffset = expectName === null ? null : findNameEntry(bytes, expectName);
  const nameOk = expectName === null ? true : nameOffset !== -1;
  return observation({
    kind: 'asset', mechanism: 'fs:uasset-package', target: spec.objectPath,
    present: magicOk && nameOk,
    digest: sha256(bytes),
    now: spec.now,
    detail: {
      file: found,
      byteLength: bytes.length,
      magicOk,
      expectName,
      nameOffset,
      // A file that exists but is not a package is the interesting case: say so
      // rather than letting `present:false` read as "nothing was created".
      note: magicOk ? undefined : 'file exists but does not carry the UE package magic',
    },
  });
}

/**
 * Observe an actor inside a SAVED level package, or inside its One-File-Per-Actor
 * directory when the level is partitioned.
 *
 * HONEST LIMIT, stated in the record itself: this proves what is on DISK. An actor
 * spawned into the in-memory world is invisible here until the level is saved, so
 * a caller must save before asserting presence, and `detail.provesDiskOnly` says
 * so in the evidence rather than in a comment nobody reads.
 * @param {{ projectRoot: string, levelPath: string, actorName: string,
 *   externalActorsRoot?: string|null, now?: () => Date }} spec
 * @returns {Observation}
 */
export function observeLevelActor(spec) {
  const base = packageBasePath(spec.projectRoot, spec.levelPath);
  if (base === null) {
    return observation({
      kind: 'actor', mechanism: 'fs:umap-package', target: `${spec.levelPath}::${spec.actorName}`,
      present: null, conclusive: false, now: spec.now, detail: { reason: 'NOT_UNDER_GAME' },
    });
  }
  const umap = `${base}.umap`;
  const embedded = existsSync(umap) ? findNameEntry(readFileSync(umap), spec.actorName) !== -1 : false;
  // World Partition / OFPA stores each actor as its own package under
  // Content/__ExternalActors__/<LevelPath>/... — a directory scan, not a name table.
  let external = false;
  /** @type {string|null} */
  let externalHit = null;
  const externalRoot = spec.externalActorsRoot ?? null;
  if (externalRoot !== null && existsSync(externalRoot)) {
    for (const file of walkFiles(externalRoot)) {
      if (findNameEntry(readFileSync(file), spec.actorName) !== -1) {
        external = true;
        externalHit = file;
        break;
      }
    }
  }
  return observation({
    kind: 'actor', mechanism: 'fs:umap-package', target: `${spec.levelPath}::${spec.actorName}`,
    present: embedded || external,
    now: spec.now,
    detail: {
      levelFile: umap,
      levelExists: existsSync(umap),
      embedded,
      external,
      externalHit,
      provesDiskOnly: 'presence in the SAVED package; an unsaved in-memory actor is invisible to this mechanism',
    },
  });
}

// ──────────────────────────────── file trees ─────────────────────────────────

const SKIP_TREE_DIRECTORIES = new Set(['.git', 'node_modules']);

/** @param {string} root @returns {string[]} absolute file paths, recursive */
export function walkFiles(root) {
  /** @type {string[]} */
  const files = [];
  if (!existsSync(root)) return files;
  /** @param {string} directory */
  const visit = (directory) => {
    for (const entry of readdirSync(directory, { withFileTypes: true })) {
      if (entry.isDirectory()) {
        if (!SKIP_TREE_DIRECTORIES.has(entry.name)) visit(join(directory, entry.name));
        continue;
      }
      if (entry.isFile()) files.push(join(directory, entry.name));
    }
  };
  visit(root);
  return files.sort();
}

/**
 * Observe a directory as a single content-addressed digest.
 *
 * This is the CHANGED detector. Presence alone cannot tell "the action modified
 * the asset" from "the action did nothing and the asset was already there"; a
 * digest over (relative path, file hash) pairs can, and it is also how residue is
 * proven: the post-cleanup digest must equal the pre-run digest exactly.
 * @param {{ root: string, kind?: string, now?: () => Date }} spec
 * @returns {Observation}
 */
export function observeTree(spec) {
  const root = resolve(spec.root);
  if (!existsSync(root)) {
    return observation({
      kind: spec.kind ?? 'tree', mechanism: 'fs:tree-digest', target: root,
      present: false, digest: sha256(''), now: spec.now,
      detail: { fileCount: 0, note: 'root does not exist; the empty digest is its stable identity' },
    });
  }
  const files = walkFiles(root);
  const lines = files.map((file) => {
    const rel = relative(root, file).split(sep).join('/');
    return `${sha256(readFileSync(file))}  ${rel}`;
  });
  return observation({
    kind: spec.kind ?? 'tree', mechanism: 'fs:tree-digest', target: root,
    present: files.length > 0,
    digest: sha256(lines.join('\n')),
    now: spec.now,
    detail: {
      fileCount: files.length,
      // Bounded: a run that dropped 10k files must not turn the evidence file
      // into the thing that breaks the report.
      files: files.slice(0, 64).map((file) => relative(root, file).split(sep).join('/')),
      truncated: files.length > 64,
    },
  });
}

// ─────────────────────────────── render outputs ──────────────────────────────

/**
 * Decode just enough of an image header to know it is a real, non-degenerate
 * render. Reading the bytes rather than trusting the render receipt is the point:
 * a job that reports `success` and writes a 0x0 or truncated file is precisely
 * the forged success this task was created to catch.
 * @param {Buffer} bytes
 * @returns {{ format: string, width: number|null, height: number|null, bitDepth: number|null }}
 */
export function decodeImageHeader(bytes) {
  if (bytes.length >= 24 && bytes.subarray(0, 8).equals(Buffer.from([0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a]))
    && bytes.subarray(12, 16).toString('latin1') === 'IHDR') {
    return { format: 'png', width: bytes.readUInt32BE(16), height: bytes.readUInt32BE(20), bitDepth: bytes[24] ?? null };
  }
  if (bytes.length >= 4 && bytes.readUInt32LE(0) === 0x01312f76) {
    // OpenEXR. The data window lives in the attribute table; we report the format
    // and let byteLength carry the "is it degenerate" signal rather than
    // half-implementing an EXR parser and calling the result a measurement.
    return { format: 'exr', width: null, height: null, bitDepth: null };
  }
  if (bytes.length >= 2 && bytes[0] === 0xff && bytes[1] === 0xd8) {
    // `at + 8` is the last byte a SOF frame header touches (width's low byte).
    // An earlier `at + 9` guard here silently skipped a SOF segment that ended
    // exactly at the buffer's end, which is how a truncated-but-parseable render
    // would have been reported dimensionless instead of measured.
    for (let at = 2; at + 8 < bytes.length;) {
      if (bytes[at] !== 0xff) { at += 1; continue; }
      const marker = bytes[at + 1];
      if (marker >= 0xc0 && marker <= 0xcf && marker !== 0xc4 && marker !== 0xc8 && marker !== 0xcc) {
        return { format: 'jpeg', height: bytes.readUInt16BE(at + 5), width: bytes.readUInt16BE(at + 7), bitDepth: bytes[at + 4] ?? null };
      }
      at += 2 + bytes.readUInt16BE(at + 2);
    }
    return { format: 'jpeg', width: null, height: null, bitDepth: null };
  }
  if (bytes.length >= 26 && bytes[0] === 0x42 && bytes[1] === 0x4d) {
    return { format: 'bmp', width: bytes.readInt32LE(18), height: bytes.readInt32LE(22), bitDepth: bytes.readUInt16LE(28) };
  }
  return { format: 'unknown', width: null, height: null, bitDepth: null };
}

/**
 * Observe a rendered frame or captured screenshot as bytes.
 *
 * PRESENT requires a decodable header AND a non-zero pixel area where the format
 * exposes one. "The file is there" is not the claim a render makes.
 * @param {{ file: string, minBytes?: number, now?: () => Date }} spec
 * @returns {Observation}
 */
export function observeRenderOutput(spec) {
  const file = resolve(spec.file);
  if (!existsSync(file)) {
    return observation({
      kind: 'render', mechanism: 'fs:image-header', target: file,
      present: false, now: spec.now, detail: { reason: 'FILE_ABSENT' },
    });
  }
  const bytes = readFileSync(file);
  const header = decodeImageHeader(bytes);
  const minBytes = spec.minBytes ?? 1;
  const areaOk = header.width === null || header.height === null
    ? true
    : header.width > 0 && header.height > 0;
  const present = header.format !== 'unknown' && bytes.length >= minBytes && areaOk;
  return observation({
    kind: 'render', mechanism: 'fs:image-header', target: file,
    present,
    digest: sha256(bytes),
    now: spec.now,
    detail: { ...header, byteLength: bytes.length, minBytes, areaOk },
  });
}

// ───────────────────────────────── settings ──────────────────────────────────

/**
 * Read one key out of an Unreal `.ini` directly. Project settings are files;
 * asking the plugin what its own setting is would let one bug answer for two.
 * @param {{ file: string, section: string, key: string, now?: () => Date }} spec
 * @returns {Observation}
 */
export function observeIniSetting(spec) {
  const file = resolve(spec.file);
  const target = `${file}::[${spec.section}]${spec.key}`;
  if (!existsSync(file)) {
    return observation({
      kind: 'settings', mechanism: 'fs:ini', target,
      present: false, now: spec.now, detail: { reason: 'FILE_ABSENT' },
    });
  }
  const text = readFileSync(file, 'utf8');
  /** @type {string|null} */
  let value = null;
  let inSection = false;
  for (const rawLine of text.split(/\r?\n/u)) {
    const line = rawLine.trim();
    if (line.startsWith('[') && line.endsWith(']')) {
      inSection = line.slice(1, -1) === spec.section;
      continue;
    }
    if (!inSection || line.length === 0 || line.startsWith(';')) continue;
    const equals = line.indexOf('=');
    if (equals < 0) continue;
    // UE ini directives prefix the key with +, -, . or !; strip so the caller
    // asks for the key it sees in Project Settings.
    const key = line.slice(0, equals).replace(/^[+\-.!]/u, '').trim();
    if (key === spec.key) value = line.slice(equals + 1).trim();
  }
  return observation({
    kind: 'settings', mechanism: 'fs:ini', target,
    present: value !== null,
    digest: value === null ? null : sha256(value),
    now: spec.now,
    detail: { file, section: spec.section, key: spec.key, value },
  });
}

// ─────────────────────────── processes, ports, sessions ──────────────────────

/**
 * Observe a process through /proc.
 *
 * `startTicks` is the field that makes a STALE PID detectable. A pid alone proves
 * nothing an hour later — the kernel recycles them — so evidence that names a pid
 * without its start time cannot be re-checked, and the evidence validator refuses
 * exactly that shape.
 * @param {{ pid: number, procRoot?: string, now?: () => Date }} spec
 * @returns {Observation}
 */
export function observeProcess(spec) {
  const procRoot = spec.procRoot ?? '/proc';
  const dir = join(procRoot, String(spec.pid));
  if (!existsSync(dir)) {
    // Windows has no /proc, so the directory probe can never see a live pid
    // there. This kill-0 fallback probes the REAL host only when the caller
    // did not inject a fake procRoot: an injected fake /proc is authoritative
    // on every platform, so tests that simulate "no such pid" stay
    // deterministic instead of depending on which host pids are in use. signal
    // 0 is the portable existence probe: it throws ESRCH when no such process
    // exists, and succeeds (or throws EPERM for a process this caller may not
    // signal, which still means it exists) otherwise. pid 0 is never a
    // userspace process on any platform, and kill(0, 0) succeeds on win32, so
    // it is special-cased to stay absent.
    if (process.platform === 'win32' && spec.procRoot === undefined) {
      let present = false;
      if (spec.pid !== 0) {
        try {
          process.kill(spec.pid, 0);
          present = true;
        } catch (error) {
          present = (/** @type {NodeJS.ErrnoException} */ (error))?.code === 'EPERM';
        }
      }
      return observation({
        kind: 'process', mechanism: 'win32:kill-0', target: `pid:${spec.pid}`,
        present, now: spec.now,
        detail: {
          pid: spec.pid,
          startTicks: null,
          note: 'no /proc on win32; existence probed via kill(pid, 0) and startTicks is unavailable',
        },
      });
    }
    return observation({
      kind: 'process', mechanism: 'procfs:stat', target: `pid:${spec.pid}`,
      present: false, now: spec.now, detail: { reason: 'NO_SUCH_PID' },
    });
  }
  /** @type {Record<string, unknown>} */
  const detail = { pid: spec.pid };
  try {
    const stat = readFileSync(join(dir, 'stat'), 'utf8');
    // comm can contain spaces and parens; everything after the LAST ')' is fixed-width.
    const fields = stat.slice(stat.lastIndexOf(')') + 2).split(' ');
    detail.comm = stat.slice(stat.indexOf('(') + 1, stat.lastIndexOf(')'));
    detail.state = fields[0];
    detail.startTicks = Number(fields[19]);
  } catch {
    detail.statUnreadable = true;
  }
  try {
    detail.cmdline = readFileSync(join(dir, 'cmdline'), 'utf8').split('\0').filter((part) => part.length > 0);
  } catch {
    detail.cmdline = null;
  }
  try {
    detail.exe = readlinkSync(join(dir, 'exe'));
  } catch {
    // A process we do not own, or one that exited between the two reads. Not fatal:
    // liveness is already established by the directory existing.
    detail.exe = null;
  }
  // A zombie holds a pid without being a running process; calling it "alive"
  // would make a leak check pass on a process that is only waiting to be reaped.
  const zombie = detail.state === 'Z';
  detail.zombie = zombie;
  return observation({
    kind: 'process', mechanism: 'procfs:stat', target: `pid:${spec.pid}`,
    present: !zombie, now: spec.now, detail,
  });
}

/**
 * Observe a TCP listener by parsing /proc/net/tcp{,6} directly.
 *
 * Deliberately not `ss`/`lsof`: no external binary, no parse of a human-facing
 * table that changes between distro versions, and no dependency on a tool the
 * plugin also happens to use.
 * @param {{ host?: string, port: number, procRoot?: string, now?: () => Date }} spec
 * @returns {Observation}
 */
export function observeListener(spec) {
  const procRoot = spec.procRoot ?? '/proc';
  const host = spec.host ?? '127.0.0.1';
  const target = `${host}:${spec.port}`;
  const wantedPort = spec.port.toString(16).toUpperCase().padStart(4, '0');
  // /proc/net/tcp encodes IPv4 little-endian per 32-bit word: 127.0.0.1 -> 0100007F.
  const wantedHost = host === '0.0.0.0' ? '00000000'
    : host.split('.').reverse().map((octet) => Number(octet).toString(16).toUpperCase().padStart(2, '0')).join('');
  /** @type {string[]} */
  const matches = [];
  let read = false;
  for (const table of ['net/tcp', 'net/tcp6']) {
    const file = join(procRoot, table);
    if (!existsSync(file)) continue;
    read = true;
    for (const line of readFileSync(file, 'utf8').split('\n').slice(1)) {
      const columns = line.trim().split(/\s+/u);
      if (columns.length < 4) continue;
      const [local, , state] = [columns[1], columns[2], columns[3]];
      if (state !== '0A') continue; // TCP_LISTEN
      const [address, port] = String(local).split(':');
      if (port !== wantedPort) continue;
      const tail = String(address).slice(-8).toUpperCase();
      if (tail === wantedHost || tail === '00000000') matches.push(`${table}:${local}`);
    }
  }
  if (!read) {
    return observation({
      kind: 'port', mechanism: 'procfs:net-tcp', target,
      present: null, conclusive: false, now: spec.now,
      detail: { reason: 'PROC_NET_TCP_UNREADABLE' },
    });
  }
  return observation({
    kind: 'port', mechanism: 'procfs:net-tcp', target,
    present: matches.length > 0, now: spec.now, detail: { matches, wantedHost, wantedPort },
  });
}

/**
 * Observe whether a native `/mcp` session is still alive, on a socket this
 * function opens itself.
 *
 * It does NOT reuse the driver that created the session: a driver that forgot to
 * send DELETE and a driver that mis-reports the DELETE result are the same bug,
 * and a session receipt derived from the DELETE response is that bug's alibi.
 * A live session answers 200; a released one answers 404.
 * @param {{ host?: string, port?: number, sessionId: string, token?: string|null,
 *   protocolVersion?: string, timeoutMs?: number, now?: () => Date }} spec
 * @returns {Promise<Observation>}
 */
export function observeHttpSession(spec) {
  const host = spec.host ?? '127.0.0.1';
  const port = spec.port ?? 3000;
  const target = `session:${spec.sessionId}`;
  const body = JSON.stringify({ jsonrpc: '2.0', id: 90_050, method: 'tools/list', params: {} });
  /** @type {Record<string, string>} */
  const headers = {
    'Content-Type': 'application/json',
    Accept: 'application/json, text/event-stream',
    'Content-Length': String(Buffer.byteLength(body)),
    'Mcp-Session-Id': spec.sessionId,
  };
  if (spec.protocolVersion) headers['MCP-Protocol-Version'] = spec.protocolVersion;
  if (spec.token) headers['X-MCP-Capability-Token'] = spec.token;

  return new Promise((settle) => {
    const done = (/** @type {Observation} */ result) => settle(result);
    const call = httpRequest({ host, port, path: '/mcp', method: 'POST', headers }, (response) => {
      let text = '';
      response.setEncoding('utf8');
      response.on('data', (chunk) => { text = `${text}${chunk}`.slice(0, 4096); });
      response.on('end', () => done(observation({
        kind: 'session', mechanism: 'http:independent-probe', independence: INDEPENDENCE.CROSS_TRANSPORT,
        target, present: response.statusCode === 200, now: spec.now,
        detail: { status: response.statusCode ?? 0, bodyPreview: text.slice(0, 400) },
      })));
    });
    call.setTimeout(spec.timeoutMs ?? 5_000, () => {
      call.destroy();
      done(observation({
        kind: 'session', mechanism: 'http:independent-probe', independence: INDEPENDENCE.CROSS_TRANSPORT,
        target, present: null, conclusive: false, now: spec.now, detail: { reason: 'TIMEOUT' },
      }));
    });
    call.on('error', (error) => done(observation({
      kind: 'session', mechanism: 'http:independent-probe', independence: INDEPENDENCE.CROSS_TRANSPORT,
      target,
      // Connection refused means nothing is listening, so the session cannot be
      // alive. That is a real negative, not a failed look.
      present: /ECONNREFUSED/u.test(String(error?.message)) ? false : null,
      conclusive: /ECONNREFUSED/u.test(String(error?.message)),
      now: spec.now, detail: { error: String(error?.message ?? error) },
    })));
    call.end(body);
  });
}

/**
 * Observe the editor's OWN log file for a marker.
 *
 * The editor writes this; the MCP response does not. When the two disagree the
 * log is the one that watched the game thread.
 * @param {{ file: string, pattern: RegExp, maxBytes?: number, now?: () => Date }} spec
 * @returns {Observation}
 */
export function observeEditorLog(spec) {
  const file = resolve(spec.file);
  if (!existsSync(file)) {
    return observation({
      kind: 'log', mechanism: 'fs:editor-log', target: `${file}::${spec.pattern.source}`,
      present: null, conclusive: false, now: spec.now, detail: { reason: 'LOG_ABSENT' },
    });
  }
  const maxBytes = spec.maxBytes ?? 2 * 1024 * 1024;
  const text = readFileSync(file, 'utf8');
  const tail = text.slice(Math.max(0, text.length - maxBytes));
  const hit = spec.pattern.exec(tail);
  return observation({
    kind: 'log', mechanism: 'fs:editor-log', target: `${file}::${spec.pattern.source}`,
    present: hit !== null, now: spec.now,
    detail: { file, scannedBytes: Math.min(text.length, maxBytes), match: hit === null ? null : hit[0].slice(0, 300) },
  });
}

/**
 * Wrap a read performed over a DIFFERENT transport than the mutation used.
 *
 * Kept deliberately thin and clearly labelled: it is corroboration, never proof
 * on its own, because it still traverses the plugin and the subsystem queue that
 * the mutation traversed. `requireIndependentProof` enforces that.
 * @param {{ target: string, present: boolean|null, transport: string,
 *   detail?: Record<string, unknown>, now?: () => Date }} spec
 * @returns {Observation}
 */
export function crossTransportObservation(spec) {
  return observation({
    kind: 'asset', mechanism: `mcp:${spec.transport}-read`, independence: INDEPENDENCE.CROSS_TRANSPORT,
    target: spec.target, present: spec.present, now: spec.now,
    detail: { ...(spec.detail ?? {}), transport: spec.transport },
  });
}

/** Join an Unreal object path from parts without letting a caller hand-roll separators.
 * @param {...string} parts */
export const gamePath = (...parts) => posix.join('/', ...parts.map((part) => String(part).replace(/^\/+|\/+$/gu, '')));
