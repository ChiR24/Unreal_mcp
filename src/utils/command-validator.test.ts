
import { describe, it, expect } from 'vitest';
import { CommandValidator } from './command-validator.js';

describe('CommandValidator', () => {
    it('blocks python commands with spaces', () => {
        expect(() => CommandValidator.validate('py print("hello")')).toThrow(/Python console commands are blocked/);
    });

    it('blocks python commands with tabs', () => {
        expect(() => CommandValidator.validate('py\tprint("hello")')).toThrow(/Python console commands are blocked/);
    });

    it('blocks simple python command', () => {
        expect(() => CommandValidator.validate('py')).toThrow(/Python console commands are blocked/);
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
        expect(() => CommandValidator.validate('import os')).toThrow(/contains unsafe/);
        expect(() => CommandValidator.validate('start "cmd"')).toThrow(/contains unsafe/);
    });

    it('allows safe commands', () => {
        expect(() => CommandValidator.validate('stat fps')).not.toThrow();
        expect(() => CommandValidator.validate('viewmode lit')).not.toThrow();
    });

    // Security Bypasses
    it('blocks bypass attempts with extra whitespace', () => {
        expect(() => CommandValidator.validate('import  os')).toThrow(/contains unsafe/);
        expect(() => CommandValidator.validate('import\tos')).toThrow(/contains unsafe/);
        expect(() => CommandValidator.validate('exec (')).toThrow(/contains unsafe/);
        expect(() => CommandValidator.validate('open (')).toThrow(/contains unsafe/);
    });

    it('blocks python command alias', () => {
        expect(() => CommandValidator.validate('python print("hello")')).toThrow(/Python console commands are blocked/);
    });
});
