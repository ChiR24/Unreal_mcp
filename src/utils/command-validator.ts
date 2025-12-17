export class CommandValidator {
    private static readonly DANGEROUS_COMMANDS = [
        'quit', 'exit', 'delete', 'destroy', 'kill', 'crash',
        'viewmode visualizebuffer basecolor',
        'viewmode visualizebuffer worldnormal',
        'r.gpucrash',
        'buildpaths', // Can cause access violation if nav system not initialized
        'rebuildnavigation', // Can also crash without proper nav setup
        'obj garbage', 'obj list', 'memreport' // Heavy debug commands that can stall
    ];

    private static readonly FORBIDDEN_TOKENS = [
        'rm ', 'rm-', 'del ', 'format ', 'shutdown', 'reboot',
        'rmdir', 'mklink', 'copy ', 'move ', 'start "', 'system(',
        'import os', 'import subprocess', 'subprocess.', 'os.system',
        'exec(', 'eval(', '__import__', 'import sys', 'import importlib',
        'with open', 'open(', 'write(', 'read('
    ];

    private static readonly INVALID_PATTERNS = [
        /^\d+$/,  // Just numbers
        /^invalid_command/i,
        /^this_is_not_a_valid/i,
    ];

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

        if (cmdLower === 'py' || cmdLower.startsWith('py ')) {
            throw new Error('Python console commands are blocked from external calls for safety.');
        }

        if (this.DANGEROUS_COMMANDS.some(dangerous => cmdLower.includes(dangerous))) {
            throw new Error(`Dangerous command blocked: ${command}`);
        }

        if (cmdLower.includes('&&') || cmdLower.includes('||')) {
            throw new Error('Command chaining with && or || is blocked for safety.');
        }

        if (this.FORBIDDEN_TOKENS.some(token => cmdLower.includes(token))) {
            throw new Error(`Command contains unsafe token and was blocked: ${command}`);
        }
    }

    static isLikelyInvalid(command: string): boolean {
        const cmdTrimmed = command.trim();
        return this.INVALID_PATTERNS.some(pattern => pattern.test(cmdTrimmed));
    }

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
