import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';

import { describe, expect, it } from 'vitest';

import {
  CREDENTIAL_TAILS,
  GENERIC_VALUE_KEYS,
  MEASUREMENT_HEADS,
  NAME_BEARING_KEYS,
  SECRET_KEY_WORDS,
  SECRET_QUALIFIERS,
  TRANSPARENT_HEADS
} from '../../../src/tools/catalog/capabilities/semantic/receipt-redaction.js';

// TS/native parity contract for the key-name credential classifier.
//
// `isSecretKey()` (receipt-redaction.ts) and `McpIsSecretKey()` /
// `McpNamesCredentialBySibling()` (McpNativeReceiptSecretKeys.cpp) must agree on
// the closed vocabulary and on how a separator-less compound is split, or one
// transport masks a credential that the other ships in the clear. The C++ cannot
// run under Vitest, so the contract is enforced structurally: the vocabulary sets
// must be byte-identical, and the credential predicate must be applied only to a
// COMPLETE segmentation (never to a sub-segmentation during recursion), which is
// the exact divergence that once left the native surface masking less than stdio.

const CPP = readFileSync(
  resolve(
    process.cwd(),
    'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/MCP/Execute',
    'McpNativeReceiptSecretKeys.cpp'
  ),
  'utf8'
);

function extractVocabulary(source: string, fnName: string): Set<string> {
  const fnStart = source.indexOf(`${fnName}()`);
  if (fnStart === -1) throw new Error(`missing C++ vocabulary function ${fnName}()`);
  const wordsDecl = source.indexOf('static const TSet<FString> Words', fnStart);
  if (wordsDecl === -1) throw new Error(`missing Words set in ${fnName}()`);
  const open = source.indexOf('{', wordsDecl);
  const close = source.indexOf('};', open);
  if (open === -1 || close === -1) throw new Error(`unbounded Words set in ${fnName}()`);
  const body = source.slice(open + 1, close);
  return new Set([...body.matchAll(/TEXT\("([^"]+)"\)/g)].map((match) => match[1]));
}

// Returns the body of a file-scope function, matching braces from its first `{`
// to the closing brace that returns the depth to zero. The classifier functions
// carry no braces inside string literals, so a plain counter is exact.
function functionBody(source: string, signature: string): string {
  const start = source.indexOf(signature);
  if (start === -1) throw new Error(`missing function ${signature}`);
  const open = source.indexOf('{', start);
  if (open === -1) throw new Error(`missing body for ${signature}`);
  let depth = 0;
  for (let i = open; i < source.length; i += 1) {
    if (source[i] === '{') depth += 1;
    else if (source[i] === '}') {
      depth -= 1;
      if (depth === 0) return source.slice(open + 1, i);
    }
  }
  throw new Error(`unbalanced braces in ${signature}`);
}

describe('receipt redaction — native vocabulary mirrors the TS classifier', () => {
  it('matches SECRET_KEY_WORDS', () => {
    expect(extractVocabulary(CPP, 'SecretKeyWords')).toEqual(SECRET_KEY_WORDS);
  });

  it('matches TRANSPARENT_HEADS', () => {
    expect(extractVocabulary(CPP, 'TransparentHeads')).toEqual(TRANSPARENT_HEADS);
  });

  it('matches MEASUREMENT_HEADS', () => {
    expect(extractVocabulary(CPP, 'MeasurementHeads')).toEqual(MEASUREMENT_HEADS);
  });

  it('matches SECRET_QUALIFIERS', () => {
    expect(extractVocabulary(CPP, 'SecretQualifiers')).toEqual(SECRET_QUALIFIERS);
  });

  it('matches CREDENTIAL_TAILS', () => {
    expect(extractVocabulary(CPP, 'CredentialTails')).toEqual(CREDENTIAL_TAILS);
  });

  it('matches GENERIC_VALUE_KEYS', () => {
    expect(extractVocabulary(CPP, 'GenericValueKeys')).toEqual(GENERIC_VALUE_KEYS);
  });

  it('matches NAME_BEARING_KEYS', () => {
    expect(extractVocabulary(CPP, 'NameBearingKeys')).toEqual(NAME_BEARING_KEYS);
  });
});

describe('receipt redaction — the compound splitter judges only complete segmentations', () => {
  it('applies the credential predicate in AppendCompound, not during recursion', () => {
    expect(functionBody(CPP, 'void CollectSegmentations')).not.toContain(
      'PartsNameCredential'
    );
    expect(functionBody(CPP, 'void AppendCompound')).toContain('PartsNameCredential');
  });
});
