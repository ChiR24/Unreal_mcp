import { describe, it, expect } from 'vitest';
import { CommandValidator } from './command-validator.js';

// The invariants that matter: dangerous verbs, Python, command chaining and
// shell-reaching snippets are refused in any spelling; ordinary commands pass.
const REFUSED: ReadonlyArray<readonly [string, string]> = [
  ['dangerous first token', 'quit'],
  ['dangerous first token, padded', ' quit\t'],
  ['dangerous first token, upper case', 'CRASH'],
  ['whole-verb debug', 'debug assert'],
  ['exec runs a file of commands', 'exec commands.txt'],
  ['python', 'py'],
  ['python with args', 'py print("hello")'],
  ['python with a tab', 'py\tprint("hello")'],
  ['python alias', 'python print("hello")'],
  ['separator ;', 'stat fps; quit'],
  ['separator &&', 'stat fps && quit'],
  ['separator |', 'stat fps | quit'],
  ['backtick', 'stat `fps`'],
  ['python import', 'import os'],
  ['python import, extra whitespace', 'import\tos'],
  ['exec call', 'exec ('],
  ['open call', 'open ('],
  ['shell start', 'start "cmd"'],
  ['plugin-only restricted verb', 'delete everything'],
  ['plugin-only forbidden token', 'memreport -full']
];

const ALLOWED = ['stat fps', 'stat none', 'viewmode lit', 'r.ScreenPercentage 75', 'showflag.navigation 1'];

describe('CommandValidator', () => {
  it.each(REFUSED)('refuses %s: %j', (_label, command) => {
    expect(() => CommandValidator.validate(command)).toThrow(/Dangerous command blocked/);
  });

  it.each(ALLOWED)('allows %j', (command) => {
    expect(() => CommandValidator.validate(command)).not.toThrow();
  });

  it('refuses multi-line input', () => {
    expect(() => CommandValidator.validate('stat fps\nquit')).toThrow(/Multi-line/);
  });
});
