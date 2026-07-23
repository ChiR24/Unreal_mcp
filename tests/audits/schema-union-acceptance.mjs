/**
 * Acceptance semantics for `oneOf`/`anyOf` nodes in the schema parity audit.
 *
 * The C++ schema builder cannot encode a JSON-Schema union, so the generator
 * projects a scalar union down to a plain type list (`TypeUnion`). That
 * projection is only honest when both sides accept exactly the same values.
 *
 * A branch constraining nothing but its type (`{type:'string'}`) accepts EVERY
 * string, so it subsumes any sibling branch restricted to the same type
 * (`{type:'string', enum:[...]}`). When every constrained branch is covered by
 * such an open branch, the union accepts precisely the open type set and the
 * projection loses nothing.
 *
 * Two shapes are deliberately NOT lossless and stay hard failures: a union with
 * no open branch (projecting it would WIDEN what the native side accepts), and
 * a branch carrying `properties`/`items`/a nested union (structural, so a flat
 * type list would drop the structure).
 */

const SCALAR_TYPES = new Set(['string', 'number', 'integer', 'boolean', 'null']);

const branchTypes = (branch) => {
  const type = branch.type;
  if (typeof type === 'string') return [type];
  if (Array.isArray(type)) return type.map(String);
  return [];
};

const isScalarBranch = (branch) => {
  const types = branchTypes(branch);
  if (types.length === 0 || !types.every((type) => SCALAR_TYPES.has(type))) return false;
  return branch.properties === undefined
    && branch.items === undefined
    && branch.oneOf === undefined
    && branch.anyOf === undefined;
};

const unionBranches = (node) => [
  ...(Array.isArray(node.oneOf) ? node.oneOf : []),
  ...(Array.isArray(node.anyOf) ? node.anyOf : []),
];

/**
 * The sorted type list a union accepts, or `undefined` when projecting it to a
 * type list would change what is accepted.
 */
export function losslessUnionTypes(node) {
  const branches = unionBranches(node);
  if (branches.length === 0 || !branches.every(isScalarBranch)) return undefined;

  const open = new Set();
  const constrained = new Set();
  for (const branch of branches) {
    const target = branch.enum === undefined ? open : constrained;
    for (const type of branchTypes(branch)) target.add(type);
  }

  if (open.size === 0) return undefined;
  for (const type of constrained) {
    if (!open.has(type)) return undefined;
  }
  return [...open].sort();
}
