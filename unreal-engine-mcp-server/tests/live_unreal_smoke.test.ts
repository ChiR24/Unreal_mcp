import { describe, it, expect } from 'vitest'
import { UnrealBridge } from '../src/unreal-bridge.js'

// Live smoke tests against a running Unreal Editor with Remote Control enabled.
// Prerequisites:
// - UE running with Web Remote Control (HTTP 30010, WS 30020) on 127.0.0.1
// - PythonScript plugin allowed for remote execution (per README)

describe('Live Unreal smoke', () => {
  it('connects to Unreal Remote Control (WebSocket)', async () => {
    const bridge = new UnrealBridge()
    const connected = await bridge.tryConnect(1, 5000, 0)
    expect(connected).toBe(true)
    // Note: Some setups disable RC HTTP; we only assert WS connectivity here.
  })
})
