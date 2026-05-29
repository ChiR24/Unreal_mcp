export function expectedCondition(expectation) {
  if (expectation && typeof expectation === 'object') {
    if (typeof expectation.condition === 'string') return expectation.condition;

    const conditions = [];
    if (typeof expectation.successPattern === 'string') {
      conditions.push('success', expectation.successPattern);
    }
    if (typeof expectation.errorPattern === 'string') {
      conditions.push('error', expectation.errorPattern);
    }
    if (conditions.length > 0) return conditions.join('|');

    try {
      return JSON.stringify(expectation);
    } catch {
      return String(expectation);
    }
  }
  return typeof expectation === 'string' ? expectation : String(expectation ?? '');
}

export function splitExpectedConditions(condition) {
  if (typeof condition !== 'string') return [];
  if (condition.includes(' or ')) return condition.split(' or ').map((entry) => entry.trim()).filter(Boolean);
  if (condition.includes('|')) return condition.split('|').map((entry) => entry.trim()).filter(Boolean);
  const trimmed = condition.trim();
  return trimmed ? [trimmed] : [];
}
