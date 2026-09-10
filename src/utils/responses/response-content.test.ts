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

describe('pin linkedTo targets', () => {
  it('renders link targets as 8-hex nodeId prefixes with pin names, plus the total', () => {
    const text = buildSummaryText('probe', {
      success: true,
      pins: [{
        pinName: 'then', direction: 'Output', pinType: 'exec',
        linkedTo: [{ nodeId: '5C2E1A9F4D3B41E8A7C6F0B2D9E83714', pinName: 'execute' }],
      }],
    });
    expect(text).toContain('linkedTo=[5C2E1A9F.execute] (1)');
  });

  it('keeps the compact linkedTo=0 form for unconnected pins', () => {
    const text = buildSummaryText('probe', {
      success: true,
      pins: [{ pinName: 'then', direction: 'Output', pinType: 'exec', linkedTo: [] }],
    });
    expect(text).toContain('linkedTo=0');
  });

  it('caps at five targets and announces the spill with the total', () => {
    const links = Array.from({ length: 7 }, (_, i) => ({
      nodeId: `${String(i).padStart(2, '0')}AACCE54817BB6726E95CB9642E51DD`.slice(0, 32), pinName: 'execute',
    }));
    const text = buildSummaryText('probe', { success: true, pins: [{ pinName: 'then', linkedTo: links }] });
    expect(text).toContain(', ...] (7)');
    expect(text).toContain('00AACCE5.execute');
  });
});

describe('identity with addressable sibling', () => {
  it('pairs a display name with the path a follow-up call takes', () => {
    const text = buildSummaryText('probe', {
      success: true,
      assets: [{ name: 'BP_Player', path: '/Game/Blueprints/BP_Player' }],
    });
    expect(text).toContain('BP_Player (/Game/Blueprints/BP_Player)');
  });

  it('renders the identity alone when no addressable sibling exists', () => {
    const text = buildSummaryText('probe', { success: true, assets: [{ name: 'BP_Solo' }] });
    expect(text).toContain('BP_Solo');
    expect(text).not.toContain('BP_Solo (');
  });

  it('does not duplicate when the chosen key IS the addressable one', () => {
    const text = buildSummaryText('probe', { success: true, refs: [{ path: '/Game/Only/Path' }] });
    expect(text).toContain('/Game/Only/Path');
    expect(text).not.toContain('/Game/Only/Path (');
  });

  it('pairs sequence-style ids behind names', () => {
    const text = buildSummaryText('probe', {
      success: true,
      bindings: [{ name: 'HeroBinding', id: '8F3A2C11-0000-4000-8000-1234567890AB' }],
    });
    expect(text).toContain('HeroBinding (8F3A2C11-0000-4000-8000-1234567890AB)');
  });
});
