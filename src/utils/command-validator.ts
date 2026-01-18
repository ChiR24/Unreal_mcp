/**
 * Validates console commands before execution to prevent dangerous operations.
 * Blocks crash-inducing commands, shell injection, and Python execution.
 */
export class CommandValidator {
    /**
     * Commands that can crash the engine or cause severe instability.
     * These are blocked unconditionally.
     */
    private static readonly DANGEROUS_COMMANDS = [
        // Engine termination commands
        'quit', 'exit', 'kill', 'crash',
        // Crash-inducing commands
        'r.gpucrash', 'r.crash', 'debug crash', 'forcecrash', 'debug break',
        'assert false', 'check(false)',
        // View buffer commands that can crash on some hardware
        'viewmode visualizebuffer basecolor',
        'viewmode visualizebuffer worldnormal',
        // Heavy operations that can cause access violations if systems not initialized
        'buildpaths', 'rebuildnavigation',
        // Heavy debug commands that can stall or crash
        'obj garbage', 'obj list', 'memreport',
        // Potentially destructive without proper setup
        'delete', 'destroy'
    ];

    /**
     * Tokens that indicate shell injection or external system access attempts.
     * Any command containing these is blocked.
     * 
     * NOTE: These patterns are designed to avoid false positives:
     * - 'rm ' and 'move ' would match log strings like "transform" or "Asset: move asset"
     * - Instead, we check for actual shell command patterns at word boundaries or start of line
     */
    private static readonly FORBIDDEN_TOKENS = [
        // Shell commands (Windows/Unix) - use patterns that avoid matching inside words
        'del ', 'format ', 'shutdown', 'reboot',
        'rmdir', 'mklink', 'start "', 'system(',
        // Python injection attempts
        'import os', 'import subprocess', 'subprocess.', 'os.system',
        'exec(', 'eval(', '__import__', 'import sys', 'import importlib',
        'with open', 'open(', 'write(', 'read('
    ];

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
     * Pre-compiled patterns for dangerous commands using word boundaries.
     * This prevents false positives like 'show exit menu' matching 'exit'.
     */
    private static readonly DANGEROUS_PATTERNS = CommandValidator.DANGEROUS_COMMANDS.map(
        cmd => new RegExp(`(?:^|\\s)${cmd.replace(/[.*+?^${}()|[\]\\]/g, '\\$&')}(?:\\s|$)`, 'i')
    );

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

        // Check for 'py' followed by any whitespace or end of string
        // This catches 'py', 'py ', 'py\t', etc. to prevent bypasses
        if (/^py(?:\s|$)/.test(cmdLower)) {
            throw new Error('Python console commands are blocked from external calls for safety.');
        }

        // Use word-boundary matching to avoid false positives like 'show exit menu'
        if (this.DANGEROUS_PATTERNS.some(pattern => pattern.test(cmdLower))) {
            throw new Error(`Dangerous command blocked: ${command}`);
        }

        if (cmdLower.includes('&&') || cmdLower.includes('||')) {
            throw new Error('Command chaining with && or || is blocked for safety.');
        }

        // Block semicolon and pipe which can also be used for command chaining/injection
        if (cmdTrimmed.includes(';')) {
            throw new Error('Command chaining with ; (semicolon) is blocked for safety.');
        }
        if (cmdTrimmed.includes('|')) {
            throw new Error('Command piping with | is blocked for safety.');
        }

        if (this.FORBIDDEN_TOKENS.some(token => cmdLower.includes(token))) {
            throw new Error(`Command contains unsafe token and was blocked: ${command}`);
        }

        // Check for shell commands with word boundary matching to avoid false positives
        // like matching "rm " inside "transform " or "move " inside "Asset: move asset"
        // Only match when command STARTS with these patterns (actual shell injection)
        const cmdStart = cmdLower.split(' ')[0] ?? '';
        if (cmdStart === 'rm' || cmdStart === 'copy' || cmdStart === 'move') {
            throw new Error(`Command '${cmdStart}' contains forbidden shell command and is blocked`);
        }

        // Block backticks which can be used for shell execution
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
        if (command.includes('BuildLighting') || command.includes('BuildPaths')) {
            return 1; // Heavy operation
        } else if (command.includes('summon') || command.includes('spawn')) {
            return 5; // Medium operation
        } else if (command.startsWith('stat')) {
            return 8; // Dedicated throttling for stat commands
        } else if (command.startsWith('show')) {
            return 9; // Light operation
        }
        return 7; // Default priority
    }
}
