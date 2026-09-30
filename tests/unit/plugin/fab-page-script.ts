// Runs the JavaScript the Fab adapter hands to Fab's page, against a fake page.
//
// The scripts live inside C++ raw string literals, so nothing but a real editor ever ran them: a stray
// brace or a mistyped variable surfaced as a page that never answered. This lifts the literals out of the
// source, fills their printf slots, and executes them in a vm with a scripted `fetch`, so the selection
// rules and the reply shape are checked without an editor. It never touches the network.

import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';
import vm from 'node:vm';

const fabModule = resolve(
  process.cwd(),
  'plugins/McpAutomationBridge/Source/McpAutomationBridgeFab/Private',
);

/** Every `R"JS( ... )JS"` literal of a C++ file, in order, concatenated the way adjacent literals are. */
export function rawScript(file: string): string {
  const source = readFileSync(resolve(fabModule, file), 'utf8').replace(/\r\n/gu, '\n');
  const blocks = [...source.matchAll(/R"JS\(([\s\S]*?)\)JS"/gu)].map((match) => match[1] ?? '');
  if (blocks.length === 0) {
    throw new Error(`${file} holds no R"JS( literal`);
  }
  return blocks.join('');
}

/** Fills the printf `%s` and `%d` slots in order, refusing a count mismatch so a new slot cannot go unfilled. */
export function fillSlots(script: string, values: readonly (string | number)[]): string {
  const slots = script.match(/%[sd]/gu)?.length ?? 0;
  if (slots !== values.length) {
    throw new Error(`script has ${slots} printf slot(s), ${values.length} value(s) given`);
  }
  let index = 0;
  return script.replace(/%[sd]/gu, () => String(values[index++] ?? ''));
}

export type FakeResponse = { readonly status?: number; readonly body?: unknown; readonly text?: string };
export type Route = readonly [RegExp, FakeResponse | ((url: string, init?: Record<string, unknown>) => FakeResponse)];

export type PageRun = {
  /** Every result the script reported, parsed. */
  readonly results: Record<string, unknown>[];
  /** Every error the script reported through onerror. */
  readonly errors: string[];
  /** Every fetch, as `METHOD url`. */
  readonly fetches: string[];
  /** What the script handed to Fab's importer. */
  readonly addToProject: { url: string; metadata: Record<string, unknown> }[];
  /** Headers of each POST, for checking what crossed the wire. */
  readonly posts: { url: string; headers: Record<string, string>; form: Record<string, string> }[];
};

/** Executes a page script; `routes` answer its fetches in order of declaration, first match wins. */
export async function runPageScript(
  script: string,
  routes: readonly Route[],
  cookie = '',
): Promise<PageRun> {
  const run: PageRun = { results: [], errors: [], fetches: [], addToProject: [], posts: [] };

  const respond = (url: string, init?: Record<string, unknown>) => {
    run.fetches.push(`${String(init?.method ?? 'GET')} ${url}`);
    const route = routes.find(([pattern]) => pattern.test(url));
    const answer = route === undefined ? { status: 404, body: { detail: 'no route' } } : typeof route[1] === 'function' ? route[1](url, init) : route[1];
    const status = answer.status ?? 200;
    if (String(init?.method ?? 'GET') === 'POST') {
      const form = init?.body as { entries?: Record<string, string> } | undefined;
      run.posts.push({
        url,
        headers: (init?.headers as Record<string, string> | undefined) ?? {},
        form: form?.entries ?? {},
      });
    }
    return Promise.resolve({
      ok: status >= 200 && status < 300,
      status,
      json: () => Promise.resolve(answer.body ?? {}),
      text: () => Promise.resolve(answer.text ?? JSON.stringify(answer.body ?? {})),
      blob: () => Promise.resolve({ type: 'image/jpeg', size: 10 }),
    });
  };

  class FakeFormData {
    public readonly entries: Record<string, string> = {};
    public append(key: string, value: string): void {
      this.entries[key] = value;
    }
  }

  const sandbox = {
    window: {
      ue: {
        mcpfab: {
          onresult: (_id: string, payload: string) => run.results.push(JSON.parse(payload) as Record<string, unknown>),
          onerror: (_id: string, message: string) => run.errors.push(message),
        },
        fab: {
          addtoproject: (url: string, metadata: Record<string, unknown>) => run.addToProject.push({ url, metadata }),
        },
      },
    },
    document: { cookie, querySelector: () => null },
    fetch: respond,
    FormData: FakeFormData,
    setTimeout,
  };
  vm.runInNewContext(script, sandbox);
  // The scripts are promise chains over instantly-resolved fetches; a few turns settle them.
  for (let turn = 0; turn < 60; turn += 1) {
    await new Promise((settle) => setImmediate(settle));
  }
  return run;
}
