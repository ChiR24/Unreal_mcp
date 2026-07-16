/**
 * Deliberately naive, INCORRECT classifier used only to prove the inventory's
 * correctness. It collapses purely on action-name equality and ignores the
 * target/domain namespace, which would wrongly merge `asset.delete`,
 * `actor.delete`, `level.delete`, and `sequence.delete` into one capability.
 * The inventory must NOT do this; tests/audit assert the naive result differs.
 */
export function naiveNameOnlyCanonicalId(_tool: string, action: string): string {
  return `cap:name:${action}`;
}
