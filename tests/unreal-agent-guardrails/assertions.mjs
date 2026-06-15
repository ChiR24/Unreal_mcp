import assert from 'node:assert/strict';

export async function assertRejects(
  action,
  expectedText,
  label = 'guardrail action',
) {
  try {
    await action();
  } catch (error) {
    assert.match(String(error), expectedText, label);
    return;
  }
  assert.fail(`${label} was not rejected`);
}
