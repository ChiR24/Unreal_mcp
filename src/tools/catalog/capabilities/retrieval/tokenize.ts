import { RETRIEVAL_TOKENIZATION } from './constants.js';

const CAMEL_CASE_BOUNDARY = /([a-z0-9])([A-Z])/g;
const ASCII_TOKEN_PATTERN = /[a-z0-9]+/g;

export function tokenizeCapabilityText(value: string): readonly string[] {
  const expanded = RETRIEVAL_TOKENIZATION.splitCamelCase
    ? value.replace(CAMEL_CASE_BOUNDARY, '$1 $2')
    : value;
  const matches = expanded.toLowerCase().match(ASCII_TOKEN_PATTERN);
  if (matches === null) return [];
  return matches
    .slice(0, RETRIEVAL_TOKENIZATION.maxTokens)
    .map((token) => token.slice(0, RETRIEVAL_TOKENIZATION.maxTokenLength));
}

export function uniqueCapabilityTokens(value: string): readonly string[] {
  const seen = new Set<string>();
  const tokens: string[] = [];
  for (const token of tokenizeCapabilityText(value)) {
    if (seen.has(token)) continue;
    seen.add(token);
    tokens.push(token);
  }
  return tokens;
}
