import { Logger } from '../logging/logger.js';

/**
 * Whether re-running a command after a transient failure is safe.
 *
 * A client-side timeout or dropped connection never proves the editor skipped
 * the work, so re-running is only safe when repeating it cannot double a side
 * effect: `safe-read` (side-effect free) or `idempotency-protected` (the callee
 * collapses the replay). `never` is the default for everything else.
 */
export type CommandRetryPolicy = 'never' | 'safe-read' | 'idempotency-protected';

export interface CommandExecuteOptions {
  readonly retryPolicy?: CommandRetryPolicy;
}

export interface CommandQueueItem<T = unknown> {
  command: () => Promise<T>;
  resolve: (value: T) => void;
  reject: (reason?: unknown) => void;
  priority: number;
  retryPolicy: CommandRetryPolicy;
  retryCount?: number;
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
  private readonly MAX_RECOVERY_ATTEMPTS = 1;
  private readonly RECOVERY_DELAY_MS = 500;

  constructor() {
    this.startProcessor();
  }

  /**
   * Execute a command with priority-based throttling
   */
  async execute<T>(
    command: () => Promise<T>,
    priority: number = 5,
    options: CommandExecuteOptions = {}
  ): Promise<T> {
    if (this.stopped) {
      throw new UnrealCommandQueueStoppedError();
    }

    return new Promise((resolve, reject) => {
      const item: CommandQueueItem = {
        command: command as () => Promise<unknown>,
        resolve: resolve as (value: unknown) => void,
        reject,
        priority,
        retryPolicy: options.retryPolicy ?? 'never'
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
          await this.delay(requiredDelay - timeSinceLastCommand);
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

          const msgRaw = error instanceof Error ? error.message : String(error);
          const msg = String(msgRaw).toLowerCase();
          if (item.retryCount === undefined) item.retryCount = 0;

          const isTransient = (
            msg.includes('timeout') ||
            msg.includes('timed out') ||
            msg.includes('connect') ||
            msg.includes('econnrefused') ||
            msg.includes('econnreset') ||
            msg.includes('broken pipe') ||
            msg.includes('automation bridge') ||
            msg.includes('not connected')
          );

          const isDeterministicFailure = (
            msg.includes('command not executed') ||
            msg.includes('exec_failed') ||
            msg.includes('invalid command') ||
            msg.includes('invalid argument') ||
            msg.includes('unknown_plugin_action') ||
            msg.includes('unknown action')
          );

          if (isTransient && this.mayRecover(item)) {
            item.retryCount++;
            this.log.warn(
              `Command failed (transient), recovering ${item.retryPolicy} work (${item.retryCount}/${this.MAX_RECOVERY_ATTEMPTS})`
            );
            this.queue.unshift({
              ...item,
              priority: Math.max(1, item.priority - 1)
            });
            await this.delay(this.RECOVERY_DELAY_MS);
          } else {
            if (isTransient) {
              this.log.warn(
                `Command failed (transient) and is not retry-safe (retryPolicy=${item.retryPolicy}); refusing to re-run`
              );
            } else if (isDeterministicFailure) {
              this.log.warn(`Command failed (non-retryable): ${msgRaw}`);
            }
            item.reject(error);
          }
        }

        this.lastCommandTime = Date.now();
      }
    } finally {
      this.isProcessing = false;
    }
  }

  private mayRecover(item: CommandQueueItem): boolean {
    if (item.retryPolicy === 'never') {
      return false;
    }
    return (item.retryCount ?? 0) < this.MAX_RECOVERY_ATTEMPTS;
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

  private delay(ms: number): Promise<void> {
    return new Promise(resolve => setTimeout(resolve, ms));
  }
}
