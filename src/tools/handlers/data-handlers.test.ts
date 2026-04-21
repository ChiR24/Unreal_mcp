import { beforeEach, describe, expect, it, vi } from 'vitest';

const { executeAutomationRequestMock } = vi.hoisted(() => ({
  executeAutomationRequestMock: vi.fn(
    async (): Promise<Record<string, unknown>> => ({ success: true })
  ),
}));

vi.mock('./common-handlers.js', () => ({
  executeAutomationRequest: executeAutomationRequestMock,
  requireNonEmptyString: (value: unknown, fieldName: string) => {
    if (typeof value !== 'string' || value.trim().length === 0) {
      throw new Error(`Missing required parameter: ${fieldName}`);
    }
  },
}));

import { handleDataTools } from './data-handlers.js';

describe('manage_data skeleton', () => {
  beforeEach(() => {
    executeAutomationRequestMock.mockReset();
    executeAutomationRequestMock.mockResolvedValue({ success: true });
  });

  it('throws Unsupported for unrecognised actions', async () => {
    await expect(
      handleDataTools('__not_a_real_action__', {} as never, {} as never)
    ).rejects.toThrow(/Unsupported manage_data action/);
  });
});

describe('manage_data create_data_table', () => {
  beforeEach(() => {
    executeAutomationRequestMock.mockReset();
    executeAutomationRequestMock.mockResolvedValue({
      success: true,
      assetPath: '/Game/Data/DT_Test',
    });
  });

  it('forwards path, name, rowStructPath to the automation bridge', async () => {
    const res = await handleDataTools(
      'create_data_table',
      {
        path: '/Game/Data',
        name: 'DT_Test',
        rowStructPath: '/Game/Data/ST_Row.ST_Row',
      } as unknown as Record<string, unknown>,
      {} as never
    );

    expect(res.success).toBe(true);
    expect(res.assetPath).toBe('/Game/Data/DT_Test');
    const calls = executeAutomationRequestMock.mock.calls as unknown as Array<unknown[]>;
    expect(calls[0][1]).toBe('manage_data');
    const payload = calls[0][2] as Record<string, unknown>;
    expect(payload.subAction).toBe('create_data_table');
    expect(payload.path).toBe('/Game/Data');
    expect(payload.name).toBe('DT_Test');
    expect(payload.rowStructPath).toBe('/Game/Data/ST_Row.ST_Row');
  });

  it('throws on missing rowStructPath', async () => {
    await expect(
      handleDataTools(
        'create_data_table',
        { path: '/Game', name: 'X' } as unknown as Record<string, unknown>,
        {} as never
      )
    ).rejects.toThrow(/rowStructPath/);
  });

  it('throws on missing name', async () => {
    await expect(
      handleDataTools(
        'create_data_table',
        { path: '/Game', rowStructPath: '/Game/ST.ST' } as unknown as Record<string, unknown>,
        {} as never
      )
    ).rejects.toThrow(/name/);
  });
});

describe('manage_data add_data_table_row', () => {
  beforeEach(() => {
    executeAutomationRequestMock.mockReset();
    executeAutomationRequestMock.mockResolvedValue({ success: true, rowName: 'R1' });
  });

  it('forwards path, rowName, fields', async () => {
    const res = await handleDataTools(
      'add_data_table_row',
      { path: '/Game/DT', rowName: 'R1', fields: { DisplayName: 'A', Value: 1.5 } } as unknown as Record<string, unknown>,
      {} as never
    );
    expect(res.success).toBe(true);
    expect(res.rowName).toBe('R1');
    const calls = executeAutomationRequestMock.mock.calls as unknown as Array<unknown[]>;
    const payload = calls[0][2] as Record<string, unknown>;
    expect(payload.subAction).toBe('add_data_table_row');
    expect(payload.rowName).toBe('R1');
    expect(payload.fields).toEqual({ DisplayName: 'A', Value: 1.5 });
  });

  it('defaults fields to empty object when omitted', async () => {
    await handleDataTools(
      'add_data_table_row',
      { path: '/Game/DT', rowName: 'R1' } as unknown as Record<string, unknown>,
      {} as never
    );
    const calls = executeAutomationRequestMock.mock.calls as unknown as Array<unknown[]>;
    const payload = calls[0][2] as Record<string, unknown>;
    expect(payload.fields).toEqual({});
  });

  it('throws on missing rowName', async () => {
    await expect(
      handleDataTools(
        'add_data_table_row',
        { path: '/Game/DT', fields: {} } as unknown as Record<string, unknown>,
        {} as never
      )
    ).rejects.toThrow(/rowName/);
  });
});

describe('manage_data set_data_table_row', () => {
  beforeEach(() => {
    executeAutomationRequestMock.mockReset();
    executeAutomationRequestMock.mockResolvedValue({ success: true });
  });

  it('forwards fields overwriting full row', async () => {
    await handleDataTools(
      'set_data_table_row',
      { path: '/Game/DT', rowName: 'R1', fields: { DisplayName: 'B', Value: 9 } } as unknown as Record<string, unknown>,
      {} as never
    );
    const calls = executeAutomationRequestMock.mock.calls as unknown as Array<unknown[]>;
    const payload = calls[0][2] as Record<string, unknown>;
    expect(payload.subAction).toBe('set_data_table_row');
    expect(payload.rowName).toBe('R1');
    expect(payload.fields).toEqual({ DisplayName: 'B', Value: 9 });
  });

  it('throws on missing fields', async () => {
    await expect(
      handleDataTools(
        'set_data_table_row',
        { path: '/Game/DT', rowName: 'R1' } as unknown as Record<string, unknown>,
        {} as never
      )
    ).rejects.toThrow(/fields/);
  });
});

describe('manage_data update_data_table_row', () => {
  beforeEach(() => {
    executeAutomationRequestMock.mockReset();
    executeAutomationRequestMock.mockResolvedValue({ success: true, updatedFields: ['Value'] });
  });

  it('forwards partial fields and returns updatedFields array', async () => {
    const res = await handleDataTools(
      'update_data_table_row',
      { path: '/Game/DT', rowName: 'R1', fields: { Value: 99 } } as unknown as Record<string, unknown>,
      {} as never
    );
    expect(res.updatedFields).toEqual(['Value']);
    const calls = executeAutomationRequestMock.mock.calls as unknown as Array<unknown[]>;
    const payload = calls[0][2] as Record<string, unknown>;
    expect(payload.subAction).toBe('update_data_table_row');
    expect(payload.fields).toEqual({ Value: 99 });
  });

  it('throws on missing fields', async () => {
    await expect(
      handleDataTools(
        'update_data_table_row',
        { path: '/Game/DT', rowName: 'R1' } as unknown as Record<string, unknown>,
        {} as never
      )
    ).rejects.toThrow(/fields/);
  });
});

describe('manage_data remove_data_table_row', () => {
  beforeEach(() => {
    executeAutomationRequestMock.mockReset();
    executeAutomationRequestMock.mockResolvedValue({ success: true });
  });

  it('forwards path + rowName', async () => {
    await handleDataTools(
      'remove_data_table_row',
      { path: '/Game/DT', rowName: 'R1' } as unknown as Record<string, unknown>,
      {} as never
    );
    const calls = executeAutomationRequestMock.mock.calls as unknown as Array<unknown[]>;
    const payload = calls[0][2] as Record<string, unknown>;
    expect(payload.subAction).toBe('remove_data_table_row');
    expect(payload.rowName).toBe('R1');
  });

  it('throws on missing rowName', async () => {
    await expect(
      handleDataTools(
        'remove_data_table_row',
        { path: '/Game/DT' } as unknown as Record<string, unknown>,
        {} as never
      )
    ).rejects.toThrow(/rowName/);
  });
});

describe('manage_data get_data_table_rows', () => {
  beforeEach(() => {
    executeAutomationRequestMock.mockReset();
    executeAutomationRequestMock.mockResolvedValue({
      success: true,
      rows: { Row1: { DisplayName: 'A', Value: 1 } },
    });
  });

  it('returns rows object when no filter is provided', async () => {
    const res = await handleDataTools(
      'get_data_table_rows',
      { path: '/Game/DT' } as unknown as Record<string, unknown>,
      {} as never
    );
    expect(res.rows).toEqual({ Row1: { DisplayName: 'A', Value: 1 } });
    const calls = executeAutomationRequestMock.mock.calls as unknown as Array<unknown[]>;
    const payload = calls[0][2] as Record<string, unknown>;
    expect(payload.subAction).toBe('get_data_table_rows');
    expect(payload.rowNames).toBeUndefined();
  });

  it('forwards rowNames filter when provided', async () => {
    await handleDataTools(
      'get_data_table_rows',
      { path: '/Game/DT', rowNames: ['Row1', 'Row2'] } as unknown as Record<string, unknown>,
      {} as never
    );
    const calls = executeAutomationRequestMock.mock.calls as unknown as Array<unknown[]>;
    const payload = calls[0][2] as Record<string, unknown>;
    expect(payload.rowNames).toEqual(['Row1', 'Row2']);
  });
});

describe('manage_data list_data_table_rows', () => {
  beforeEach(() => {
    executeAutomationRequestMock.mockReset();
    executeAutomationRequestMock.mockResolvedValue({ success: true, rowNames: ['A', 'B'] });
  });

  it('returns the row name array', async () => {
    const res = await handleDataTools(
      'list_data_table_rows',
      { path: '/Game/DT' } as unknown as Record<string, unknown>,
      {} as never
    );
    expect(res.rowNames).toEqual(['A', 'B']);
    const calls = executeAutomationRequestMock.mock.calls as unknown as Array<unknown[]>;
    const payload = calls[0][2] as Record<string, unknown>;
    expect(payload.subAction).toBe('list_data_table_rows');
  });

  it('throws on missing path', async () => {
    await expect(
      handleDataTools('list_data_table_rows', {} as never, {} as never)
    ).rejects.toThrow(/path/);
  });
});

describe('manage_data create_data_asset', () => {
  beforeEach(() => {
    executeAutomationRequestMock.mockReset();
    executeAutomationRequestMock.mockResolvedValue({
      success: true,
      assetPath: '/Game/DA/DA_Item',
    });
  });

  it('forwards path + name + dataAssetClassPath', async () => {
    const res = await handleDataTools(
      'create_data_asset',
      {
        path: '/Game/DA',
        name: 'DA_Item',
        dataAssetClassPath: '/Game/BP_ItemData.BP_ItemData_C',
      } as unknown as Record<string, unknown>,
      {} as never
    );
    expect(res.success).toBe(true);
    expect(res.assetPath).toBe('/Game/DA/DA_Item');
    const calls = executeAutomationRequestMock.mock.calls as unknown as Array<unknown[]>;
    const payload = calls[0][2] as Record<string, unknown>;
    expect(payload.subAction).toBe('create_data_asset');
    expect(payload.path).toBe('/Game/DA');
    expect(payload.name).toBe('DA_Item');
    expect(payload.dataAssetClassPath).toBe('/Game/BP_ItemData.BP_ItemData_C');
  });

  it('throws on missing dataAssetClassPath', async () => {
    await expect(
      handleDataTools(
        'create_data_asset',
        { path: '/Game/DA', name: 'DA_Item' } as unknown as Record<string, unknown>,
        {} as never
      )
    ).rejects.toThrow(/dataAssetClassPath/);
  });
});

describe('manage_data set_data_table_row_struct', () => {
  beforeEach(() => {
    executeAutomationRequestMock.mockReset();
    executeAutomationRequestMock.mockResolvedValue({ success: true, rowsReinitialized: 3 });
  });

  it('forwards path + newRowStructPath and returns rowsReinitialized', async () => {
    const res = await handleDataTools(
      'set_data_table_row_struct',
      { path: '/Game/DT', newRowStructPath: '/Game/ST_New.ST_New' } as unknown as Record<string, unknown>,
      {} as never
    );
    expect(res.rowsReinitialized).toBe(3);
    const calls = executeAutomationRequestMock.mock.calls as unknown as Array<unknown[]>;
    const payload = calls[0][2] as Record<string, unknown>;
    expect(payload.subAction).toBe('set_data_table_row_struct');
    expect(payload.newRowStructPath).toBe('/Game/ST_New.ST_New');
  });

  it('throws on missing newRowStructPath', async () => {
    await expect(
      handleDataTools(
        'set_data_table_row_struct',
        { path: '/Game/DT' } as unknown as Record<string, unknown>,
        {} as never
      )
    ).rejects.toThrow(/newRowStructPath/);
  });
});
