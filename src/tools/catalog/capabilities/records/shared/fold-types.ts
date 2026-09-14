// src/tools/catalog/capabilities/records/shared/fold-types.ts
//
// The fold data shapes, shared by the fold transform (fold.ts) and the fold
// spec vocabulary (fold-spec.ts).
//
// A fold turns sibling records that differ only by one selector value into a
// single record. The primary operation carries the union of their parameters
// plus an enum-typed selector; `routing.dispatchBy` maps each value to the old
// bridge action; every old name stays callable as a folded legacy pair whose
// pins supply the value it implied. Handlers on both doors keep receiving
// exactly the actions they already implement, so no C++ or TypeScript
// handler changes for a fold.
//
// When the primary is itself one of the members, the selector is optional and
// defaults to that member's value, so every call that worked before the fold
// still works unchanged. When the primary is a new name, the selector is
// required.
//
// A fold without a selector is a pure alias fold; `aliasMembers` adds such
// aliases to a selector fold. Either way an alias keeps dispatching itself.

export type FoldSpec = {
  /** The advertised action; may name one of the members, which then keeps its pair as the primary. */
  readonly primary: string;
  /** Selector parameter added to the union schema. Omit for a pure alias fold. */
  readonly selector?: string;
  readonly summary: string;
  readonly whenToUse?: readonly string[];
  readonly whenNotToUse?: readonly string[];
  /** Caller phrasings, appended after the primary action name. */
  readonly topics?: readonly string[];
  /** selector value -> old action; or, for an alias fold, the old actions. */
  readonly members: Readonly<Record<string, string>> | readonly string[];
  /**
   * Self-dispatching aliases folded alongside the family. A plain string list
   * suits an optional (defaulted) selector; on a family whose selector is
   * REQUIRED the map form must carry the selector value each old name implied,
   * or calls by that name are refused at validation.
   */
  readonly aliasMembers?: Readonly<Record<string, string>> | readonly string[];
};

export type MemberEntry = {
  /** The advertised selector value; set only for map-form members (dispatchBy targets). */
  readonly value: string | undefined;
  readonly action: string;
  /** The selector value this name implies; set for map-form members and map-form alias members. */
  readonly pin?: string;
};
