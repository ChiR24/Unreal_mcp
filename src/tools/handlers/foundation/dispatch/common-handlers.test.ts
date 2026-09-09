import { afterEach, describe, expect, it, vi } from 'vitest';
import type { ITools } from '../../../../types/tools/tool-interfaces.js';
import { createSubActionDispatcher, executeAutomationRequest, getTimeoutMs, normalizePathFields, validateSecurityPatterns } from './common-handlers.js';

vi.mock('../../../../config.js', async (importOriginal) => {
  const actual = await importOriginal<typeof import('../../../../config.js')>();
  return {
    ...actual,
    getAdditionalPathPrefixes: () => ['/ProjectObject/']
  };
});

function createConnectedTools() {
  const sendAutomationRequest = vi.fn(async () => ({ success: true }));
  const tools: ITools = {
    systemTools: {
      executeConsoleCommand: vi.fn(async () => ({ success: true })),
      getProjectSettings: vi.fn(async () => ({}))
    },
    assetResources: {
      list: vi.fn(async () => ({}))
    },
    automationBridge: {
      isConnected: () => true,
      sendAutomationRequest
    }
  };

  return { tools, sendAutomationRequest };
}

describe('getTimeoutMs', () => {
  const originalCanonicalTimeout = process.env.MCP_REQUEST_TIMEOUT_MS;
  const originalTimeout = process.env.MCP_AUTOMATION_REQUEST_TIMEOUT_MS;

  afterEach(() => {
    if (originalCanonicalTimeout === undefined) {
      delete process.env.MCP_REQUEST_TIMEOUT_MS;
    } else {
      process.env.MCP_REQUEST_TIMEOUT_MS = originalCanonicalTimeout;
    }
    if (originalTimeout === undefined) {
      delete process.env.MCP_AUTOMATION_REQUEST_TIMEOUT_MS;
    } else {
      process.env.MCP_AUTOMATION_REQUEST_TIMEOUT_MS = originalTimeout;
    }
  });

  it('uses the default when the timeout env var is not set', () => {
    delete process.env.MCP_REQUEST_TIMEOUT_MS;
    delete process.env.MCP_AUTOMATION_REQUEST_TIMEOUT_MS;

    expect(getTimeoutMs(1234)).toBe(1234);
  });

  it('accepts canonical positive decimal integer strings', () => {
    process.env.MCP_REQUEST_TIMEOUT_MS = '60000';

    expect(getTimeoutMs(1234)).toBe(60000);
  });

  it('accepts the legacy automation-specific timeout alias', () => {
    delete process.env.MCP_REQUEST_TIMEOUT_MS;
    process.env.MCP_AUTOMATION_REQUEST_TIMEOUT_MS = '45000';

    expect(getTimeoutMs(1234)).toBe(45000);
  });

  it('prefers the canonical timeout over the legacy alias', () => {
    process.env.MCP_REQUEST_TIMEOUT_MS = '60000';
    process.env.MCP_AUTOMATION_REQUEST_TIMEOUT_MS = '45000';

    expect(getTimeoutMs(1234)).toBe(60000);
  });

  it('rejects partial, non-decimal, fractional, zero, and negative strings', () => {
    for (const value of ['5000ms', '0x1388', '5e3', '100.5', '0', '-1']) {
      process.env.MCP_REQUEST_TIMEOUT_MS = value;

      expect(getTimeoutMs(1234)).toBe(1234);
    }
  });
});

describe('normalizePathFields', () => {
  it('maps Content and Game root aliases without duplicating /Game', () => {
    const normalized = normalizePathFields({
      contentPath: 'Content/Foo/Bar',
      gamePath: 'Game/Foo/Bar',
      enginePath: 'Engine/EngineMaterials/DefaultMaterial',
      niagaraPath: 'Niagara/Modules/EmitterState',
      pluginPath: 'ProjectObject/Materials/M_Test',
      windowsContentPath: 'Content\\Foo\\Bar',
      barePath: 'Foo/Bar'
    }, ['contentPath', 'gamePath', 'enginePath', 'niagaraPath', 'pluginPath', 'windowsContentPath', 'barePath']);

    expect(normalized.contentPath).toBe('/Game/Foo/Bar');
    expect(normalized.gamePath).toBe('/Game/Foo/Bar');
    expect(normalized.enginePath).toBe('/Engine/EngineMaterials/DefaultMaterial');
    expect(normalized.niagaraPath).toBe('/Niagara/Modules/EmitterState');
    expect(normalized.pluginPath).toBe('/ProjectObject/Materials/M_Test');
    expect(normalized.windowsContentPath).toBe('/Game/Foo/Bar');
    expect(normalized.barePath).toBe('/Game/Foo/Bar');
  });

  it('maps a bare root alias with no subpath', () => {
    const normalized = normalizePathFields({
      contentRoot: '/Content',
      windowsContentRoot: '\\Content'
    }, ['contentRoot', 'windowsContentRoot']);

    expect(normalized.contentRoot).toBe('/Game');
    expect(normalized.windowsContentRoot).toBe('/Game');
  });

  it('leaves a folder that merely starts with Content alone', () => {
    const normalized = normalizePathFields({
      siblingPath: '/ContentOther/Foo'
    }, ['siblingPath']);

    expect(normalized.siblingPath).toBe('/ContentOther/Foo');
  });

  it('blocks parent-directory path segments after alias normalization', () => {
    for (const value of ['..', 'Foo/..', 'Foo\\..', '/Game/..', '/Game/Foo/../Bar']) {
      const normalized = normalizePathFields({ assetPath: value }, ['assetPath']);

      expect(validateSecurityPatterns(normalized)).toContain('Path traversal is not allowed');
    }
  });

  it('allows dot characters inside normal path segments', () => {
    expect(validateSecurityPatterns({ assetPath: '/Game/Foo..Bar/Baz' })).toBeUndefined();
  });

  it.each([
    '/TEMP/unreal-mcp/snapshot.json',
    '/temp/unreal-mcp/snapshot.json'
  ])('allows native snapshot path alias %s', path => {
    expect(validateSecurityPatterns({ outputPath: path })).toBeUndefined();
  });

  it.each([
    '/tmp/mcp-sequence-render',
    '/TMP/mcp-sequence-render'
  ])('allows local temporary output paths for filesystem outputs %s', path => {
    expect(validateSecurityPatterns({ outputDirectory: path })).toBeUndefined();
    expect(validateSecurityPatterns({ filePath: `${path}/source.webm` })).toBeUndefined();
  });

  it.each([
    '/Saved/unreal-mcp/snapshot.json',
    '/saved/unreal-mcp/snapshot.json'
  ])('allows snapshot-only Saved path alias %s', path => {
    expect(validateSecurityPatterns({
      action: 'export_snapshot',
      path
    })).toBeUndefined();
    expect(validateSecurityPatterns({
      action: 'import_snapshot',
      path
    })).toBeUndefined();
  });

  it('allows the native snapshot outputPath alias under Saved', () => {
    expect(validateSecurityPatterns({
      action: 'export_snapshot',
      outputPath: '/Saved/unreal-mcp/snapshot.json'
    })).toBeUndefined();
  });

  it('rejects Saved paths for unrelated actions', () => {
    expect(validateSecurityPatterns({
      action: 'create_asset',
      path: '/Saved/unreal-mcp/snapshot.json'
    })).toContain('Security violation');
  });

  it.each([
    '/etc/passwd',
    '/home/user/snapshot.json',
    'C:\\Windows\\System32\\config\\SAM',
    '/TEMP/unreal-mcp/../escape.json',
    '/Saved/../escape.json'
  ])('continues to reject host or traversing path %s', path => {
    expect(validateSecurityPatterns({ outputPath: path })).toContain('Security violation');
  });

  // Plugin/game-feature content lives under its own mount root, which is
  // per-project and therefore unknowable to a static allowlist. The engine's
  // mount table is the authority (FPackageName::IsValidLongPackageName), so an
  // unknown root must come back as NOT_FOUND from Unreal, not as a security
  // violation from the gateway.
  it.each([
    '/Pedestrian_System/Blueprints/BP_Pedestrian',
    '/Aura/Characters/BP_Hero',
    '/LowEntry/Widgets/WBP_Panel',
    '/Paper2D/Textures/T_Sprite'
  ])('allows plugin content mount %s for asset paths', blueprintPath => {
    expect(validateSecurityPatterns({
      action: 'get_graph_details',
      blueprintPath
    })).toBeUndefined();
  });

  it('still rejects on-disk project roots that are shaped like a mount', () => {
    expect(validateSecurityPatterns({
      action: 'get_graph_details',
      blueprintPath: '/Binaries/Win64/payload.dll'
    })).toContain('Security violation');
  });

  // sourcePath is an on-disk file for import but a VIRTUAL asset path for rename/duplicate/move (aliased
  // from assetPath). Keying the strict list on the name alone made the SAME call pass or fail depending on
  // which alias the caller used — and the plugin-mount tests above all use blueprintPath, so they could not
  // see it. This pair IS the invariant.
  it.each(['rename', 'duplicate', 'move'])('treats sourcePath as a virtual asset path for %s', action => {
    expect(validateSecurityPatterns({
      action,
      sourcePath: '/Paper2D/PlaceholderTextures/DummySpriteTexture'
    })).toBeUndefined();
  });

  it('still treats sourcePath as an on-disk path for import', () => {
    expect(validateSecurityPatterns({
      action: 'import',
      sourcePath: '/Pedestrian_System/x.png'
    })).toContain('Security violation');
  });

  it('gives the same answer for sourcePath and its assetPath alias', () => {
    const viaSource = validateSecurityPatterns({ action: 'rename', sourcePath: '/Aura/Characters/BP_Hero' });
    const viaAsset = validateSecurityPatterns({ action: 'rename', assetPath: '/Aura/Characters/BP_Hero' });
    expect(viaSource).toEqual(viaAsset);
  });

  it('does not relax filesystem keys to plugin mount roots', () => {
    expect(validateSecurityPatterns({
      action: 'configure_output_settings',
      outputPath: '/Pedestrian_System/out.png'
    })).toContain('Security violation');
  });

  it('rejects unauthorized render output directories before Unreal dispatch', () => {
    expect(validateSecurityPatterns({
      action: 'configure_output_settings',
      outputDirectory: '/home/user/.ssh'
    })).toContain('Security violation');
  });

  it('validates local media path arrays before Unreal dispatch', () => {
    expect(validateSecurityPatterns({
      action: 'create_media_playlist',
      filePaths: ['/tmp/mcp-sequence/source.webm']
    })).toBeUndefined();
    expect(validateSecurityPatterns({
      action: 'create_media_playlist',
      filePaths: ['/tmp/mcp-sequence/../escape.webm']
    })).toContain('Path traversal is not allowed');
  });

  it('rejects network media URLs at the TypeScript boundary', () => {
    expect(validateSecurityPatterns({
      action: 'create_media_source',
      streamUrl: 'https://example.invalid/cinematics.m3u8'
    })).toContain('network media URLs are disabled');
    expect(validateSecurityPatterns({
      action: 'create_media_source',
      streamUrl: 'https://other.invalid/cinematics.m3u8'
    })).toContain('network media URLs are disabled');
    expect(validateSecurityPatterns({
      action: 'create_media_source',
      url: 'file:///tmp/cinematics.webm'
    })).toContain('file URL');
    expect(validateSecurityPatterns({
      action: 'create_media_source',
      urls: ['ftp://example.invalid/cinematics.webm']
    })).toContain('network media URLs are disabled');
  });

  it.each([
    'http://127.0.0.1:3000/mcp',
    'http://localhost:8090/private',
    'http://10.0.0.8/media.webm',
    'http://169.254.169.254/latest/meta-data',
    'http://[::1]/media.webm',
    'https://service.internal/media.webm'
  ])('rejects network media URL %s before Unreal dispatch', url => {
    expect(validateSecurityPatterns({
      action: 'create_media_source',
      streamUrl: url
    })).toContain('network media URLs are disabled');
  });
});

describe('executeAutomationRequest console command validation', () => {
  it('blocks unsafe console_command payloads before sending to the bridge', async () => {
    const { tools, sendAutomationRequest } = createConnectedTools();

    await expect(executeAutomationRequest(tools, 'console_command', { command: 'py print("unsafe")' }))
      .rejects.toThrow(/Dangerous command blocked/);

    expect(sendAutomationRequest).not.toHaveBeenCalled();
  });

  it('validates each batch_console_commands entry before sending', async () => {
    const { tools, sendAutomationRequest } = createConnectedTools();

    await expect(executeAutomationRequest(tools, 'batch_console_commands', {
      commands: ['stat fps', { command: 'quit' }]
    })).rejects.toThrow(/Dangerous command blocked/);

    expect(sendAutomationRequest).not.toHaveBeenCalled();
  });

  it('validates cmd when a batch command object has an empty command alias', async () => {
    const { tools, sendAutomationRequest } = createConnectedTools();

    for (const command of ['', '   ']) {
      await expect(executeAutomationRequest(tools, 'batch_console_commands', {
        commands: [{ command, cmd: 'quit' }]
      })).rejects.toThrow(/Dangerous command blocked/);
    }

    expect(sendAutomationRequest).not.toHaveBeenCalled();
  });

  it('sends safe console commands after validation', async () => {
    const { tools, sendAutomationRequest } = createConnectedTools();

    await executeAutomationRequest(tools, 'console_command', { command: 'stat fps' });

    expect(sendAutomationRequest).toHaveBeenCalledWith('console_command', { command: 'stat fps' }, { timeoutMs: expect.any(Number) });
  });

  it('forwards timeoutMs to Unreal only when explicitly requested', async () => {
    const { tools, sendAutomationRequest } = createConnectedTools();

    await executeAutomationRequest(
      tools,
      'manage_sequence',
      { action: 'start_render', timeoutMs: 60000 },
      undefined,
      { timeoutMs: 65000, forwardTimeoutMsToUnreal: true }
    );

    expect(sendAutomationRequest).toHaveBeenCalledWith(
      'manage_sequence',
      { action: 'start_render', timeoutMs: 60000 },
      { timeoutMs: 65000 }
    );
  });
});

describe('createSubActionDispatcher', () => {
  it('normalizes configured paths, applies payload preparation, and uses dispatcher timeout', async () => {
    const { tools, sendAutomationRequest } = createConnectedTools();
    const dispatcher = createSubActionDispatcher(tools, {
      action: 'outer_action',
      assetPath: 'Content\\UI\\WBP_Menu',
      timeoutMs: 50
    }, {
      toolName: 'manage_widget_authoring',
      domainName: 'widget authoring',
      pathFields: ['assetPath', 'folderPath'],
      timeoutMs: 900,
      preparePayload: (payload, subAction) => ({
        ...payload,
        preparedSubAction: subAction
      })
    });

    await dispatcher.sendRequest('create_widget', {
      folderPath: 'ProjectObject\\Menus'
    });

    expect(dispatcher.argsRecord.assetPath).toBe('/Game/UI/WBP_Menu');
    expect(sendAutomationRequest).toHaveBeenCalledWith('manage_widget_authoring', {
      action: 'outer_action',
      assetPath: '/Game/UI/WBP_Menu',
      folderPath: '/ProjectObject/Menus',
      subAction: 'create_widget',
      preparedSubAction: 'create_widget'
    }, { timeoutMs: 900 });
  });
});
