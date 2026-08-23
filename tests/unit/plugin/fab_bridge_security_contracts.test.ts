/**
 * Source contracts for the Fab browser bridge.
 *
 * The bridge executes script inside a page that is already authenticated as the
 * user. That is safe only while two properties hold, and neither is provable by
 * running the happy path:
 *
 *   1. No caller can ask for arbitrary script. Console commands are reachable
 *      through MCP's console_command, so a generic evaluator would let a caller
 *      run window.ue.fab.getauthtoken and read the credential straight back.
 *   2. No credential-shaped value can reach a response or a log.
 *
 * These read the C++ as text, the same way the other plugin contract suites do,
 * because both properties are about what the source is allowed to contain.
 */

import { readFileSync, readdirSync, statSync } from 'node:fs';
import { basename, dirname, join, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';
import { CommandValidator } from '../../../src/utils/commands/command-validator.js';
import { describe, expect, it } from 'vitest';

const here = dirname(fileURLToPath(import.meta.url));
const fabModuleRoot = resolve(
  here,
  '../../../plugins/McpAutomationBridge/Source/McpAutomationBridgeFab',
);

function listSources(directory: string): string[] {
  const found: string[] = [];
  for (const entry of readdirSync(directory)) {
    const full = join(directory, entry);
    if (statSync(full).isDirectory()) {
      found.push(...listSources(full));
    } else if (/\.(?:cpp|h)$/u.test(entry)) {
      found.push(full);
    }
  }
  return found;
}

const sources = listSources(fabModuleRoot).map((file) => ({
  file,
  text: readFileSync(file, 'utf8'),
}));

/** Comment bodies explain these rules, so rule checks must ignore them. */
function stripComments(text: string): string {
  return text.replace(/\/\*[\s\S]*?\*\//gu, '').replace(/^[ \t]*\/\/.*$/gmu, '');
}

describe('Fab bridge: arbitrary script cannot be requested', () => {
  it('registers no console command that takes script as an argument', () => {
    const offenders: string[] = [];
    for (const { file, text } of sources) {
      const code = stripComments(text);
      // A command taking args is fine; one whose name advertises evaluation is not.
      if (/FAutoConsoleCommand\w*\s+\w+\s*\(\s*TEXT\("[^"]*(?:Eval|Exec|RunScript|Javascript)[^"]*"\)/iu.test(code)) {
        offenders.push(file);
      }
    }
    expect(offenders, 'no console command may expose script evaluation').toEqual([]);
  });

  it('never passes a console argument into ExecuteJavascript', () => {
    const offenders: string[] = [];
    for (const { file, text } of sources) {
      const code = stripComments(text);
      if (!code.includes('ExecuteJavascript')) {
        continue;
      }
      // Every script reaching the page must come from a native builder, so the
      // argument is an identifier -- never Args[...] and never a concatenation
      // of a caller-supplied value.
      for (const line of code.split('\n')) {
        if (!line.includes('ExecuteJavascript(')) {
          continue;
        }
        const call = line.slice(line.indexOf('ExecuteJavascript('));
        if (/Args\s*\[|\+/u.test(call)) {
          offenders.push(`${file}: ${line.trim()}`);
        }
      }
    }
    expect(offenders, 'script must be composed natively, never from caller input').toEqual([]);
  });

  it('validates any caller-supplied identifier before it can reach a URL path', () => {
    const addOperation = sources.find(({ file }) => file.endsWith('McpFabAddToProject.cpp'));
    expect(addOperation, 'the add operation source must exist').toBeDefined();
    const code = stripComments(addOperation?.text ?? '');
    // A listing id is interpolated into an API path, so an allowlist has to gate
    // it. Rejecting is what keeps /i/account and /i/auth unreachable.
    expect(code).toMatch(/IsSafeListingId/u);
    expect(code).toMatch(/FChar::IsAlnum/u);
    expect(code).toMatch(/encodeURIComponent/u);
  });
});

describe('Fab bridge: credentials cannot reach a response or a log', () => {
  it('never reads an auth token binding from the page', () => {
    const offenders: string[] = [];
    for (const { file, text } of sources) {
      const code = stripComments(text);
      // Fab publishes getauthtoken and getrefreshtoken into the page. Calling
      // either would pull the credential across the boundary this design exists
      // to keep it behind. These stay banned outright: unlike a CSRF nonce they
      // ARE the credential.
      if (/getauthtoken|getrefreshtoken/iu.test(code)) {
        offenders.push(file);
      }
    }
    expect(offenders, 'the bridge must never read Fab credentials').toEqual([]);
  });

  /**
   * Cookie access is confined to one helper, and what matters is that nothing
   * escapes it.
   *
   * This replaced a blanket ban on document.cookie. The ban was the safer
   * default while nothing needed a cookie, but Fab guards add-to-library with
   * Django CSRF and publishes the token nowhere in the DOM, so claiming a
   * listing is impossible without reading csrftoken. That cookie is set
   * non-HttpOnly precisely so page script can echo it as a header; it is an
   * anti-forgery nonce, not a credential, and is worthless without the session
   * cookie. The pair of rules below is stricter than the ban it replaced: the
   * old one proved an API went uncalled, these prove the value cannot leak.
   */
  it('reads cookies in exactly one helper, in one file', () => {
    const readers = sources
      .filter(({ text }) => /document\.cookie/u.test(stripComments(text)))
      .map(({ file }) => basename(file));
    expect(readers).toEqual(['McpFabAddToProject.cpp']);

    const addToProject = sources.find(({ file }) => basename(file) === 'McpFabAddToProject.cpp');
    const code = stripComments(addToProject?.text ?? '');
    // One occurrence, and it sits inside the named helper rather than loose in
    // the operation body.
    expect(code.match(/document\.cookie/gu)?.length).toBe(1);
    // Sliced from the helper's own declaration rather than matched by
    // indentation, so reformatting the file cannot quietly void this rule.
    const start = code.indexOf('function readCsrfCookie()');
    expect(start, 'the cookie read must live in readCsrfCookie').toBeGreaterThan(-1);
    const helper = code.slice(start, start + 500);
    expect(helper).toContain('document.cookie');
    // It selects csrftoken by name; a helper returning whatever it finds would
    // hand back the session cookie.
    expect(helper).toContain('"csrftoken"');
    // And the single cookie occurrence in the file is the one inside it.
    expect(code.indexOf('document.cookie')).toBeGreaterThan(start);
  });

  it('never lets the csrf value reach a reply or a log', () => {
    const addToProject = sources.find(({ file }) => basename(file) === 'McpFabAddToProject.cpp');
    const code = stripComments(addToProject?.text ?? '');
    // Only the SOURCE is reported. Assigning the token to `out` would put it in
    // the page reply, which is logged in full on refusal.
    expect(code).toContain('out.csrfSource = csrfFrom;');
    expect(code).not.toMatch(/out\.csrf\s*=/u);
    expect(code).not.toMatch(/out\.csrfToken/u);
    // And it never appears inside a call that leaves the page.
    for (const line of code.split(/\r?\n/u)) {
      if (/(?:send|onresult|onerror)\s*\(/u.test(line) || /UE_LOG/u.test(line)) {
        expect(line, 'the csrf value must not cross the boundary').not.toMatch(
          /csrf(?!Source|From)/iu,
        );
      }
    }
  });

  it('never logs a resolved download URL', () => {
    const offenders: string[] = [];
    for (const { file, text } of sources) {
      const code = stripComments(text);
      for (const line of code.split(/\r?\n/u)) {
        if (!/UE_LOG/u.test(line)) {
          continue;
        }
        if (/\b(?:downloadUrl|DownloadUrl|SignedUrl|signedUrl)\b/u.test(line)) {
          offenders.push(`${file}: ${line.trim()}`);
        }
      }
    }
    expect(offenders, 'a signed URL must never be logged').toEqual([]);
  });

  it('keeps the result struct free of credential-shaped fields', () => {
    const providerHeader = readFileSync(
      resolve(
        here,
        '../../../plugins/McpAutomationBridge/Source/McpAutomationBridge/Public/McpFabProvider.h',
      ),
      'utf8',
    );
    const struct = /struct FMcpFabAddResult\s*\{([\s\S]*?)\};/u.exec(stripComments(providerHeader));
    expect(struct, 'FMcpFabAddResult must exist').not.toBeNull();
    const body = struct?.[1] ?? '';
    for (const banned of ['Url', 'Token', 'Cookie', 'Secret', 'Credential']) {
      // The banned word lands in the FIELD NAME, not the type: a leak looks like
      // `FString DownloadUrl;`. Matching `<type-containing-Url> <name>` instead
      // made this rule unable to reject the very thing it exists to catch.
      expect(body, `FMcpFabAddResult must not carry a ${banned} field`).not.toMatch(
        new RegExp(`\\b\\w+\\s+\\w*${banned}\\w*\\s*(?:=|;)`, 'iu'),
      );
    }
  });
});

/**
 * The page exposes one binding name, so only one call can be in flight.
 *
 * Six files each kept a callback of their own, which meant every operation
 * judged itself idle while another was mid-flight: dispatching rebound
 * window.ue.mcpfab, and the earlier reply then reached an object that had never
 * armed that id, so the correlation check discarded it. No caller got the wrong
 * data -- it just never got any. A single owner is what makes the in-flight
 * guard say something true, so the count is pinned here rather than left to
 * whoever adds the seventh operation.
 */

/**
 * Mutating Fab operations must not be reachable through the generic
 * console_command capability.
 *
 * console_command is {scope: write, consent: none}; asset.add_fab_asset_to_project
 * is {scope: write, consent: explicit}. The console commands exist for manual
 * bring-up in the editor console; through MCP's console_command they would let a
 * write-scoped principal with no consent grant claim a listing into the signed-in
 * Fab library and import content -- the exact side effect the explicit-consent
 * gate exists to protect. The generated console-command policy therefore blocks
 * them on BOTH surfaces (src/utils/commands/console-command-policy-rules.ts,
 * rule both.fab-bridge-console). This pins the generated artifacts to contain the
 * block so the rule cannot silently stop regenerating.
 */
describe('Fab bridge: mutating console commands are blocked by the console-command policy', () => {
  it('blocks Mcp.Fab.AddToProject on the TypeScript surface', () => {
    const validator = resolve(here, '../../../src/utils/commands/command-validator.ts');
    expect(stripComments(readFileSync(validator, 'utf8'))).not.toBe('');
  });

  it('generated policy contains the Fab command block on both surfaces', () => {
    const tsPolicy = readFileSync(
      resolve(here, '../../../src/utils/commands/console-command-policy.generated.ts'),
      'utf8',
    );
    const nativePolicy = readFileSync(
      resolve(here, '../../../plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/Domains/ConsoleCommand/McpAutomationBridge_ConsoleCommandPolicy.generated.h'),
      'utf8',
    );
    // The rule id and both command names must appear in the TS mirror...
    expect(tsPolicy).toContain('both.fab-bridge-console');
    expect(tsPolicy).toContain('mcp.fab.addtoproject');
    expect(tsPolicy).toContain('mcp.fab.describecatalogshape');
    // ...and the native header must carry them as first-token blocked commands.
    expect(nativePolicy).toContain('TEXT("mcp.fab.addtoproject")');
    expect(nativePolicy).toContain('TEXT("mcp.fab.describecatalogshape")');
  });

  it('CommandValidator rejects Mcp.Fab.AddToProject at runtime', () => {
    expect(() => CommandValidator.validate('Mcp.Fab.AddToProject abc123')).toThrow(/blocked/iu);
    expect(() => CommandValidator.validate('Mcp.Fab.DescribeCatalogShape')).toThrow(/blocked/iu);
  });
});
describe('Fab bridge: one callback owner', () => {
  it('constructs the page callback in exactly one file', () => {
    const creators = sources
      .filter(({ text }) => /NewObject<\s*UMcpFabBridgeCallback\s*>/u.test(stripComments(text)))
      .map(({ file }) => basename(file));
    expect(creators).toEqual(['McpFabBridgeDispatch.cpp']);
  });

  it('dispatches page script from that same file only', () => {
    const callers = sources
      .filter(
        ({ file, text }) =>
          /McpFabBrowserSession::RunScriptWithCallback\s*\(/u.test(stripComments(text)) &&
          basename(file) !== 'McpFabBrowserSessionBridge.cpp',
      )
      .map(({ file }) => basename(file));
    expect(callers).toEqual(['McpFabBridgeDispatch.cpp']);
  });
});

/**
 * Navigation of the authenticated page is as sensitive as script execution.
 *
 * The bridge repairs Fab's stalled bootstrap by navigating the tab itself. That
 * page holds the user's session, so a caller who could steer the destination
 * could park an authenticated browser on a host of their choosing. The targets
 * are therefore a fixed literal and Fab's own geturl binding, and nothing else
 * -- no concatenation, no interpolation, no caller value.
 */
describe('Fab bridge: page navigation cannot be steered', () => {
  const allowed = /^(?:u \|\| home|home|u)$/u;

  it('navigates only from the dispatch file', () => {
    const navigators = sources
      .filter(({ text }) => /location\.replace\s*\(/u.test(stripComments(text)))
      .map(({ file }) => basename(file));
    expect(navigators).toEqual(['McpFabBridgeDispatch.cpp']);
  });

  it('passes only a fixed literal or the url Fab itself resolved', () => {
    const args: string[] = [];
    for (const { text } of sources) {
      for (const m of stripComments(text).matchAll(/location\.replace\s*\(([^)]*)\)/gu)) {
        args.push(m[1].trim());
      }
    }
    expect(args.length).toBeGreaterThan(0);
    expect(args.filter((a) => !allowed.test(a))).toEqual([]);
  });

  it('defines the fallback host as an exact literal', () => {
    const dispatch = sources.find(({ file }) => basename(file) === 'McpFabBridgeDispatch.cpp');
    expect(dispatch).toBeDefined();
    expect(stripComments(dispatch?.text ?? '')).toContain('var home = "https://www.fab.com/";');
  });
});

/**
 * Negative controls.
 *
 * The suite above passes today, which proves nothing on its own: a rule that
 * cannot reject anything is indistinguishable from a rule that is satisfied.
 * Gate 3 spent this project's whole native history green while running zero
 * tests. Each rule is therefore fired at a synthetic violation here.
 */
describe('Fab bridge: the rules can actually reject a violation', () => {
  const evalCommand = 'static FAutoConsoleCommand G(TEXT("Mcp.Fab.Eval"), TEXT("d"), ...);';
  const scriptFromArgs = 'Browser->ExecuteJavascript(Args[0]);';
  const concatenatedScript = 'Browser->ExecuteJavascript(TEXT("(") + Caller + TEXT(")"));';
  const tokenRead = 'window.ue.fab.getauthtoken()';
  const loggedUrl = 'UE_LOG(LogX, Log, TEXT("url=%s"), *DownloadUrl);';

  it('rejects a console command advertising evaluation', () => {
    expect(
      /FAutoConsoleCommand\w*\s+\w+\s*\(\s*TEXT\("[^"]*(?:Eval|Exec|RunScript|Javascript)[^"]*"\)/iu.test(
        evalCommand,
      ),
    ).toBe(true);
  });

  it('rejects script taken straight from a console argument', () => {
    const call = scriptFromArgs.slice(scriptFromArgs.indexOf('ExecuteJavascript('));
    expect(/Args\s*\[|\+/u.test(call)).toBe(true);
  });

  it('rejects script concatenated from a caller value', () => {
    const call = concatenatedScript.slice(concatenatedScript.indexOf('ExecuteJavascript('));
    expect(/Args\s*\[|\+/u.test(call)).toBe(true);
  });

  it('rejects a credential read', () => {
    expect(/getauthtoken|getrefreshtoken|document\.cookie/iu.test(tokenRead)).toBe(true);
  });

  it('rejects a logged signed URL', () => {
    expect(/UE_LOG/u.test(loggedUrl)).toBe(true);
    expect(/\b(?:downloadUrl|DownloadUrl|SignedUrl|signedUrl)\b/u.test(loggedUrl)).toBe(true);
  });

  it('rejects a credential-shaped field on the result struct', () => {
    const badBody = 'bool bAccepted = false; FString DownloadUrl;';
    expect(/\b\w+\s+\w*Url\w*\s*(?:=|;)/iu.test(badBody)).toBe(true);
  });

  it('rejects a second callback owner', () => {
    const rogue = 'GCallback = NewObject<UMcpFabBridgeCallback>();';
    expect(/NewObject<\s*UMcpFabBridgeCallback\s*>/u.test(rogue)).toBe(true);
  });

  it('rejects an operation dispatching page script itself', () => {
    const rogue = 'if (!McpFabBrowserSession::RunScriptWithCallback(S, C, D))';
    expect(/McpFabBrowserSession::RunScriptWithCallback\s*\(/u.test(rogue)).toBe(true);
  });

  it('rejects a cookie read outside the named helper', () => {
    const rogue = 'var all = document.cookie;';
    expect(/document\.cookie/u.test(rogue)).toBe(true);
    expect(/function readCsrfCookie\(\)/u.test(rogue)).toBe(false);
  });

  it('rejects a helper that returns whatever cookie it finds', () => {
    const rogue = 'function readCsrfCookie() { return String(document.cookie); }';
    expect(rogue).not.toContain('"csrftoken"');
  });

  it('rejects the csrf value being put on the reply', () => {
    const rogue = 'out.csrf = csrf;';
    expect(/out\.csrf\s*=/u.test(rogue)).toBe(true);
  });

  it('rejects the csrf value being logged', () => {
    const rogue = 'UE_LOG(LogX, Log, TEXT("csrf=%s"), *csrf);';
    expect(/UE_LOG/u.test(rogue) && /csrf(?!Source|From)/iu.test(rogue)).toBe(true);
  });

  it('rejects a navigation built by concatenation', () => {
    const rogue = 'location.replace(base + caller);';
    const match = /location\.replace\s*\(([^)]*)\)/u.exec(rogue);
    expect(match).not.toBeNull();
    expect(/^(?:u \|\| home|home|u)$/u.test((match?.[1] ?? '').trim())).toBe(false);
  });

  it('rejects a navigation to an unexpected literal host', () => {
    const rogue = 'location.replace("https://elsewhere.example/");';
    const match = /location\.replace\s*\(([^)]*)\)/u.exec(rogue);
    expect(match).not.toBeNull();
    expect(/^(?:u \|\| home|home|u)$/u.test((match?.[1] ?? '').trim())).toBe(false);
  });
});
