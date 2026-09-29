import { RETRIEVAL_TOKENIZATION } from './constants.js';

const CAMEL_CASE_BOUNDARY = /([a-z0-9])([A-Z])/g;
const ASCII_TOKEN_PATTERN = /[a-z0-9]+/g;

/**
 * Regular plurals and the two regular verb inflections only - deliberately NOT
 * a stemmer. Every rule is a suffix rewrite depending on nothing but the token,
 * so any surface mirroring this seam reproduces it exactly; replacing it with a
 * real stemmer would silently break that parity.
 */
function foldInflection(token: string): string {
  if (token.length > 4 && token.endsWith('ies')) return `${token.slice(0, -3)}y`;
  if (
    token.length > 4
    && (token.endsWith('ses') || token.endsWith('xes')
      || token.endsWith('ches') || token.endsWith('shes'))
  ) {
    return token.slice(0, -2);
  }
  if (token.length > 3 && token.endsWith('s') && !token.endsWith('ss')) return token.slice(0, -1);
  if (token.length > 5 && token.endsWith('ing')) return token.slice(0, -3);
  if (token.length > 4 && token.endsWith('ed')) return token.slice(0, -2);
  return token;
}

// Verbs a caller uses for "delete", folded in queries and catalog text alike, so
// "remove node" finds delete_node while remove_* actions still match "remove".
// Mirrors FoldSynonym in McpNativeGatewaySearchMatch.cpp.
const foldSynonym = (token: string): string =>
  token === 'remove' || token === 'destroy' || token === 'erase' ? 'delete' : token;

export function tokenizeCapabilityText(value: string): readonly string[] {
  const expanded = RETRIEVAL_TOKENIZATION.splitCamelCase
    ? value.replace(CAMEL_CASE_BOUNDARY, '$1 $2')
    : value;
  const matches = expanded.toLowerCase().match(ASCII_TOKEN_PATTERN);
  if (matches === null) return [];
  const bounded = matches.map((token) => token.slice(0, RETRIEVAL_TOKENIZATION.maxTokenLength));
  return (RETRIEVAL_TOKENIZATION.foldInflections ? bounded.map(foldInflection) : bounded).map(foldSynonym);
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

/**
 * A query's unique tokens, at most maxTokens of them. Only the query is capped: record text is
 * read whole, as the native door reads it; the cap once cut every summary past 48 tokens, so the
 * tail of set_widget_layout's (font face, letter spacing, button sounds) never reached search.
 */
export function queryCapabilityTokens(query: string): readonly string[] {
  return uniqueCapabilityTokens(query).slice(0, RETRIEVAL_TOKENIZATION.maxTokens);
}
