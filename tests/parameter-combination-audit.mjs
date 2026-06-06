#!/usr/bin/env node

import { parseAuditOptions, printUsage } from './parameter-audit-cli.mjs';
import { buildAudit, printSummary, writeReport } from './parameter-audit-coverage.mjs';

const options = parseAuditOptions(process.argv.slice(2));

if (options.help) {
  printUsage();
  process.exit(0);
}

if (options.unknownFlags.length > 0) {
  console.error(`Unknown option(s): ${options.unknownFlags.join(', ')}`);
  console.error('Run with --help for usage.');
  process.exit(2);
}

const audit = await buildAudit(options);
const reportPath = writeReport(audit);
printSummary(audit, reportPath);

if (audit.totals.missingActions > 0 || audit.totals.extraActions > 0) {
  process.exitCode = 1;
}

if (!options.staticOnly && (audit.totals.missingLiveReports > 0 || audit.totals.failedLiveCases > 0)) {
  process.exitCode = 1;
}

if (options.strict && audit.totals.extraParameters > 0) {
  process.exitCode = 1;
}

if (options.optionalStrict && audit.totals.missingOptionalParameters > 0) {
  process.exitCode = 1;
}
