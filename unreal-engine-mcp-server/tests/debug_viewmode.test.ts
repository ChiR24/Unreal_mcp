import { describe, it, expect, vi } from 'vitest'
import { DebugVisualizationTools } from '../src/tools/debug.js'

class MockBridge {
  public executeConsoleCommand = vi.fn(async (_cmd: string) => ({ ok: true }))
}

describe('DebugVisualizationTools.setViewMode', () => {
  it('routes Collision* to show flags, not viewmode', async () => {
    const bridge = new MockBridge()
    const tools = new DebugVisualizationTools(bridge as any)

    await tools.setViewMode({ mode: 'CollisionPawn' as any })

    const calls = bridge.executeConsoleCommand.mock.calls.map((c: any[]) => String(c[0]).toLowerCase())
    // Expect a 'show collision' call and no 'viewmode collisionpawn'
    expect(calls.some(c => c.startsWith('show collision'))).toBe(true)
    expect(calls.some(c => c.includes('viewmode collisionpawn'))).toBe(false)
  })

  it('accepts valid view modes and calls viewmode', async () => {
    const bridge = new MockBridge()
    const tools = new DebugVisualizationTools(bridge as any)

    await tools.setViewMode({ mode: 'Wireframe' })
    const calls = bridge.executeConsoleCommand.mock.calls.map((c: any[]) => String(c[0]))
    expect(calls.some(c => c === 'viewmode Wireframe')).toBe(true)
  })
})
