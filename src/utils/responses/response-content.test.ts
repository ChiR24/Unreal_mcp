import { describe, expect, it } from 'vitest';
import { buildSummaryText } from './response-content.js';

describe('buildSummaryText — output field budget', () => {
  it('keeps a short output verbatim', () => {
    const text = buildSummaryText('system_control', {
      success: true,
      result: { output: 'MONTAGE COUNT 3' },
      message: 'Python executed successfully'
    });

    expect(text).toContain('output: MONTAGE COUNT 3');
    expect(text).not.toContain('truncated');
  });

  it('keeps a multi-line output between 150 and 2000 chars intact (not clipped at 150)', () => {
    const lines = Array.from({ length: 20 }, (_, i) => `/Game/Anims/AM_Attack_${String(i).padStart(2, '0')}`);
    const output = lines.join('\n'); // ~500 chars — the execute_python listing shape
    const text = buildSummaryText('system_control', { success: true, result: { output } });

    expect(text).toContain('AM_Attack_19'); // the tail survives
    expect(text).not.toContain('truncated');
  });

  it('announces truncation explicitly above 2000 chars — sizes + structuredContent pointer, no bare ellipsis', () => {
    const output = 'X'.repeat(5000);
    const text = buildSummaryText('system_control', { success: true, result: { output } });

    expect(text).toContain('[output truncated: showing 2000 of 5000 chars — full text in structuredContent]');
    expect(text).not.toContain('X'.repeat(2001)); // budget enforced
    expect(text).toContain('X'.repeat(2000)); // budget delivered
  });

  it('other long string fields keep the compact 150-char summary behaviour', () => {
    const long = 'Y'.repeat(500);
    const text = buildSummaryText('some_tool', { success: true, detail: long });

    expect(text).toContain(`detail: ${'Y'.repeat(150)}...`);
    expect(text).not.toContain('Y'.repeat(151));
  });

  it('non-string output values fall through to the generic formatter', () => {
    const text = buildSummaryText('some_tool', { success: true, output: ['a', 'b'] });

    expect(text).toContain('output: [a, b] (2)');
  });
});
