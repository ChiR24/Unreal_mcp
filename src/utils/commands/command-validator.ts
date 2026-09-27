/**
 * Validates console commands before execution to prevent dangerous operations.
 * The rules live in console-command-policy-rules.ts; the plugin re-enforces them.
 */
import { isConsoleCommandBlocked } from './console-command-policy-matching.js';

export class CommandValidator {
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

        // Backticks are in the unsafe-separator rule, so no separate check is needed.
        if (isConsoleCommandBlocked(cmdLower)) {
            throw new Error(`Dangerous command blocked: ${command}`);
        }
    }
}
