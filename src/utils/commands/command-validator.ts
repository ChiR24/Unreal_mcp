/**
 * Validates console commands before execution to prevent dangerous operations.
 *
 * The authoritative block lists are generated from the canonical typed policy
 * (src/utils/commands/console-command-policy-rules.ts) by
 * scripts/generate-console-command-policy.ts into
 * console-command-policy.generated.ts (Task 22). This module no longer
 * hand-maintains duplicated rule arrays; it delegates to the shared generated
 * policy so TypeScript and native transports stay in lockstep.
 *
 * Runtime behavior (blocked commands, tokens, separators, Python) is unchanged.
 */
import {
  applyGeneratedConsoleCommandPolicy,
} from './console-command-policy-generated.js';

export class CommandValidator {
    /**
     * Patterns that indicate obviously invalid commands.
     * Used to warn about likely typos or invalid input.
     */
    private static readonly INVALID_PATTERNS = [
        /^\d+$/,  // Just numbers
        /^invalid_command/i,
        /^this_is_not_a_valid/i,
    ];

    /**
     * Validates a console command for safety before execution.
     * @param command - The console command string to validate
     * @throws Error if the command is dangerous, contains forbidden tokens, or is invalid
     */
    static validate(command: string): void {
        if (!command || typeof command !== 'string') {
            throw new Error('Invalid command: must be a non-empty string');
        }

        const cmdTrimmed = command.trim();
        if (cmdTrimmed.length === 0) {
            return; // Empty commands are technically valid (no-op)
        }

        if (cmdTrimmed.includes('\n') || cmdTrimmed.includes('\r')) {
            throw new Error('Multi-line console commands are not allowed. Send one command per call.');
        }

        const cmdLower = cmdTrimmed.toLowerCase();

        // Use the single generated fail-closed policy. The generated policy
        // reproduces the prior TS block behavior exactly (Task 6 baseline).
        if (applyGeneratedConsoleCommandPolicy(cmdLower, 'typescript')) {
            throw new Error(`Dangerous command blocked: ${command}`);
        }

        // Block backticks which can be used for shell execution (covered by the
        // generated UNSAFE_SEPARATOR rule; kept explicit for the clear message).
        if (cmdTrimmed.includes('`')) {
            throw new Error('Backtick characters are blocked for safety.');
        }
    }

    /**
     * Check if a command looks like an obviously invalid or mistyped command.
     * @param command - The command to check
     * @returns true if the command matches known invalid patterns
     */
    static isLikelyInvalid(command: string): boolean {
        const cmdTrimmed = command.trim();
        return this.INVALID_PATTERNS.some(pattern => pattern.test(cmdTrimmed));
    }

    /**
     * Get the priority level of a command for throttling purposes.
     * Lower numbers indicate heavier operations that need more throttling.
     * @param command - The command to evaluate
     * @returns Priority level (1=heavy, 5=medium, 7=default, 8-9=light)
     */
    static getPriority(command: string): number {
        const normalized = command.trim().toLowerCase();

        if (normalized.includes('buildlighting') || normalized.includes('buildpaths')) {
            return 1; // Heavy operation
        } else if (normalized.includes('summon') || normalized.includes('spawn')) {
            return 5; // Medium operation
        } else if (normalized.startsWith('stat')) {
            return 8; // Dedicated throttling for stat commands
        } else if (normalized.startsWith('show')) {
            return 9; // Light operation
        }
        return 7; // Default priority
    }
}
