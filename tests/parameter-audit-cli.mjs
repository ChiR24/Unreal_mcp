export function printUsage() {
  console.log(`Usage: node tests/parameter-combination-audit.mjs [options]

Options:
  --static             Audit static test definitions instead of live reports.
  --strict             Fail on schema/action drift and undeclared test parameters.
  --optional-strict    Also fail on unreferenced optional parameter coverage debt.
  --coverage-strict    Alias for --optional-strict.
  --help, -h           Show this help text.`);
}

export function parseAuditOptions(args) {
  const allowedFlags = new Set(['--strict', '--static', '--optional-strict', '--coverage-strict', '--help', '-h']);
  const unknownFlags = args.filter((arg) => arg.startsWith('-') && !allowedFlags.has(arg));

  return {
    help: args.includes('--help') || args.includes('-h'),
    unknownFlags,
    strict: args.includes('--strict'),
    staticOnly: args.includes('--static'),
    optionalStrict: args.includes('--optional-strict') || args.includes('--coverage-strict')
  };
}
