import type { Logger } from '../utils/logging/logger.js';
import type { CancelRequestMessage, NaturalTimeoutNotification } from './types.js';

/**
 * Delivery seam for natural-timeout terminal notifications. Best-effort sends
 * exactly one advisory `cancel_request` frame for the expired automation id.
 * Kept as a focused module so the request dispatcher stays under the pure-LOC
 * ceiling; a delivery failure is logged and never thrown into the tracker's
 * timer callback.
 */
export interface NaturalTimeoutCancelDelivery {
    readonly send: (payload: CancelRequestMessage) => boolean;
    readonly log: Logger;
}

/**
 * Best-effort send of the single advisory cancel frame. The caller must have
 * already settled correlation so an explicit-cancel race cannot emit a second
 * frame for this automation id.
 */
export function deliverNaturalTimeoutCancellation(
    notification: NaturalTimeoutNotification,
    delivery: NaturalTimeoutCancelDelivery
): void {
    try {
        delivery.send({
            type: 'cancel_request',
            requestId: notification.requestId,
            reason: `natural timeout (${notification.kind})`
        });
    } catch {
        delivery.log.warn('Failed to deliver natural-timeout cancel_request frame to Unreal', {
            requestId: notification.requestId,
            kind: notification.kind
        });
    }
}
