import { setTimeout } from 'node:timers/promises';

/** Retry only explicit PCG reads; never replay a scheduling or destructive request. */
export async function pollPCGRead(testCase, call, ready) {
  if (!testCase.pollUntil) return call();
  const { timeoutMs, intervalMs = 100 } = testCase.pollUntil;
  const action = testCase.arguments?.action;
  if (testCase.toolName !== 'manage_pcg' || testCase.expected !== 'success' ||
      !['get_pcg_operation_status', 'get_pcg_component_results'].includes(action) ||
      !Array.isArray(testCase.assertions) || testCase.assertions.length === 0 ||
      !Number.isFinite(timeoutMs) || timeoutMs < 50 || timeoutMs > 30000 ||
      !Number.isFinite(intervalMs) || intervalMs < 50 || intervalMs > 1000 || intervalMs > timeoutMs) {
    throw new Error('pollUntil requires a bounded PCG status/inventory read and nonempty assertions.');
  }
  const deadline = performance.now() + timeoutMs;
  let firstRead = true;
  do {
    const remaining = deadline - performance.now();
    if (!firstRead && remaining < 50) break;
    firstRead = false;
    const response = await call(Math.max(1, remaining));
    if (ready(response)) return response;
    if (performance.now() >= deadline) break;
    await setTimeout(Math.min(intervalMs, deadline - performance.now()));
  } while (performance.now() < deadline);
  throw new Error('PCG read polling deadline exceeded; native work was not cancelled. Preserve owned fixtures.');
}
