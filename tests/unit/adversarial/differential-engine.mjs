// @ts-check
// tests/unit/adversarial/differential-engine.mjs
// Task 51 — the generic accept/reject differential, and the rule that keeps its
// allowlist honest.
//
// A differential test between two implementations is only as good as what it does
// with a disagreement. The failure mode is well known and it is not subtle: the
// suite is run, it reports 40 divergences, and the divergences are copied into an
// "expected differences" list until the suite is green. That list is not an
// allowlist — it is a transcript of the bugs, laundered into a specification. It
// makes the suite permanently unable to detect the thing it was built for.
//
// SO AN EXCUSE HERE IS SEMANTIC, NEVER EMPIRICAL. An allowlist entry does not name
// an input, a seed, or a count. It names:
//
//   * the RULE IDS on each side that produce the divergence (which must exist —
//     `validateAllowlist` refuses an entry citing a rule the policy does not have,
//     so an excuse cannot be invented for an unexplained result),
//   * the DIRECTION it is allowed in, and
//   * a written security argument for why that direction is safe.
//
// Four consequences, each enforced below rather than documented and hoped for:
//
//   1. An UNATTRIBUTABLE divergence — one `explain` cannot map to a rule pair — is
//      ALWAYS a finding. There is no "other" bucket to grow.
//   2. A divergence whose observed direction is not the class's declared direction
//      is a finding, even though the class exists. The excuse is for one direction.
//   3. An allowlist entry that excused nothing in a run is reported as UNUSED, so a
//      stale excuse is deleted rather than silently protecting future regressions.
//   4. `native-laxer` classes must additionally carry `defenceInDepthOnly`, because
//      the plugin is the sole authorization authority: TypeScript being stricter is
//      a redundancy, the plugin being laxer is the actual attack surface.

/** @typedef {'accept'|'reject'|'undecidable'} Verdict */
/** @typedef {{ verdict: Verdict, reason: string, rule?: string|null }} SideResult */

/**
 * @typedef {{
 *   id: string,
 *   leftRules: readonly string[],
 *   rightReasons: readonly string[],
 *   direction: 'left-stricter'|'right-stricter',
 *   rationale: string,
 *   securityArgument: string,
 *   defenceInDepthOnly?: boolean,
 * }} AsymmetryClass
 */

const MIN_ARGUMENT_CHARS = 40;

/**
 * Refuse an allowlist that could be used to launder an unexplained divergence.
 *
 * `knownLeftRules` is the real rule inventory of the left implementation. An entry
 * citing a rule id that does not exist is rejected: that is precisely the shape an
 * excuse takes when it was written to silence a result rather than to describe a
 * design decision.
 * @param {readonly AsymmetryClass[]} allowlist
 * @param {readonly string[]} knownLeftRules
 * @param {readonly string[]} knownRightReasons
 * @returns {{ ok: boolean, problems: string[] }}
 */
export function validateAllowlist(allowlist, knownLeftRules, knownRightReasons) {
  /** @type {string[]} */
  const problems = [];
  /** @type {Set<string>} */
  const seen = new Set();
  for (const entry of allowlist) {
    const at = `allowlist[${entry.id}]`;
    if (seen.has(entry.id)) problems.push(`${at}: duplicate class id`);
    seen.add(entry.id);
    if (entry.leftRules.length === 0 && entry.rightReasons.length === 0) {
      problems.push(`${at}: cites no rule on either side, so it describes an outcome rather than a design decision`);
    }
    for (const rule of entry.leftRules) {
      if (!knownLeftRules.includes(rule)) {
        problems.push(`${at}: cites left rule "${rule}", which the policy does not define; an excuse for a rule that does not exist is an excuse written to silence a result`);
      }
    }
    for (const reason of entry.rightReasons) {
      if (!knownRightReasons.includes(reason)) {
        problems.push(`${at}: cites right reason "${reason}", which the implementation cannot emit`);
      }
    }
    if (entry.rationale.trim().length < MIN_ARGUMENT_CHARS) {
      problems.push(`${at}: rationale is too short to be a reviewed judgement (${entry.rationale.trim().length} < ${MIN_ARGUMENT_CHARS} chars)`);
    }
    if (entry.securityArgument.trim().length < MIN_ARGUMENT_CHARS) {
      problems.push(`${at}: securityArgument is too short to be a reviewed judgement`);
    }
    if (entry.direction === 'right-stricter' && entry.defenceInDepthOnly !== true) {
      problems.push(`${at}: a right-stricter class means the LEFT surface accepts what the right refuses; it must be explicitly marked defenceInDepthOnly with the reason that the right side re-enforces`);
    }
  }
  return { ok: problems.length === 0, problems };
}

/**
 * Compare two implementations over a case list.
 *
 * `explain` returns the class id it attributes a divergence to, or null. Returning
 * null is the honest answer and it produces a finding; there is deliberately no way
 * for `explain` to mark something excused on its own.
 *
 * @template T
 * @param {{
 *   name: string,
 *   cases: readonly { input: T, label: string, index: number }[],
 *   left: (input: T) => SideResult,
 *   right: (input: T) => SideResult,
 *   explain: (input: T, left: SideResult, right: SideResult) => string|null,
 *   allowlist: readonly AsymmetryClass[],
 * }} spec
 */
export function runDifferential(spec) {
  const byId = new Map(spec.allowlist.map((entry) => [entry.id, entry]));
  /** @type {Map<string, number>} */
  const excusedByClass = new Map();
  /** @type {Array<Record<string, unknown>>} */
  const findings = [];
  let agreed = 0;
  let undecidable = 0;

  for (const testCase of spec.cases) {
    const left = spec.left(testCase.input);
    const right = spec.right(testCase.input);

    if (left.verdict === 'undecidable' || right.verdict === 'undecidable') {
      undecidable += 1;
      continue;
    }
    if (left.verdict === right.verdict) {
      agreed += 1;
      continue;
    }

    const direction = left.verdict === 'reject' ? 'left-stricter' : 'right-stricter';
    const classId = spec.explain(testCase.input, left, right);
    const excuse = classId === null ? undefined : byId.get(classId);

    if (excuse !== undefined && excuse.direction === direction) {
      excusedByClass.set(excuse.id, (excusedByClass.get(excuse.id) ?? 0) + 1);
      continue;
    }
    findings.push({
      differential: spec.name,
      index: testCase.index,
      label: testCase.label,
      input: testCase.input,
      left,
      right,
      direction,
      attributedClass: classId,
      why: classId === null
        ? 'the divergence could not be attributed to any declared rule pair; an unattributable disagreement is always a finding'
        : excuse === undefined
          ? `attributed to "${classId}", which is not a declared asymmetry class`
          : `attributed to "${classId}", which is declared only for ${excuse.direction} but was observed ${direction}`,
    });
  }

  const unusedClasses = spec.allowlist
    .filter((entry) => (excusedByClass.get(entry.id) ?? 0) === 0)
    .map((entry) => entry.id);

  return {
    name: spec.name,
    total: spec.cases.length,
    agreed,
    undecidable,
    excused: [...excusedByClass].map(([id, count]) => ({ id, count })),
    unusedClasses,
    findings,
    ok: findings.length === 0,
  };
}
