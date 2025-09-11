import { describe, it, expect, vi } from 'vitest'
import { AnimationTools } from '../src/tools/animation.js'

class MockBridge {
  public executePython = vi.fn(async (_script: string) => ({ LogOutput: [{ Output: 'RESULT:{"success": true, "message": "ok"}' }] }))
  public executeConsoleCommand = vi.fn(async (_cmd: string) => ({ ok: true }))
}

describe('AnimationTools.playAnimation', () => {
  it('plays montage via Python, not console', async () => {
    const bridge = new MockBridge()
    const tools = new AnimationTools(bridge as any)

    const res = await tools.playAnimation({
      actorName: 'MyCharacter',
      animationType: 'Montage',
      animationPath: '/Game/Anims/M_Run_Montage',
      playRate: 1.0,
    })

    expect(res.success).toBe(true)
    expect(bridge.executePython).toHaveBeenCalledTimes(1)
    // Ensure no console fallback was used
    expect(bridge.executeConsoleCommand).toHaveBeenCalledTimes(0)
  })

  it('plays sequence via Python on skeletal mesh component', async () => {
    const bridge = new MockBridge()
    const tools = new AnimationTools(bridge as any)

    const res = await tools.playAnimation({
      actorName: 'MyCharacter',
      animationType: 'Sequence',
      animationPath: '/Game/Anims/Idle',
      loop: true,
    })

    expect(res.success).toBe(true)
    expect(bridge.executePython).toHaveBeenCalledTimes(1)
  })
})
