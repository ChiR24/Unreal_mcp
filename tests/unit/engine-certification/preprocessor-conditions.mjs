// @ts-check
// tests/unit/engine-certification/preprocessor-conditions.mjs
// Task 52 — a tri-state evaluator for the plugin's real `#if` conditions.
//
// The offline profile adapter must answer "is this feature compiled in on 5.3?"
// with no compiler and no engine. It could do that from a hand-written table of
// thresholds — and that table would be a SECOND copy of the gating rules, which
// keeps answering with the old rule the day a domain file moves a gate. So the
// conditions are read from the sources and evaluated here instead.
//
// TRI-STATE, NOT BOOLEAN. The plugin's conditions mix two different kinds of
// fact: the engine version (which a profile knows) and build defines like
// MCP_HAS_CONTROLRIG_FACTORY (which depend on what modules UBT found). A profile
// that does not state the define cannot decide such a gate, and `false` would
// under-report the feature exactly as confidently as `true` would over-report it.
// `null` means undecidable and every consumer has to handle it.
//
// Short-circuiting still decides what it can: `false && unknown` is false and
// `true || unknown` is true, because in those cases the unknown is not
// load-bearing. That is the difference between a careful "I don't know" and a
// lazy one.

const TOKEN = /\s*(\|\||&&|[!<>=]=|[<>!()]|[A-Za-z_]\w*|\d+)/y;

/** @param {string} text @returns {string[]|null} */
function tokenize(text) {
  /** @type {string[]} */
  const tokens = [];
  TOKEN.lastIndex = 0;
  while (TOKEN.lastIndex < text.length) {
    const start = TOKEN.lastIndex;
    const match = TOKEN.exec(text);
    if (match === null || TOKEN.lastIndex === start) {
      // Anything the plugin does not actually use — a ternary, arithmetic, a
      // `defined(...)` call — is refused whole rather than parsed halfway.
      return text.slice(start).trim().length === 0 ? tokens : null;
    }
    tokens.push(/** @type {string} */ (match[1]));
  }
  return tokens;
}

/**
 * @typedef {{ kind: 'number', value: number }
 *   | { kind: 'identifier', name: string }
 *   | { kind: 'compare', op: string, left: Node, right: Node }
 *   | { kind: 'not', operand: Node }
 *   | { kind: 'and', left: Node, right: Node }
 *   | { kind: 'or', left: Node, right: Node }} Node
 */

/**
 * Recursive-descent parse of the `||`/`&&`/`!`/comparison grammar the plugin uses.
 * Returns null for anything outside it; a partially-understood condition is worse
 * than an unparsed one, because it evaluates confidently and wrongly.
 * @param {string} text @returns {Node|null}
 */
export function parseCondition(text) {
  const tokens = tokenize(text);
  if (tokens === null) return null;
  let at = 0;
  const peek = () => tokens[at];
  const take = () => tokens[at++];

  /** @returns {Node|null} */
  const primary = () => {
    const token = take();
    if (token === undefined) return null;
    if (token === '(') {
      const inner = disjunction();
      if (inner === null || take() !== ')') return null;
      return inner;
    }
    if (token === '!') {
      const operand = primary();
      return operand === null ? null : { kind: 'not', operand };
    }
    if (/^\d+$/u.test(token)) return { kind: 'number', value: Number(token) };
    if (/^[A-Za-z_]\w*$/u.test(token)) return { kind: 'identifier', name: token };
    return null;
  };

  /** @returns {Node|null} */
  const comparison = () => {
    const left = primary();
    if (left === null) return null;
    const op = peek();
    if (op === undefined || !['==', '!=', '>=', '<=', '>', '<'].includes(op)) return left;
    take();
    const right = primary();
    return right === null ? null : { kind: 'compare', op, left, right };
  };

  /** @returns {Node|null} */
  const conjunction = () => {
    let left = comparison();
    if (left === null) return null;
    while (peek() === '&&') {
      take();
      const right = comparison();
      if (right === null) return null;
      left = { kind: 'and', left, right };
    }
    return left;
  };

  /** @returns {Node|null} */
  function disjunction() {
    let left = conjunction();
    if (left === null) return null;
    while (peek() === '||') {
      take();
      const right = conjunction();
      if (right === null) return null;
      left = { kind: 'or', left, right };
    }
    return left;
  }

  const parsed = disjunction();
  return parsed !== null && at === tokens.length ? parsed : null;
}

/** @param {number|null} value */
const truthy = (value) => (value === null ? null : value !== 0);

/**
 * @param {Node} node @param {Record<string, number>} defines
 * @returns {number|null} the numeric value, or null when undecidable
 */
function evaluateNode(node, defines) {
  if (node.kind === 'number') return node.value;
  if (node.kind === 'identifier') {
    // An identifier nobody defined is NOT zero here. In a real preprocessor it
    // would be, but a profile that simply omitted MCP_HAS_PCG has not asserted
    // that PCG is absent — it has said nothing, and this evaluator refuses to
    // turn silence into a claim.
    return Object.hasOwn(defines, node.name) ? defines[node.name] : null;
  }
  if (node.kind === 'not') {
    const operand = truthy(evaluateNode(node.operand, defines));
    return operand === null ? null : operand ? 0 : 1;
  }
  if (node.kind === 'compare') {
    const left = evaluateNode(node.left, defines);
    const right = evaluateNode(node.right, defines);
    if (left === null || right === null) return null;
    switch (node.op) {
      case '==': return left === right ? 1 : 0;
      case '!=': return left !== right ? 1 : 0;
      case '>=': return left >= right ? 1 : 0;
      case '<=': return left <= right ? 1 : 0;
      case '>': return left > right ? 1 : 0;
      default: return left < right ? 1 : 0;
    }
  }
  const left = truthy(evaluateNode(node.left, defines));
  if (node.kind === 'and') {
    if (left === false) return 0;
    const right = truthy(evaluateNode(node.right, defines));
    if (right === false) return 0;
    return left === true && right === true ? 1 : null;
  }
  if (left === true) return 1;
  const right = truthy(evaluateNode(node.right, defines));
  if (right === true) return 1;
  return left === false && right === false ? 0 : null;
}

/**
 * @param {string} condition @param {Record<string, number>} defines
 * @returns {boolean|null} null when the condition depends on something unstated
 */
export function evaluateCondition(condition, defines) {
  const parsed = parseCondition(condition);
  if (parsed === null) return null;
  return truthy(evaluateNode(parsed, defines));
}

/** Directives this walker understands. `#elif` is included because the header uses it. */
const DIRECTIVE = /^\s*#\s*(if|ifdef|ifndef|elif|else|endif|define)\b\s*(.*)$/u;

/**
 * Walk a C++ header's conditional structure and resolve every `#define NAME <int>`
 * that a given engine + build configuration would actually see.
 *
 * This is not a preprocessor. It handles exactly the shapes McpVersionCompatibility.h
 * uses — nested if/elif/else/endif over integer defines — and it REPORTS every
 * condition it could not decide instead of picking a branch.
 * @param {string} text @param {Record<string, number>} defines
 */
export function evaluateCompatibilityMacros(text, defines) {
  /** @type {Record<string, number>} */
  const macros = { ...defines };
  /** @type {Array<{ condition: string, line: number }>} */
  const undecided = [];
  /** Each frame: active = this branch is being compiled; decided = some branch already won. */
  /** @type {Array<{ active: boolean|null, decided: boolean, condition: string }>} */
  const stack = [];
  const inside = () => stack.every((frame) => frame.active === true);
  const blocked = () => stack.some((frame) => frame.active === false);

  const lines = text.split('\n');
  for (const [index, line] of lines.entries()) {
    const directive = DIRECTIVE.exec(line);
    if (directive === null) continue;
    const keyword = String(directive[1]);
    const rest = String(directive[2] ?? '');
    const condition = rest.split('//')[0]?.trim() ?? '';

    if (keyword === 'if' || keyword === 'ifdef' || keyword === 'ifndef') {
      const name = condition.split(/\s+/u)[0] ?? '';
      const verdict = blocked() ? false
        : keyword === 'ifdef' ? Object.hasOwn(macros, name)
          : keyword === 'ifndef' ? !Object.hasOwn(macros, name)
            : evaluateCondition(condition, macros);
      if (verdict === null) undecided.push({ condition, line: index + 1 });
      stack.push({ active: verdict, decided: verdict === true, condition });
      continue;
    }
    const frame = stack.at(-1);
    if (keyword === 'endif') {
      stack.pop();
      continue;
    }
    // `#elif`/`#else` need an open frame; a `#define` at file scope does not, and
    // an early `continue` here is what silently dropped every top-level macro.
    if (frame !== undefined && keyword === 'elif') {
      const verdict = frame.decided || blocked() ? false : evaluateCondition(condition, macros);
      if (verdict === null) undecided.push({ condition, line: index + 1 });
      frame.active = verdict;
      frame.decided = frame.decided || verdict === true;
      continue;
    }
    if (frame !== undefined && keyword === 'else') {
      frame.active = frame.active === null ? null : !frame.decided;
      continue;
    }
    // `#define NAME <int>` inside a branch that is being compiled.
    if (!inside()) continue;
    const assignment = /^([A-Za-z_]\w*)\s+(-?\d+)\s*$/u.exec(condition);
    if (assignment !== null) macros[/** @type {string} */ (assignment[1])] = Number(assignment[2]);
  }
  return { macros, undecided, unbalanced: stack.length };
}
