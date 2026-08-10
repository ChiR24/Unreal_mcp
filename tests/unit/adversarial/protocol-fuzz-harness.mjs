// @ts-check
// tests/unit/adversarial/protocol-fuzz-harness.mjs
// Task 51 — feed the seeded malformed JSON-RPC corpus to the REAL built server.
//
// The corpus generator produces the frames; counting them proves nothing. This puts
// them on the wire of `node dist/cli.js` and watches what happens, which is the only
// way to learn that a batch-inside-a-batch or a 64-deep params object does not take
// the process down.
//
// IT WRITES RAW BYTES, NOT OBJECTS. A driver that serialises a JavaScript value can
// only ever emit well-formed JSON, so it can never test the parser. The interesting
// frames — a truncated object, a bare `]`, a NUL in the middle of a string, two
// frames with no newline between them — exist only as bytes, so the harness reaches
// past the driver's `write()` to the child's stdin directly.
//
// THE ASSERTION IS SURVIVAL AND SANITY, NOT A PER-FRAME VERDICT. The MCP spec lets a
// server ignore a frame it cannot attribute to a request id, so demanding a reply to
// every malformed frame would fail a correct server. What must hold is: the process
// stays up, stdout stays pure JSON-RPC, and a WELL-FORMED request issued afterwards
// still gets its answer — that last one is what catches a parser left in a wedged
// state, which is the failure a fuzz run is actually looking for.

import { StdioDriver } from '../live-drivers/live-driver-stdio.mjs';
import { streamFor } from './fuzz-random.mjs';
import { fuzzJsonRpcFrame } from './fuzz-protocol.mjs';
import { loadEnv, processAlive } from './load-harness.mjs';

/** Byte-level malformations no object serialiser can produce. */
export function rawMalformations() {
  return [
    '{"jsonrpc":"2.0","id":1,"method":"tools/list"',           // truncated
    ']',                                                        // stray close
    '{}',                                                       // empty object
    '[]',                                                       // empty batch
    'null',
    'not json at all',
    '{"jsonrpc":"2.0","id":1,"method":"tools/list","params":{}}{"jsonrpc":"2.0","id":2,"method":"tools/list","params":{}}',
    '{"jsonrpc":"2.0","id":1,"method":"tools/li\u0000st","params":{}}',
    `{"jsonrpc":"2.0","id":1,"method":"tools/list","params":{"x":"${'a'.repeat(64 * 1024)}"}}`,
    '{"jsonrpc":"2.0","id":1,"method":"tools/list","params":{"x":"\uD800"}}',
    '   ',
    '\t\t',
  ];
}

/**
 * Run the protocol fuzz against one live child.
 * @param {{ frames: number, seed: number|string, cwd?: string, env?: NodeJS.ProcessEnv,
 *   settleMs?: number }} spec
 */
export async function runProtocolFuzz(spec) {
  const cwd = spec.cwd ?? process.cwd();
  const env = loadEnv(spec.env ?? process.env);
  const driver = new StdioDriver({ cwd, env, clientName: 'task51-protocol-fuzz' });
  const started = await driver.start({ timeoutMs: 60_000 });
  const pid = typeof started.pid === 'number' ? started.pid : 0;
  if (!started.ok || pid === 0) {
    await driver.close();
    return { started: false, reason: started.reason, sent: 0, survived: false, checkpoints: [] };
  }

  const rng = streamFor(spec.seed, 'json-rpc');
  /** @type {string[]} */
  const wire = [];
  for (const raw of rawMalformations()) wire.push(raw);
  for (let index = 0; index < spec.frames; index += 1) {
    wire.push(JSON.stringify(fuzzJsonRpcFrame(rng).frame));
  }

  /** @type {Array<{ afterFrames: number, answered: boolean, alive: boolean }>} */
  const checkpoints = [];
  let sent = 0;
  let writeErrors = 0;

  for (const [position, payload] of wire.entries()) {
    try {
      driver.child?.stdin?.write(`${payload}\n`);
      sent += 1;
    } catch {
      // A closed stdin means the server already died; record and stop pretending.
      writeErrors += 1;
      break;
    }
    // Every 100 frames, prove the server is STILL ANSWERING. A parser wedged by a
    // malformed frame stays alive and silently stops working; only a well-formed
    // request afterwards can tell the two apart.
    if (position % 100 === 99) {
      const alive = processAlive(pid);
      const probe = alive
        ? await driver.callTool({ operation: 'search', query: 'protocol fuzz checkpoint' }, { timeoutMs: 30_000 })
        : { response: null };
      checkpoints.push({ afterFrames: position + 1, answered: probe.response !== null, alive });
      if (!alive) break;
    }
  }

  await new Promise((settle) => { setTimeout(settle, spec.settleMs ?? 1000); });
  const aliveAfter = processAlive(pid);
  const finalProbe = aliveAfter
    ? await driver.callTool({ operation: 'search', query: 'protocol fuzz final' }, { timeoutMs: 30_000 })
    : { response: null };

  const malformedStdout = driver.decoder.malformed;
  const stderrTail = driver.stderrTail.slice(-2000);
  const close = await driver.close();

  return {
    started: true,
    pid,
    sent,
    writeErrors,
    rawMalformationsSent: rawMalformations().length,
    generatedFramesSent: sent - rawMalformations().length,
    checkpoints,
    survived: aliveAfter,
    answeredAfterFuzz: finalProbe.response !== null,
    /** Non-JSON on stdout is a protocol violation: stdout belongs to JSON-RPC. */
    malformedStdoutLines: malformedStdout,
    stderrTail,
    closed: close.stopped === true,
    releasedIndependently: !processAlive(pid),
  };
}
