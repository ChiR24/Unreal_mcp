import type { CapabilityRecordSource } from '../../model.js';
import { utilityRecord } from '../utility/utility-record-builders.js';

const T = 'manage_networking' as const;
const ONLINE = ['OnlineSubsystem', 'OnlineSubsystemUtils'] as const;
const s = (action: string, summary: string, params: readonly string[] = [], required: readonly string[] = [], outputs: readonly string[] = [], outputRequired: readonly string[] = [], effect: 'read' | 'write' | 'destructive' = 'write', requiredOneOf?: readonly string[], states: readonly ('edit' | 'pie' | 'simulate')[] = ['edit', 'pie']): CapabilityRecordSource => utilityRecord({
  tool: T, action, family: 'session', summary, params, required, requiredOneOf, outputs, outputRequired,
  plugins: ONLINE, states, effect, 
  safeToRetry: effect === 'read', dispatchAction: 'manage_sessions',
});

export const NETWORKING_SESSION_RECORDS: readonly CapabilityRecordSource[] = [
  s('configure_split_screen', 'Enable or disable local split-screen and set its layout.', ['enabled', 'splitScreenType'], [], [], [], 'write', ['enabled', 'splitScreenType']),
  // Local players only exist while a game instance runs (dogfood #180).
  s('add_local_player', 'Add a local player and return its player state.', ['controllerId'], ['controllerId'], ['playerIndex'], ['playerIndex'], 'write', undefined, ['pie']),
  s('remove_local_player', 'Remove a local player.', ['playerIndex'], ['playerIndex'], [], [], 'destructive', undefined, ['pie']),
  s('host_lan_server', 'Build the listen-server travel URL for a map and optionally travel to it.', ['mapName', 'serverName', 'maxPlayers', 'travelOptions', 'executeTravel'], ['mapName'], ['serverName', 'mapPath', 'travelURL'], ['serverName', 'travelURL']),
  s('enable_voice_chat', 'Enable or disable online voice chat.', ['voiceEnabled'], ['voiceEnabled']),
  s('mute_player', 'Set mute state for an online player.', ['playerName', 'targetPlayerId', 'muted', 'localPlayerNum', 'systemWide'], [], undefined, undefined, undefined, ['playerName', 'targetPlayerId']),
  s('get_sessions_info', 'Read identifiable online-session state.', [], [], ['sessionsInfo'], ['sessionsInfo'], 'read'),
];
