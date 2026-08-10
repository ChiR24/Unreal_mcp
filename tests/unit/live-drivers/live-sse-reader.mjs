// @ts-check
// tests/unit/live-drivers/live-sse-reader.mjs
// Task 49 — an incremental, fragmentation-tolerant reader for `text/event-stream`.
//
// WHY THIS EXISTS. Every prior probe in scripts/qa/ parses SSE the same wrong way:
//
//     body.split('\n').filter(l => l.startsWith('data: ')).map(l => l.slice(6))
//
// That works only because those probes buffer the ENTIRE response first and then
// parse the finished string. The moment you read a live stream — which is the
// whole point of `GET /mcp` and of streamed progress on `POST /mcp` — TCP hands
// you arbitrary chunk boundaries. A chunk can end mid-field ("da"), mid-value,
// mid-UTF-8-sequence, or carry three complete events at once. Assuming one event
// per read is the single most common way an SSE client silently drops the frame
// that mattered, and the task brief names it explicitly as out of bounds.
//
// So this reader is a state machine over bytes:
//   - a StringDecoder holds partial multi-byte characters across chunk boundaries
//   - a line buffer holds a partial line across chunk boundaries
//   - an event accumulator holds a partial EVENT across many lines and chunks
//   - an event is dispatched ONLY on a blank line, per the WHATWG SSE grammar
//
// It implements the parts of the grammar the MCP Streamable HTTP transport uses:
// `data:`, `event:`, `id:`, `retry:`, comment lines (`:`), CRLF/CR/LF line
// endings, a single optional space after the colon, and multi-line `data` joined
// with "\n". Unknown fields are ignored, as the spec requires.

import { StringDecoder } from 'node:string_decoder';

/** @typedef {{ event: string|null, data: string, id: string|null, retry: number|null }} SseEvent */

const FIELD_SEPARATOR = ':';

/**
 * Incremental SSE parser. Feed it whatever bytes arrived; it returns only the
 * events that are COMPLETE, and keeps everything else for the next feed.
 */
export class SseReader {
  constructor() {
    this.decoder = new StringDecoder('utf8');
    /** Text received but not yet terminated by a newline. */
    this.lineBuffer = '';
    /** Fields of the event currently being accumulated. */
    this.dataLines = /** @type {string[]} */ ([]);
    this.eventName = /** @type {string|null} */ (null);
    this.lastEventId = /** @type {string|null} */ (null);
    this.retry = /** @type {number|null} */ (null);
    /** True once any field of the in-flight event has been seen. */
    this.pending = false;
    /** Total events dispatched, so a caller can assert it observed more than zero. */
    this.dispatched = 0;
  }

  /**
   * Feed one chunk. Boundaries are arbitrary: a chunk may be a single byte, may
   * split a multi-byte character, may end mid-line, or may contain many events.
   * @param {Buffer|string} chunk
   * @returns {SseEvent[]} the events completed by THIS chunk, in order
   */
  push(chunk) {
    const text = typeof chunk === 'string' ? chunk : this.decoder.write(chunk);
    this.lineBuffer += text;

    /** @type {SseEvent[]} */
    const out = [];
    // Normalize CRLF and lone CR to LF only at line boundaries we actually have.
    // A trailing lone CR is NOT consumed here: it might be the first half of a
    // CRLF whose LF arrives in the next chunk, and collapsing it early would
    // dispatch an event one read too soon.
    for (;;) {
      const boundary = this.#nextLineBoundary();
      if (boundary === null) break;
      const line = this.lineBuffer.slice(0, boundary.index);
      this.lineBuffer = this.lineBuffer.slice(boundary.index + boundary.width);
      const event = this.#consumeLine(line);
      if (event !== null) out.push(event);
    }
    return out;
  }

  /**
   * Find the next complete line terminator. Returns null when the buffer holds
   * only a partial line — including the ambiguous trailing "\r".
   * @returns {{ index: number, width: number }|null}
   */
  #nextLineBoundary() {
    const lf = this.lineBuffer.indexOf('\n');
    const cr = this.lineBuffer.indexOf('\r');
    if (lf < 0 && cr < 0) return null;
    if (cr >= 0 && (lf < 0 || cr < lf)) {
      // A CR at the very end could still become CRLF; wait for more bytes.
      if (cr === this.lineBuffer.length - 1) return null;
      const width = this.lineBuffer[cr + 1] === '\n' ? 2 : 1;
      return { index: cr, width };
    }
    return { index: lf, width: 1 };
  }

  /**
   * Apply one complete line to the in-flight event.
   * @param {string} line @returns {SseEvent|null} an event if this line ended one
   */
  #consumeLine(line) {
    if (line === '') return this.#dispatch();
    if (line.startsWith(FIELD_SEPARATOR)) return null; // comment / keep-alive

    const colon = line.indexOf(FIELD_SEPARATOR);
    const field = colon < 0 ? line : line.slice(0, colon);
    let value = colon < 0 ? '' : line.slice(colon + 1);
    if (value.startsWith(' ')) value = value.slice(1);

    switch (field) {
      case 'data': this.dataLines.push(value); this.pending = true; break;
      case 'event': this.eventName = value; this.pending = true; break;
      case 'id': if (!value.includes('\0')) { this.lastEventId = value; this.pending = true; } break;
      case 'retry': {
        const ms = Number(value);
        if (Number.isInteger(ms) && ms >= 0) { this.retry = ms; this.pending = true; }
        break;
      }
      default: break; // unknown field: ignored per the SSE grammar
    }
    return null;
  }

  /** @returns {SseEvent|null} */
  #dispatch() {
    if (!this.pending) return null;
    const event = {
      event: this.eventName,
      data: this.dataLines.join('\n'),
      id: this.lastEventId,
      retry: this.retry,
    };
    this.dataLines = [];
    this.eventName = null;
    this.retry = null;
    this.pending = false;
    this.dispatched += 1;
    return event;
  }

  /**
   * End of stream. A server that closes without a final blank line still owes us
   * the event it was mid-way through emitting; dropping it would lose exactly the
   * terminal `result` frame a short-lived POST stream ends on.
   * @returns {SseEvent[]}
   */
  end() {
    const tail = this.decoder.end();
    /** @type {SseEvent[]} */
    const out = [];
    if (tail.length > 0) out.push(...this.push(tail));
    if (this.lineBuffer.length > 0) {
      const line = this.lineBuffer.replace(/\r$/u, '');
      this.lineBuffer = '';
      const event = this.#consumeLine(line);
      if (event !== null) out.push(event);
    }
    const final = this.#dispatch();
    if (final !== null) out.push(final);
    return out;
  }
}

/**
 * Parse each event's `data` as JSON, skipping frames that are not JSON (keep-alive
 * comments never reach here; a non-JSON data frame is a server-side concern this
 * reader reports by omission rather than by throwing mid-stream).
 * @param {readonly SseEvent[]} events
 * @returns {unknown[]}
 */
export function jsonFrames(events) {
  /** @type {unknown[]} */
  const out = [];
  for (const event of events) {
    if (event.data.length === 0) continue;
    try { out.push(JSON.parse(event.data)); } catch { /* not a JSON frame */ }
  }
  return out;
}

/**
 * Read a whole body that may be EITHER a plain JSON response or an SSE stream,
 * returning every JSON-RPC frame it carried. This is the one place that decides
 * "is this SSE?", so no driver has to guess from a content type it may not have.
 * @param {string} body
 * @returns {unknown[]}
 */
export function framesFromBody(body) {
  const reader = new SseReader();
  const events = [...reader.push(body), ...reader.end()];
  const frames = jsonFrames(events);
  if (frames.length > 0) return frames;
  try { return [JSON.parse(body)]; } catch { return []; }
}
