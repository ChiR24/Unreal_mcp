import {
  CAPABILITY_TIMEOUT_TIER_MS,
  MIN_CAPABILITY_TIMEOUT_MS,
  UNKNOWN_CAPABILITY_TIMEOUT_MS,
  stringToPositiveInteger
} from '../../../../config.js';
import { CAPABILITY_COST_INDEX } from '../../../catalog/capabilities/generated/capability-cost-index.generated.js';
import type { CapabilityCost } from '../../../catalog/capabilities/model.js';

export function getTimeoutMs(defaultMs: number = 120000): number {
  const raw = process.env.MCP_REQUEST_TIMEOUT_MS ?? process.env.MCP_AUTOMATION_REQUEST_TIMEOUT_MS;
  return stringToPositiveInteger(raw, defaultMs);
}

type LatencyClass = keyof typeof CAPABILITY_TIMEOUT_TIER_MS;
type ResourceClass = keyof (typeof CAPABILITY_TIMEOUT_TIER_MS)['instant'];

const isLatencyClass = (value: string): value is LatencyClass => value in CAPABILITY_TIMEOUT_TIER_MS;

const isResourceClass = (value: string): value is ResourceClass =>
  value in CAPABILITY_TIMEOUT_TIER_MS.instant;

// An operator who pins a timeout has taken responsibility for it, so the pin
// beats every derived tier. An unparseable value is treated as absent rather
// than as zero, which would expire every request immediately.
function envTimeoutOverrideMs(): number | undefined {
  const raw = process.env.MCP_REQUEST_TIMEOUT_MS ?? process.env.MCP_AUTOMATION_REQUEST_TIMEOUT_MS;
  if (raw === undefined) return undefined;
  const parsed = stringToPositiveInteger(raw, 0);
  return parsed > 0 ? parsed : undefined;
}

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
export function resolveActionTimeoutMs(toolName: string, action?: string): number {
  const override = envTimeoutOverrideMs();
  if (override !== undefined) return override;
  if (action === undefined || action.length === 0) return UNKNOWN_CAPABILITY_TIMEOUT_MS;

  const encoded = CAPABILITY_COST_INDEX[`${toolName}::${action}`];
  if (encoded === undefined) return UNKNOWN_CAPABILITY_TIMEOUT_MS;

  const [latency, resources] = encoded.split('|');
  if (latency === undefined || resources === undefined) return UNKNOWN_CAPABILITY_TIMEOUT_MS;
  if (!isLatencyClass(latency) || !isResourceClass(resources)) return UNKNOWN_CAPABILITY_TIMEOUT_MS;

  return resolveCostTimeoutMs({ latency, resources });
}
