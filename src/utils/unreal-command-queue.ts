import { Logger } from './logger.js';

export interface CommandQueueItem<T = unknown> {
  command: () => Promise<T>;
  resolve: (value: T) => void;
  reject: (reason?: unknown) => void;
  priority: number;
  retryCount?: number;
}

export class UnrealCommandQueue {
  private log = new Logger('UnrealCommandQueue');
  private queue: CommandQueueItem[] = [];
  private isProcessing = false;
  private lastCommandTime = 0;
  private lastStatCommandTime = 0;
  private processorInterval?: ReturnType<typeof setInterval>;

  // Config
  private readonly MIN_COMMAND_DELAY = 100;
  private readonly MAX_COMMAND_DELAY = 500;
  private readonly STAT_COMMAND_DELAY = 300;

  constructor() {
    this.startProcessor();
  }

  /**
   * Execute a command with priority-based throttling
   */
  async execute<T>(command: () => Promise<T>, priority: number = 5): Promise<T> {
    return new Promise((resolve, reject) => {
      this.queue.push({
        command: command as () => Promise<unknown>,
        resolve: resolve as (value: unknown) => void,
        reject,
        priority
      });

      // Sort by priority (lower number = higher priority)
      this.queue.sort((a, b) => a.priority - b.priority);

      // Process queue if not already processing
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

    while (this.queue.length > 0) {
      const item = this.queue.shift();
      if (!item) continue;

      // Calculate delay based on time since last command
      const timeSinceLastCommand = Date.now() - this.lastCommandTime;
      const requiredDelay = this.calculateDelay(item.priority);

      if (timeSinceLastCommand < requiredDelay) {
        await this.delay(requiredDelay - timeSinceLastCommand);
      }

      try {
        const result = await item.command();
        item.resolve(result);
      } catch (error: unknown) {
        // Enhanced retry policy
        const errObj = error as Record<string, unknown> | null;
        const msgRaw = errObj?.message ?? String(error);
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

        if (isTransient && item.retryCount < 3) {
          item.retryCount++;
          this.log.warn(`Command failed (transient), retrying (${item.retryCount}/3)`);
          this.queue.unshift({
            command: item.command,
            resolve: item.resolve,
            reject: item.reject,
            priority: Math.max(1, item.priority - 1),
            retryCount: item.retryCount
          });
          await this.delay(500);
        } else {
          if (isDeterministicFailure) {
            this.log.warn(`Command failed (non-retryable): ${msgRaw}`);
          }
          item.reject(error);
        }
      }

      this.lastCommandTime = Date.now();
    }

    this.isProcessing = false;
  }

  private calculateDelay(priority: number): number {
    if (priority <= 3) {
      return this.MAX_COMMAND_DELAY;
    } else if (priority <= 6) {
      return 200;
    } else if (priority === 8) {
      const timeSinceLastStat = Date.now() - this.lastStatCommandTime;
      if (timeSinceLastStat < this.STAT_COMMAND_DELAY) {
        return this.STAT_COMMAND_DELAY;
      }
      this.lastStatCommandTime = Date.now();
      return 150;
    } else {
      const baseDelay = this.MIN_COMMAND_DELAY;
      const jitter = Math.random() * 50;
      return baseDelay + jitter;
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
    if (this.processorInterval) {
      clearInterval(this.processorInterval);
      this.processorInterval = undefined;
    }
  }

  private delay(ms: number): Promise<void> {
    return new Promise(resolve => setTimeout(resolve, ms));
  }
}
