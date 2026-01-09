#!/usr/bin/env node
/**
 * Token Measurement Utility
 * 
 * Measures current token usage and shows progress toward targets.
 * Run after any schema changes to verify token reduction.
 * 
 * Usage: npm run measure:tokens
 */

import { readFileSync, existsSync } from 'fs';
import { fileURLToPath } from 'url';
import { dirname, join } from 'path';
import { encoding_for_model } from 'tiktoken';

const __dirname = dirname(fileURLToPath(import.meta.url));
const ROOT = join(__dirname, '..');

// Targets from plan
const TARGETS = {
    must_have: 50000,
    should_have: 40000,
    stretch: 30000
};

// Warning threshold per tool
const TOOL_TOKEN_WARNING = 1000;
// NOTE: MAX_ACTIONS_PER_TOOL truncation REMOVED - all actions must be visible to LLM

// ============================================================================
// Schema Pruning Logic (replicated from src/server/tool-registry.ts:48-97)
// ============================================================================

const TOOLLIST_SCHEMA_DROP_KEYS = new Set([
    'title', 'examples', 'default', '$comment', 'deprecated', 'readOnly', 'writeOnly',
    'minimum', 'maximum', 'minLength', 'maxLength', 'pattern', 'format',
    'minItems', 'maxItems', 'uniqueItems', 'exclusiveMinimum', 'exclusiveMaximum', 'multipleOf',
    '$schema', '$id', '$ref', '$defs', 'definitions', 'additionalProperties', 'additionalItems', 'propertyNames',
    'outputSchema', // AGGRESSIVE: Remove outputSchema entirely
    'category' // ULTRA-AGGRESSIVE: Remove internal category field
]);

function isPlainObject(value) {
    return Boolean(value) && typeof value === 'object' && !Array.isArray(value);
}

function shouldKeepDescription(parentKey) {
    if (!parentKey) return false;
    if (parentKey === 'action') return true;
    return false;
}

// ULTRA-AGGRESSIVE: Simplify schema objects for minimal token usage
// NOTE: Action truncation removed - all actions visible
function simplifySchemaObject(schema, parentKey) {
    if (!isPlainObject(schema)) return schema;
    
    let result = { ...schema };
    
    // Remove 'type: string' when enum is present
    if (result.enum && result.type === 'string') {
        delete result.type;
    }
    
    // ULTRA-AGGRESSIVE: Remove primitive types - LLM can infer from param names
    const primitiveTypes = ['string', 'number', 'boolean', 'integer'];
    if (primitiveTypes.includes(result.type) && !result.enum) {
        delete result.type;
    }
    
    // Simplify arrays - just { type: 'array' } without items detail
    if (result.type === 'array') {
        return { type: 'array' };
    }
    
    // Simplify location/rotation/scale/color/transform objects - just keep type: object
    if (result.type === 'object' && result.properties) {
        const propKeys = Object.keys(result.properties);
        if ((propKeys.length === 3 && 
            (propKeys.every(k => ['x', 'y', 'z'].includes(k)) ||
             propKeys.every(k => ['r', 'g', 'b'].includes(k)) ||
             propKeys.every(k => ['pitch', 'yaw', 'roll'].includes(k)))) ||
            (propKeys.length === 4 && propKeys.every(k => ['r', 'g', 'b', 'a'].includes(k))) ||
            (propKeys.length === 2 && propKeys.every(k => ['x', 'y'].includes(k))) ||
            (propKeys.length === 6 && propKeys.every(k => ['x', 'y', 'z', 'pitch', 'yaw', 'roll'].includes(k)))) {
            return { type: 'object' };
        }
    }
    
    // Remove empty required array or required: ['action'] (obvious)
    if (result.required) {
        if (result.required.length === 0 || (result.required.length === 1 && result.required[0] === 'action')) {
            delete result.required;
        }
    }
    
    // Remove empty objects
    if (Object.keys(result).length === 0) {
        return {};
    }
    
    return result;
}

function pruneSchemaForToolList(schema, parentKey) {
    if (Array.isArray(schema)) {
        return schema.map(v => pruneSchemaForToolList(v, parentKey));
    }
    if (!isPlainObject(schema)) return schema;

    const out = {};
    for (const [k, v] of Object.entries(schema)) {
        if (TOOLLIST_SCHEMA_DROP_KEYS.has(k)) continue;
        if (k === 'description' && !shouldKeepDescription(parentKey)) continue;
        out[k] = pruneSchemaForToolList(v, k);
    }
    const simplified = simplifySchemaObject(out, parentKey);
    
    // ULTRA-AGGRESSIVE: Remove root type:object (implied for tool inputSchema)
    if (parentKey === undefined && simplified.type === 'object') {
        delete simplified.type;
    }
    
    return simplified;
}

// ============================================================================
// Token Counting
// ============================================================================

let encoder;
try {
    encoder = encoding_for_model('gpt-4');
} catch (e) {
    console.error('Failed to initialize tiktoken:', e.message);
    process.exit(1);
}

function countTokens(text) {
    return encoder.encode(text).length;
}

// ============================================================================
// Load Tool Definitions
// ============================================================================

async function loadToolDefinitions() {
    const distPath = join(ROOT, 'dist', 'tools', 'consolidated-tool-definitions.js');
    try {
        const module = await import('file://' + distPath.replace(/\\/g, '/'));
        return module.consolidatedToolDefinitions;
    } catch (e) {
        console.error('Failed to load tools. Run: npm run build:core');
        process.exit(1);
    }
}

// ============================================================================
// Main
// ============================================================================

async function main() {
    const tools = await loadToolDefinitions();
    
    let totalTokens = 0;
    const warnings = [];
    const toolData = [];

    for (const tool of tools) {
        const mcpFormat = {
            name: tool.name,
            description: tool.description,
            inputSchema: pruneSchemaForToolList(tool.inputSchema)
        };
        const tokens = countTokens(JSON.stringify(mcpFormat));
        
        let actionCount = 0;
        if (tool.inputSchema?.properties?.action?.enum) {
            actionCount = tool.inputSchema.properties.action.enum.length;
        }

        toolData.push({ name: tool.name, tokens, actionCount });
        totalTokens += tokens;

        if (tokens > TOOL_TOKEN_WARNING) {
            warnings.push({ name: tool.name, tokens, reason: `>${TOOL_TOKEN_WARNING} tokens` });
        }
        // NOTE: MAX_ACTIONS_PER_TOOL warning removed - all actions visible
    }

    // Sort by tokens descending
    toolData.sort((a, b) => b.tokens - a.tokens);

    // Output
    console.log('='.repeat(70));
    console.log('MCP Tool Token Measurement');
    console.log('='.repeat(70));
    console.log();
    console.log(`Tools: ${tools.length}  |  Total Tokens: ${totalTokens.toLocaleString()}`);
    console.log();

    // Progress bars
    console.log('TARGET PROGRESS:');
    for (const [tier, target] of Object.entries(TARGETS)) {
        const pct = Math.min(100, (target / totalTokens) * 100);
        const bar = '█'.repeat(Math.floor(pct / 5)) + '░'.repeat(20 - Math.floor(pct / 5));
        const status = totalTokens <= target ? '✓' : ' ';
        const gap = totalTokens - target;
        console.log(`  ${tier.padEnd(12)} [${bar}] ${target.toLocaleString().padStart(6)} ${status} (${gap > 0 ? '+' + gap.toLocaleString() : gap.toLocaleString()})`);
    }
    console.log();

    // Warnings
    if (warnings.length > 0) {
        console.log('WARNINGS:');
        const uniqueWarnings = [...new Map(warnings.map(w => [w.name + w.reason, w])).values()];
        for (const w of uniqueWarnings.slice(0, 15)) {
            console.log(`  ⚠ ${w.name}: ${w.reason}`);
        }
        if (uniqueWarnings.length > 15) {
            console.log(`  ... and ${uniqueWarnings.length - 15} more`);
        }
        console.log();
    }

    // Top 10
    console.log('TOP 10 BY TOKENS:');
    console.log('-'.repeat(60));
    console.log('  Tool'.padEnd(42) + 'Tokens'.padStart(8) + 'Actions'.padStart(10));
    console.log('-'.repeat(60));
    for (const t of toolData.slice(0, 10)) {
        console.log(`  ${t.name.padEnd(40)} ${t.tokens.toString().padStart(6)} ${t.actionCount.toString().padStart(8)}`);
    }
    console.log();

    // Cleanup
    encoder.free();

    // Exit code based on must-have target
    if (totalTokens > TARGETS.must_have) {
        console.log(`FAIL: Exceeds MUST HAVE target (${TARGETS.must_have})`);
        process.exit(1);
    }
    console.log('PASS: Within MUST HAVE target');
}

main().catch(err => {
    console.error('Error:', err);
    process.exit(1);
});
