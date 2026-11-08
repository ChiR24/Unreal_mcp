// Code that one supported compiler accepts and another rejects. The local build is UE 5.8 on Windows (MSVC in strict
// conformance mode), so none of these show up there; each one fails a build elsewhere:
//
// - clang's -Wcomment, an error under -Werror (macOS builds): a '/*' inside a block comment, or a // comment whose
//   line ends in a backslash (the comment swallows the next line). "Config/*.ini" in a doc comment broke a macOS build.
// - A hidden friend that calls its own name unqualified (`friend uint32 GetTypeHash(const FKey& K)` hashing an int32
//   with GetTypeHash). MSVC without /permissive- and /Zc:hiddenFriend, which is how UE 5.0 and 5.1 compile, lets the
//   friend be found by ordinary lookup, so the int32 overloads in the global namespace are never considered. Qualify
//   the call (::GetTypeHash).
// - A file-scope name that macOS declares in the global namespace. UE's Mac platform headers include Carbon and
//   CoreServices in every translation unit, so a helper named Handle in an anonymous namespace is ambiguous with
//   MacTypes.h's `typedef Ptr* Handle` wherever it is called from outside that namespace (a member function, a named
//   namespace). Names a named namespace or a class declares are safe.

import { readFileSync, readdirSync } from 'node:fs';
import { join, relative, resolve } from 'node:path';

import { describe, expect, it } from 'vitest';

const sourceRoot = resolve(process.cwd(), 'plugins/EngineWire/Source');

/**
 * The global names macOS's MacTypes.h declares (typedefs, struct tags, enumerators, functions; taken from the SDK
 * header), plus Carbon names a small helper could plausibly take. A helper with one of these names belongs in a named
 * namespace or needs another name.
 */
const MAC_GLOBAL_NAMES = new Set(
  (
    'AbsoluteTime Boolean Byte ByteCount ByteOffset BytePtr CharParameter CompTimeValue ConstLogicalAddress ' +
    'ConstStringPtr DebugStr Debugger Duration Fixed FixedPoint FixedPtr FixedRect Float32 Float32Point Float64 ' +
    'Float80 Float96 FourCharCode Fract FractPtr Handle ItemCount LangCode LogicalAddress NumVersion NumVersionVariant ' +
    'OSErr OSStatus OSType OptionBits PBVersion PRefCon PhysicalAddress Point PointPtr ProcHandle ProcessSerialNumber ' +
    'Ptr Rect RectPtr RegionCode ResType SRefCon ScriptCode ShortFixed SignedByte Size Str15 Str255 Str27 Str31 Str32 ' +
    'Str63 StrFileName StringHandle StringPtr Style StyleField StyleParameter SysBreak TimeBase TimeRecord TimeScale ' +
    'TimeValue TimeValue64 URefCon UTF16Char UTF32Char UTF8Char UniChar UniCharCount UnicodeScalarValue ' +
    'UniversalProcPtr UnsignedFixed UnsignedWide VHSelect VersRec bold condense extend extended80 italic kNilOptions ' +
    'kUnknownType noErr normal outline shadow underline wide ' +
    'Button Collection Component Delay Microseconds TickCount'
  ).split(' '),
);

interface Hit {
  readonly file: string;
  readonly line: number;
  readonly text: string;
}

function sourceFiles(dir: string, out: string[] = []): string[] {
  for (const entry of readdirSync(dir, { withFileTypes: true })) {
    const path = join(dir, entry.name);
    if (entry.isDirectory()) sourceFiles(path, out);
    else if (/\.(?:h|cpp|inl)$/u.test(entry.name)) out.push(path);
  }
  return out;
}

const lineOf = (text: string, index: number): number => text.slice(0, index).split('\n').length;

/** The source with comments and string/character literals blanked (newlines kept), and clang's -Wcomment hits. */
function lex(src: string): { code: string; comments: { index: number; what: string }[] } {
  const out: string[] = [];
  const comments: { index: number; what: string }[] = [];
  const blank = (from: number, to: number): void => {
    for (let k = from; k < to; k += 1) out.push(src[k] === '\n' ? '\n' : ' ');
  };
  let i = 0;
  while (i < src.length) {
    const c = src[i];
    const next = src[i + 1];
    if (c === '/' && next === '/') {
      let j = i + 2;
      for (;;) {
        while (j < src.length && src[j] !== '\n') j += 1;
        let k = j - 1;
        while (k > i && (src[k] === ' ' || src[k] === '\t' || src[k] === '\r')) k -= 1;
        if (j >= src.length || src[k] !== '\\') break;
        comments.push({ index: k, what: '// comment ends in a backslash and swallows the next line' });
        j += 1;
      }
      blank(i, j);
      i = j;
    } else if (c === '/' && next === '*') {
      let j = i + 2;
      while (j < src.length && !(src[j] === '*' && src[j + 1] === '/')) {
        if (src[j] === '/' && src[j + 1] === '*') comments.push({ index: j, what: "'/*' within block comment" });
        j += 1;
      }
      blank(i, Math.min(src.length, j + 2));
      i = j + 2;
    } else if (c === 'R' && next === '"' && /^(?:|u8|u|U|L)$/u.test(/[A-Za-z0-9_]*$/u.exec(src.slice(Math.max(0, i - 3), i))?.[0] ?? '')) {
      const open = src.indexOf('(', i + 2);
      const close = src.indexOf(`)${src.slice(i + 2, open)}"`, open);
      const end = close < 0 ? src.length : close + open - i;
      blank(i, end);
      i = end;
    } else if (c === '"' || (c === "'" && !/[0-9][0-9A-Za-z_]*$/u.test(src.slice(Math.max(0, i - 24), i)))) {
      let j = i + 1;
      while (j < src.length && src[j] !== c && src[j] !== '\n') j += src[j] === '\\' ? 2 : 1;
      blank(i, j + 1);
      i = j + 1;
    } else {
      out.push(c);
      i += 1;
    }
  }
  return { code: out.join(''), comments };
}

/** Preprocessor lines blanked, and only the first branch of each #if chain kept, so braces stay balanced. */
function firstBranch(code: string): string {
  const frames: boolean[] = []; // true while inside an #elif/#else branch
  let continued = false;
  return code
    .split('\n')
    .map((line) => {
      const directive = continued ? null : /^\s*#\s*(\w+)/u.exec(line)?.[1];
      const wasContinued = continued;
      if (directive || wasContinued) continued = /\\\s*$/u.test(line);
      if (directive === 'if' || directive === 'ifdef' || directive === 'ifndef') frames.push(false);
      else if (directive === 'elif' || directive === 'else') frames[frames.length - 1] = true;
      else if (directive === 'endif') frames.pop();
      const skipped = directive !== undefined || wasContinued || frames.some(Boolean);
      return skipped ? ' '.repeat(line.length) : line;
    })
    .join('\n');
}

/** The name a file-scope declaration header introduces, if any. */
function declaredName(header: string): string | undefined {
  const text = header.replace(/\s+/gu, ' ').trim();
  if (!text || /^(?:using namespace|static_assert|friend|template\b[^()]*$)/u.test(text)) return undefined;
  const typeName = /\b(?:struct|class|union|enum(?: class| struct)?) (\w+)\b(?![^(]*\()/u.exec(text)?.[1]
    ?? /^using (\w+) =/u.exec(text)?.[1]
    ?? (/^typedef\b/u.test(text) ? /(\w+)\s*(?:\[[^\]]*\])?$/u.exec(text)?.[1] : undefined);
  if (typeName) return typeName;
  // the declarator ends at the initializer (a default argument's '=' sits inside the parentheses, after the name)
  const declarator = text.replace(/<[^<>()]*>/gu, '').split(/(?<![=!<>])=(?!=)/u)[0].trim();
  const call = /([A-Za-z_]\w*)\s*\(/u.exec(declarator);
  if (call) {
    const before = declarator.slice(0, call.index).trim();
    if (!before || before.endsWith('::') || /^[A-Z0-9_]+$/u.test(call[1])) return undefined;
    return call[1];
  }
  return /([A-Za-z_]\w*)\s*(?:\[[^\]]*\])?$/u.exec(declarator)?.[1];
}

const files = sourceFiles(sourceRoot);
const lexed = files.map((file) => ({ file: relative(sourceRoot, file).replace(/\\/gu, '/'), src: readFileSync(file, 'utf8') }))
  .map(({ file, src }) => ({ file, src, ...lex(src) }));

describe('plugin source compiles on every supported compiler', () => {
  it('reads the whole plugin source', () => {
    expect(files.length).toBeGreaterThan(1000);
  });

  it("has no '/*' inside a block comment and no // comment ending in a backslash (clang -Wcomment)", () => {
    const hits: Hit[] = lexed.flatMap(({ file, src, comments }) =>
      comments.map(({ index, what }) => ({ file, line: lineOf(src, index), text: what })),
    );
    expect(hits).toEqual([]);
  });

  it('never calls a hidden friend by its own name unqualified inside its body', () => {
    const hits: Hit[] = [];
    for (const { file, code } of lexed) {
      for (const friend of code.matchAll(/\bfriend\b[^;{}()]*?\b([A-Za-z_]\w*)\s*\(/gu)) {
        const name = friend[1];
        if (name === 'operator') continue;
        let at = friend.index + friend[0].length;
        for (let depth = 1; at < code.length && depth > 0; at += 1) {
          if (code[at] === '(') depth += 1;
          else if (code[at] === ')') depth -= 1;
        }
        const open = /^[^;{]*\{/u.exec(code.slice(at));
        if (!open) continue; // a declaration, defined elsewhere
        const start = at + open[0].length;
        let end = start;
        for (let depth = 1; end < code.length && depth > 0; end += 1) {
          if (code[end] === '{') depth += 1;
          else if (code[end] === '}') depth -= 1;
        }
        for (const call of code.slice(start, end).matchAll(new RegExp(`(?<![\\w.:>~])${name}\\s*\\(`, 'gu'))) {
          hits.push({ file, line: lineOf(code, start + call.index), text: `${name}( inside the hidden friend ${name}: write ::${name}(` });
        }
      }
    }
    expect(hits).toEqual([]);
  });

  it('declares no file-scope name that macOS declares in the global namespace', () => {
    const hits: Hit[] = [];
    for (const { file, code: raw } of lexed) {
      const code = firstBranch(raw);
      // 'file': an anonymous namespace or a linkage block, whose names lookup also finds at global scope
      const scopes: ('file' | 'named' | 'other')[] = [];
      let start = 0;
      for (let i = 0; i < code.length; i += 1) {
        const c = code[i];
        if (c !== ';' && c !== '{' && c !== '}') continue;
        const header = code.slice(start, i);
        const atFileScope = scopes.every((scope) => scope === 'file');
        if (c === '}') {
          scopes.pop();
        } else {
          const kind = /\b(?:namespace|extern)\s*$/u.test(header)
            ? 'file'
            : /\bnamespace\s+[\w:]+\s*$/u.test(header) ? 'named' : 'other';
          if (atFileScope && kind === 'other') {
            const name = declaredName(header);
            if (name && MAC_GLOBAL_NAMES.has(name)) {
              hits.push({ file, line: lineOf(code, start + header.search(/\S/u)), text: `${name} is a macOS global name` });
            }
          }
          if (c === '{') scopes.push(kind);
        }
        start = i + 1;
      }
    }
    expect(hits).toEqual([]);
  });
});
