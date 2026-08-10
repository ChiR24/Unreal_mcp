import type { CapabilityRecordSource } from '../../index.js';
import { utilityRecord } from '../utility/helpers.js';

const T = 'manage_networking' as const;
const ONLINE = ['OnlineSubsystem', 'OnlineSubsystemUtils'] as const;
const s = (action: string, summary: string, params: readonly string[] = [], required: readonly string[] = [], outputs: readonly string[] = [], outputRequired: readonly string[] = [], effect: 'read' | 'write' | 'destructive' = 'write', requiredOneOf?: readonly string[]): CapabilityRecordSource => utilityRecord({
  tool: T, action, family: 'session', summary, params, required, requiredOneOf, outputs, outputRequired,
  plugins: ONLINE, states: ['edit', 'pie'], effect, supportsUndo: false,
  safeToRetry: effect === 'read', dispatchAction: 'manage_sessions',
});

export const NETWORKING_SESSION_RECORDS: readonly CapabilityRecordSource[] = [
  s('configure_local_session_settings', 'Configure local online-session settings.', ['sessionName', 'maxPlayers', 'bIsLANMatch', 'bAllowJoinInProgress', 'bAllowInvites', 'bUsesPresence', 'bUseLobbiesIfAvailable', 'bShouldAdvertise']),
  s('configure_session_interface', 'Configure the online-session interface.', ['interfaceType'], ['interfaceType']),
  s('configure_split_screen', 'Enable or disable local split-screen.', ['enabled'], ['enabled']),
  s('set_split_screen_type', 'Set the split-screen layout.', ['splitScreenType'], ['splitScreenType']),
  s('add_local_player', 'Add a local player and return its player state.', ['controllerId'], ['controllerId'], ['playerIndex'], ['playerIndex']),
  s('remove_local_player', 'Remove a local player.', ['playerIndex'], ['playerIndex'], [], [], 'destructive'),
  s('configure_lan_play', 'Configure LAN play settings.', ['enabled', 'serverPort'], ['enabled']),
  s('host_lan_server', 'Start hosting a LAN session and return session state.', ['mapName', 'sessionName', 'serverName', 'maxPlayers', 'serverPort', 'serverPassword', 'travelOptions', 'executeTravel'], ['mapName'], ['sessionName', 'serverAddress'], ['sessionName']),
  s('join_lan_server', 'Join a LAN session and return connection state.', ['serverAddress', 'serverPort', 'serverPassword'], ['serverAddress'], ['sessionName', 'serverAddress'], ['serverAddress']),
  s('enable_voice_chat', 'Enable or disable online voice chat.', ['voiceEnabled'], ['voiceEnabled']),
  s('configure_voice_settings', 'Configure online voice processing.', ['voiceSettings'], ['voiceSettings']),
  s('set_voice_channel', 'Set the active voice channel.', ['channelName', 'channelType'], ['channelName']),
  s('mute_player', 'Set mute state for an online player.', ['playerName', 'targetPlayerId', 'muted', 'localPlayerNum', 'systemWide'], [], undefined, undefined, undefined, ['playerName', 'targetPlayerId']),
  s('set_voice_attenuation', 'Configure proximity voice attenuation.', ['attenuationRadius', 'attenuationFalloff'], ['attenuationRadius']),
  s('configure_push_to_talk', 'Configure push-to-talk input.', ['pushToTalkEnabled', 'pushToTalkKey'], ['pushToTalkEnabled']),
  s('get_sessions_info', 'Read identifiable online-session state.', [], [], ['sessionsInfo'], ['sessionsInfo'], 'read'),
];
