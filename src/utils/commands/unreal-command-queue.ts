import { Logger } from '../logging/logger.js';
import { setTimeout as delay } from 'node:timers/promises';

interface CommandQueueItem<T = unknown> {
  command: () => Promise<T>;
  resolve: (value: T) => void;
  reject: (reason?: unknown) => void;
  priority: number;
}

export class UnrealCommandQueueStoppedError extends Error {
  constructor() {
    super('Unreal command queue stopped');
    this.name = 'UnrealCommandQueueStoppedError';
  }
}

export class UnrealCommandQueue {
  private log = new Logger('UnrealCommandQueue');
  private queue: CommandQueueItem[] = [];
  private isProcessing = false;
  private lastCommandTime = 0;
  private lastStatCommandTime = 0;
  private processorInterval?: ReturnType<typeof setInterval>;
  private stopped = false;

  // Config
  private readonly MIN_COMMAND_DELAY = 100;
  private readonly MAX_COMMAND_DELAY = 500;
  private readonly STAT_COMMAND_DELAY = 300;

  constructor() {
    this.startProcessor();
  }

  /**
   * Execute a command with priority-based throttling. A failed command is never
   * re-run: a client-side timeout or dropped connection does not prove the editor
   * skipped the work, so replaying it could double a side effect.
   */
  async execute<T>(command: () => Promise<T>, priority: number = 5): Promise<T> {
    if (this.stopped) {
      throw new UnrealCommandQueueStoppedError();
    }

    return new Promise((resolve, reject) => {
      const item: CommandQueueItem = {
        command: command as () => Promise<unknown>,
        resolve: resolve as (value: unknown) => void,
        reject,
        priority
      };

      this.enqueue(item);

      if (!this.isProcessing) {
        this.processQueue();
      }
    });
  }

  private async processQueue(): Promise<void> {
    if (this.isProcessing || this.queue.length === 0) {
      return;
    }

    this.isProcessing = true;

    try {
      while (this.queue.length > 0) {
        const item = this.queue.shift();
        if (!item) continue;

        // Calculate delay based on time since last command
        const timeSinceLastCommand = Date.now() - this.lastCommandTime;
        const requiredDelay = this.calculateDelay(item.priority);

        if (timeSinceLastCommand < requiredDelay) {
          await delay(requiredDelay - timeSinceLastCommand);
        }

        if (this.stopped) {
          item.reject(new UnrealCommandQueueStoppedError());
          continue;
        }

        this.recordCommandStart(item.priority);

        try {
          const result = await item.command();
          item.resolve(result);
        } catch (error: unknown) {
          if (this.stopped) {
            item.reject(new UnrealCommandQueueStoppedError());
            continue;
          }

          // The classification steers the log line only: a failed command is
          // never re-run either way (see execute()'s contract). An unmatched
          // message used to log nothing at all, hiding the failure entirely.
          const msgRaw = error instanceof Error ? error.message : String(error);
          const msg = msgRaw.toLowerCase();
          const TRANSIENT = ['timeout', 'timed out', 'connect', 'econnrefused', 'econnreset',
            'broken pipe', 'automation bridge', 'not connected'];
          const DETERMINISTIC = ['command not executed', 'exec_failed', 'invalid command',
            'invalid argument', 'unknown_plugin_action', 'unknown action'];

          const kind = TRANSIENT.some(t => msg.includes(t))
            ? 'transient; refusing to re-run'
            : DETERMINISTIC.some(t => msg.includes(t))
              ? 'non-retryable'
              : 'unclassified';
          this.log.warn(`Command failed (${kind}): ${msgRaw}`);
          item.reject(error);
        }

        this.lastCommandTime = Date.now();
      }
    } finally {
      this.isProcessing = false;
    }
  }

  private enqueue(item: CommandQueueItem): void {
    let low = 0;
    let high = this.queue.length;
    while (low < high) {
      const mid = Math.floor((low + high) / 2);
      if (this.queue[mid].priority <= item.priority) {
        low = mid + 1;
      } else {
        high = mid;
      }
    }
    this.queue.splice(low, 0, item);
  }

  private calculateDelay(priority: number): number {
    if (priority <= 3) {
      return this.MAX_COMMAND_DELAY;
    } else if (priority <= 6) {
      return 200;
    } else if (priority === 8) {
      if (this.lastStatCommandTime === 0) {
        return 150;
      }

      const timeSinceLastStat = Date.now() - this.lastStatCommandTime;
      if (timeSinceLastStat < this.STAT_COMMAND_DELAY) {
        return this.STAT_COMMAND_DELAY;
      }
      return 150;
    } else {
      const baseDelay = this.MIN_COMMAND_DELAY;
      const jitter = Math.random() * 50;
      return baseDelay + jitter;
    }
  }

  private recordCommandStart(priority: number): void {
    if (priority === 8) {
      this.lastStatCommandTime = Date.now();
    }
  }

  private startProcessor(): void {
    // Fallback processor - primary processing is triggered directly from execute()
    // Reduced from 1000ms to 250ms for faster recovery if processor stalls
    this.processorInterval = setInterval(() => {
      if (!this.isProcessing && this.queue.length > 0) {
        this.processQueue();
      }
    }, 250);
  }

  /**
   * Stop the command queue processor and clean up the interval.
   * Should be called during shutdown to allow clean process exit.
   */
  stopProcessor(): void {
    this.stopped = true;
    if (this.processorInterval) {
      clearInterval(this.processorInterval);
      this.processorInterval = undefined;
    }

    const stoppedError = new UnrealCommandQueueStoppedError();
    for (const item of this.queue.splice(0)) {
      item.reject(stoppedError);
    }
  }
}
