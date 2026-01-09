#!/usr/bin/env node
/**
 * Baseline Token Measurement Script
 * 
 * Measures the token count of tool schemas using multiple methods:
 * - Raw character count / 4 (approximation)
 * - tiktoken (accurate GPT-4 tokenization)
 * - Before/after schema pruning
 * 
 * Outputs results to scripts/token-baseline.json
 */

import { readFileSync, writeFileSync } from 'fs';
import { fileURLToPath } from 'url';
import { dirname, join } from 'path';
import { encoding_for_model } from 'tiktoken';

const __dirname = dirname(fileURLToPath(import.meta.url));
const ROOT = join(__dirname, '..');

// ============================================================================
// Schema Pruning Logic (replicated from src/server/tool-registry.ts:48-97)
// ============================================================================

const TOOLLIST_SCHEMA_DROP_KEYS = new Set([
    'title', 'examples', 'default', '$comment', 'deprecated', 'readOnly', 'writeOnly',
    // validation constraints (LLM doesn't need these for tool selection)
    'minimum', 'maximum', 'minLength', 'maxLength', 'pattern', 'format',
    'minItems', 'maxItems', 'uniqueItems', 'exclusiveMinimum', 'exclusiveMaximum', 'multipleOf',
    // JSON Schema meta
    '$schema', '$id', '$ref', '$defs', 'definitions', 'additionalProperties', 'additionalItems', 'propertyNames'
]);

const TOOLLIST_DESCRIPTION_KEEP_KEYS = new Set([
    // ONLY keep descriptions for truly ambiguous/critical parameters
    'assetPath', 'assetPaths', 'blueprintPath',
    'overwrite', 'delete', 'delete_assets', 'delete_asset', 'force'
]);

function isPlainObject(value) {
    return Boolean(value) && typeof value === 'object' && !Array.isArray(value);
}

function shouldKeepDescription(parentKey) {
    if (!parentKey) return false;
    if (parentKey === 'action') return true;
    return TOOLLIST_DESCRIPTION_KEEP_KEYS.has(parentKey);
}

function pruneSchemaForToolList(schema, parentKey) {
    if (Array.isArray(schema)) {
        return schema.map(v => pruneSchemaForToolList(v, parentKey));
    }

    if (!isPlainObject(schema)) return schema;

    const out = {};
    for (const [k, v] of Object.entries(schema)) {
        if (TOOLLIST_SCHEMA_DROP_KEYS.has(k)) continue;

        if (k === 'description' && !shouldKeepDescription(parentKey)) {
            continue;
        }

        out[k] = pruneSchemaForToolList(v, k);
    }

    return out;
}

// ============================================================================
// Token Counting
// ============================================================================

let encoder;
try {
    encoder = encoding_for_model('gpt-4');
} catch (e) {
    console.error('Failed to initialize tiktoken encoder:', e.message);
    process.exit(1);
}

function countTokens(text) {
    return encoder.encode(text).length;
}

function countChars(text) {
    return text.length;
}

// ============================================================================
// Load Tool Definitions
// ============================================================================

async function loadToolDefinitions() {
    // Try loading from dist first (compiled), fallback to parsing source
    const distPath = join(ROOT, 'dist', 'tools', 'consolidated-tool-definitions.js');
    
    try {
        const module = await import('file://' + distPath.replace(/\\/g, '/'));
        return module.consolidatedToolDefinitions;
    } catch (e) {
        console.error('Failed to load from dist:', e.message);
        console.log('Please run: npm run build:core');
        process.exit(1);
    }
}

// ============================================================================
// Format MCP Tool for Measurement
// ============================================================================

function formatToolForMcp(tool) {
    // MCP sends tools in this format (based on SDK)
    return {
        name: tool.name,
        description: tool.description,
        inputSchema: tool.inputSchema
    };
}

// ============================================================================
// Main Measurement
// ============================================================================

async function main() {
    console.log('='.repeat(70));
    console.log('MCP Tool Token Baseline Measurement');
    console.log('='.repeat(70));
    console.log();

    const tools = await loadToolDefinitions();
    console.log(`Loaded ${tools.length} tools\n`);

    const results = {
        generated_at: new Date().toISOString(),
        summary: {},
        per_tool: []
    };

    let totalRawChars = 0;
    let totalRawTokens = 0;
    let totalPrunedChars = 0;
    let totalPrunedTokens = 0;

    // Measure each tool
    for (const tool of tools) {
        const mcpFormat = formatToolForMcp(tool);
        const rawJson = JSON.stringify(mcpFormat);
        const rawChars = countChars(rawJson);
        const rawTokens = countTokens(rawJson);

        // Apply pruning
        const prunedSchema = pruneSchemaForToolList(mcpFormat.inputSchema);
        const prunedMcpFormat = {
            ...mcpFormat,
            inputSchema: prunedSchema
        };
        const prunedJson = JSON.stringify(prunedMcpFormat);
        const prunedChars = countChars(prunedJson);
        const prunedTokens = countTokens(prunedJson);

        // Count actions if present
        let actionCount = 0;
        if (tool.inputSchema?.properties?.action?.enum) {
            actionCount = tool.inputSchema.properties.action.enum.length;
        }

        const toolResult = {
            name: tool.name,
            actionCount,
            raw: {
                chars: rawChars,
                chars_div_4: Math.ceil(rawChars / 4),
                tokens: rawTokens
            },
            pruned: {
                chars: prunedChars,
                chars_div_4: Math.ceil(prunedChars / 4),
                tokens: prunedTokens
            },
            reduction: {
                chars_pct: ((1 - prunedChars / rawChars) * 100).toFixed(1),
                tokens_pct: ((1 - prunedTokens / rawTokens) * 100).toFixed(1)
            }
        };

        results.per_tool.push(toolResult);

        totalRawChars += rawChars;
        totalRawTokens += rawTokens;
        totalPrunedChars += prunedChars;
        totalPrunedTokens += prunedTokens;
    }

    // Sort by pruned tokens (largest first)
    results.per_tool.sort((a, b) => b.pruned.tokens - a.pruned.tokens);

    // Summary
    results.summary = {
        tool_count: tools.length,
        total_actions: results.per_tool.reduce((sum, t) => sum + t.actionCount, 0),
        raw: {
            chars: totalRawChars,
            chars_div_4: Math.ceil(totalRawChars / 4),
            tokens: totalRawTokens
        },
        pruned: {
            chars: totalPrunedChars,
            chars_div_4: Math.ceil(totalPrunedChars / 4),
            tokens: totalPrunedTokens
        },
        reduction: {
            chars_pct: ((1 - totalPrunedChars / totalRawChars) * 100).toFixed(1),
            tokens_pct: ((1 - totalPrunedTokens / totalRawTokens) * 100).toFixed(1)
        },
        targets: {
            must_have: { tokens: 50000, reduction_needed_pct: ((1 - 50000 / totalPrunedTokens) * 100).toFixed(1) },
            should_have: { tokens: 40000, reduction_needed_pct: ((1 - 40000 / totalPrunedTokens) * 100).toFixed(1) },
            stretch: { tokens: 30000, reduction_needed_pct: ((1 - 30000 / totalPrunedTokens) * 100).toFixed(1) }
        }
    };

    // Output
    console.log('SUMMARY');
    console.log('-'.repeat(70));
    console.log(`Tools:          ${results.summary.tool_count}`);
    console.log(`Total Actions:  ${results.summary.total_actions}`);
    console.log();
    console.log('RAW (before pruning):');
    console.log(`  Characters:   ${totalRawChars.toLocaleString()}`);
    console.log(`  Chars/4:      ${Math.ceil(totalRawChars / 4).toLocaleString()} (approximation)`);
    console.log(`  Tiktoken:     ${totalRawTokens.toLocaleString()} tokens`);
    console.log();
    console.log('PRUNED (current state with existing optimizations):');
    console.log(`  Characters:   ${totalPrunedChars.toLocaleString()}`);
    console.log(`  Chars/4:      ${Math.ceil(totalPrunedChars / 4).toLocaleString()} (approximation)`);
    console.log(`  Tiktoken:     ${totalPrunedTokens.toLocaleString()} tokens`);
    console.log();
    console.log(`Pruning Reduction: ${results.summary.reduction.tokens_pct}% tokens`);
    console.log();
    console.log('TARGET GAP:');
    console.log(`  MUST HAVE (50k):   Need ${results.summary.targets.must_have.reduction_needed_pct}% more reduction`);
    console.log(`  SHOULD HAVE (40k): Need ${results.summary.targets.should_have.reduction_needed_pct}% more reduction`);
    console.log(`  STRETCH (30k):     Need ${results.summary.targets.stretch.reduction_needed_pct}% more reduction`);
    console.log();
    console.log('TOP 10 LARGEST TOOLS (by pruned tokens):');
    console.log('-'.repeat(70));
    console.log('Rank  Tool Name                              Tokens   Actions');
    console.log('-'.repeat(70));
    for (let i = 0; i < Math.min(10, results.per_tool.length); i++) {
        const t = results.per_tool[i];
        console.log(`${(i + 1).toString().padStart(2)}    ${t.name.padEnd(38)} ${t.pruned.tokens.toString().padStart(6)}   ${t.actionCount.toString().padStart(4)}`);
    }
    console.log();

    // Save to file
    const outputPath = join(__dirname, 'token-baseline.json');
    writeFileSync(outputPath, JSON.stringify(results, null, 2));
    console.log(`Results saved to: ${outputPath}`);

    // Cleanup
    encoder.free();
}

main().catch(err => {
    console.error('Error:', err);
    process.exit(1);
});
