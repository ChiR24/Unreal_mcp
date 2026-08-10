import { RETRIEVAL_PARITY_VECTOR_SCHEMA } from './constants.js';
import { parseCapabilityRetrievalRequest } from './request.js';
import type {
  CapabilityRetrievalParityVector,
  CapabilityRetrievalResult,
} from './types.js';

export type CapabilityParityVectorInput = {
  readonly name: string;
  readonly request: unknown;
  readonly result: CapabilityRetrievalResult;
};

export function createCapabilityRetrievalParityVector(
  input: CapabilityParityVectorInput,
): CapabilityRetrievalParityVector {
  const request = parseCapabilityRetrievalRequest(input.request);
  return Object.freeze({
    schema: RETRIEVAL_PARITY_VECTOR_SCHEMA,
    name: input.name,
    request,
    expected: Object.freeze({
      rankedCapabilityIds: Object.freeze(input.result.matches.map((match) => match.id)),
      nearTieCapabilityIds: Object.freeze([...input.result.nearTieCapabilityIds]),
      selection: input.result.selection,
    }),
  });
}
