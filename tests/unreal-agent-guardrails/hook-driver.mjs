export function createHookDriver(hooks) {
  const before = async (
    tool,
    sessionID,
    args,
    callID = `${sessionID}-${tool}`,
  ) => {
    await hooks['tool.execute.before'](
      { tool, sessionID, callID },
      { args },
    );
  };
  const after = async (
    tool,
    sessionID,
    args,
    output,
    callID = `${sessionID}-${tool}`,
  ) => {
    await hooks['tool.execute.after'](
      { tool, sessionID, callID, args },
      output,
    );
  };
  const routeCard = async (
    sessionID,
    role = 'assistant',
    messageID = `${sessionID}-${role}-message`,
    text = [
      'Intent: change one actor',
      'Evidence: current actor inspected',
      'Tool route: control_actor/spawn_actor',
      'Mutation bounds: one actor',
      'Safety: no overwrite',
      'Validation: inspect actor',
      'Rollback: delete created actor',
    ].join('\n'),
    extraPartFields = {},
  ) => {
    await hooks.event({
      event: {
        type: 'message.updated',
        properties: { info: { id: messageID, sessionID, role } },
      },
    });
    await hooks.event({
      event: {
        type: 'message.part.updated',
        properties: {
          part: {
            id: `${messageID}-part`,
            sessionID,
            messageID,
            type: 'text',
            text,
            ...extraPartFields,
          },
        },
      },
    });
  };
  return { after, before, hooks, routeCard };
}
