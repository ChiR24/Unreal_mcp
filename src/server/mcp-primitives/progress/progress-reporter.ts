// src/server/mcp-primitives/progress/progress-reporter.ts
// Task 44: monotonic, bounded, active-only progress emission.
//
// A progress stream is only useful to a client if three things hold, and each
// is enforced here rather than trusted of the upstream source (Unreal decides
// how chatty it is, and it can report the same percent repeatedly or regress):
//
//   monotonic   - `progress` strictly increases, so a client never renders
//                 progress going backwards
//   bounded     - at most `max` notifications per operation, so a pathological
//                 source cannot flood the JSON-RPC channel
//   active-only - nothing is emitted once `close()` ran, so a late frame can
//                 never trail the terminal result of the same request
//
// Delivery is SERIALIZED on an internal promise chain. Emitting without
// chaining would let two transport writes resolve out of order, showing the
// client a lower value after a higher one even though each was individually
// monotonic. `drain()` awaits that chain, which is what lets tests assert
// ordering deterministically instead of sleeping.

import type { ProgressToken } from './progress-token.js';

/** Default per-operation notification ceiling. */
export const MAX_PROGRESS_NOTIFICATIONS = 64;

/** Upper bound on the forwarded status string, so a huge message cannot flood. */
export const MAX_PROGRESS_MESSAGE_LENGTH = 512;

/** The exact `notifications/progress` shape handed to the transport. */
export interface ProgressNotification {
  readonly method: 'notifications/progress';
  readonly params: {
    /** The CLIENT's token, verbatim — never a server-side id. */
    readonly progressToken: ProgressToken;
    readonly progress: number;
    readonly total?: number;
    readonly message?: string;
  };
}

/** One progress observation from the upstream source. */
export interface ProgressUpdate {
  readonly progress: number;
  readonly total?: number;
  readonly message?: string;
}

export type ProgressNotifier = (notification: ProgressNotification) => Promise<void>;

export interface ProgressReporter {
  /** Record an observation. Never throws and never blocks the caller. */
  report(update: ProgressUpdate): void;
  /** Terminal transition. Idempotent; after it, `report` is a no-op. */
  close(): void;
  /** Resolves once every accepted notification has been handed to the transport. */
  drain(): Promise<void>;
  /** How many notifications were actually emitted. */
  readonly sent: number;
  /** False once `close()` ran. */
  readonly active: boolean;
}

export interface CreateProgressReporterOptions {
  /** The client's token. `undefined` means the client asked for no progress. */
  readonly token: ProgressToken | undefined;
  readonly notify: ProgressNotifier;
  readonly max?: number;
  readonly onError?: (error: unknown) => void;
}

/**
 * Build a reporter for one in-flight operation.
 *
 * When `token` is `undefined` the reporter is INERT: it emits nothing at all
 * rather than inventing a token so it has something to send. Silence is the
 * correct answer for a client that did not ask for progress.
 */
export function createProgressReporter(options: CreateProgressReporterOptions): ProgressReporter {
  const { token, notify, onError } = options;
  const max = options.max ?? MAX_PROGRESS_NOTIFICATIONS;

  let lastProgress = Number.NEGATIVE_INFINITY;
  let sent = 0;
  let active = true;
  let chain: Promise<void> = Promise.resolve();

  const accepts = (update: ProgressUpdate): boolean =>
    active &&
    sent < max &&
    Number.isFinite(update.progress) &&
    update.progress > lastProgress;

  const buildParams = (
    activeToken: ProgressToken,
    update: ProgressUpdate,
  ): ProgressNotification['params'] => ({
    progressToken: activeToken,
    progress: update.progress,
    ...(Number.isFinite(update.total) ? { total: update.total } : {}),
    ...(typeof update.message === 'string' && update.message.length > 0
      ? { message: update.message.slice(0, MAX_PROGRESS_MESSAGE_LENGTH) }
      : {}),
  });

  return {
    report(update: ProgressUpdate): void {
      // A client that sent no token gets silence, not an invented token.
      if (token === undefined || !accepts(update)) return;

      lastProgress = update.progress;
      sent += 1;
      const notification: ProgressNotification = {
        method: 'notifications/progress',
        params: buildParams(token, update),
      };

      // Chained so delivery order equals report order. A failure is absorbed
      // into the chain (never rethrown at the caller, never left unhandled) so
      // a dead transport cannot break the operation that is reporting.
      chain = chain.then(
        () =>
          notify(notification).then(
            () => undefined,
            (error: unknown) => {
              onError?.(error);
            },
          ),
        () => undefined,
      );
    },

    close(): void {
      active = false;
    },

    drain(): Promise<void> {
      return chain;
    },

    get sent(): number {
      return sent;
    },

    get active(): boolean {
      return active;
    },
  };
}
