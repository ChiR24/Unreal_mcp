import { describe, it, expect } from 'vitest';
import { CommandValidator } from './command-validator.js';

describe('CommandValidator', () => {
    it('blocks python commands with spaces', () => {
        expect(() => CommandValidator.validate('py print("hello")')).toThrow(/Dangerous command blocked/);
    });

    it('blocks python commands with tabs', () => {
        expect(() => CommandValidator.validate('py\tprint("hello")')).toThrow(/Dangerous command blocked/);
    });

    it('blocks simple python command', () => {
        expect(() => CommandValidator.validate('py')).toThrow(/Dangerous command blocked/);
    });

    it('blocks dangerous commands', () => {
        expect(() => CommandValidator.validate('quit')).toThrow(/Dangerous command blocked/);
        expect(() => CommandValidator.validate('exit')).toThrow(/Dangerous command blocked/);
        expect(() => CommandValidator.validate('crash')).toThrow(/Dangerous command blocked/);
    });

    it('blocks dangerous commands with whitespace', () => {
        expect(() => CommandValidator.validate('quit ')).toThrow(/Dangerous command blocked/);
        expect(() => CommandValidator.validate(' quit')).toThrow(/Dangerous command blocked/);
        expect(() => CommandValidator.validate('quit\t')).toThrow(/Dangerous command blocked/);
    });

    it('blocks forbidden tokens', () => {
        expect(() => CommandValidator.validate('import os')).toThrow(/Dangerous command blocked/);
        expect(() => CommandValidator.validate('start "cmd"')).toThrow(/Dangerous command blocked/);
    });

    it('allows safe commands', () => {
        expect(() => CommandValidator.validate('stat fps')).not.toThrow();
        expect(() => CommandValidator.validate('viewmode lit')).not.toThrow();
    });

    // Security Bypasses
    it('blocks bypass attempts with extra whitespace', () => {
        expect(() => CommandValidator.validate('import  os')).toThrow(/Dangerous command blocked/);
        expect(() => CommandValidator.validate('import\tos')).toThrow(/Dangerous command blocked/);
        expect(() => CommandValidator.validate('exec (')).toThrow(/Dangerous command blocked/);
        expect(() => CommandValidator.validate('open (')).toThrow(/Dangerous command blocked/);
    });

    it('blocks python command alias', () => {
        expect(() => CommandValidator.validate('python print("hello")')).toThrow(/Dangerous command blocked/);
    });

    it('normalizes case and spacing when calculating priority', () => {
        expect(CommandValidator.getPriority('  STAT FPS')).toBe(8);
        expect(CommandValidator.getPriority('ShowFlag.Navigation')).toBe(9);
        expect(CommandValidator.getPriority('MAP BUILDLIGHTING')).toBe(1);
        expect(CommandValidator.getPriority('Summon SomeActor')).toBe(5);
    });
});
