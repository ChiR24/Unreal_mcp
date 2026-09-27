import {
  CAPABILITY_TIMEOUT_TIER_MS,
  MIN_CAPABILITY_TIMEOUT_MS,
  UNKNOWN_CAPABILITY_TIMEOUT_MS,
  requestTimeoutOverrideMs
} from '../../../../config.js';
import { CAPABILITY_COST_INDEX } from '../../../catalog/capabilities/generated/capability-cost-index.generated.js';
import type { CapabilityCost } from '../../../catalog/capabilities/model.js';

type LatencyClass = keyof typeof CAPABILITY_TIMEOUT_TIER_MS;
type ResourceClass = keyof (typeof CAPABILITY_TIMEOUT_TIER_MS)['instant'];

const isLatencyClass = (value: string): value is LatencyClass => value in CAPABILITY_TIMEOUT_TIER_MS;

const isResourceClass = (value: string): value is ResourceClass =>
  value in CAPABILITY_TIMEOUT_TIER_MS.instant;

export function resolveCostTimeoutMs(cost: CapabilityCost): number {
  return Math.max(
    CAPABILITY_TIMEOUT_TIER_MS[cost.latency][cost.resources],
    MIN_CAPABILITY_TIMEOUT_MS
  );
}

/**
 * Request budget for one tool/action pair, derived from the cost class the
 * capability declares in its record instead of one flat number for every
 * action. An action with no record entry keeps the historical flat default, so
 * introducing tiers never shortens a budget that was never classified.
 */
// start_render blocks until Unreal's own render deadline (300000ms default; the
// gateway never forwards a shorter one) plus its 30000ms cancel wait and reply
// grace. A shorter transport budget expires first and its natural-timeout
// cancel_request stops a healthy render, so no operator pin may go below it.
export const MRQ_START_RENDER_TRANSPORT_MS = 335_000;

export function resolveActionTimeoutMs(toolName: string, action?: string): number {
  // An operator who pins a timeout has taken responsibility for it, so the pin
  // beats every derived tier.
  const budget = requestTimeoutOverrideMs() ?? tierTimeoutMs(toolName, action);
  return toolName === 'manage_sequence' && action === 'start_render'
    ? Math.max(budget, MRQ_START_RENDER_TRANSPORT_MS)
    : budget;
}

function tierTimeoutMs(toolName: string, action?: string): number {
  if (action === undefined || action.length === 0) return UNKNOWN_CAPABILITY_TIMEOUT_MS;

  const encoded = CAPABILITY_COST_INDEX[`${toolName}::${action}`];
  if (encoded === undefined) return UNKNOWN_CAPABILITY_TIMEOUT_MS;

  const [latency, resources] = encoded.split('|');
  if (latency === undefined || resources === undefined) return UNKNOWN_CAPABILITY_TIMEOUT_MS;
  if (!isLatencyClass(latency) || !isResourceClass(resources)) return UNKNOWN_CAPABILITY_TIMEOUT_MS;

  return resolveCostTimeoutMs({ latency, resources });
}
