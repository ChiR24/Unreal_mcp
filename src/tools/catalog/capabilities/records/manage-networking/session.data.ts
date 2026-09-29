import type { CapabilityRecordSource } from '../../model.js';
import { utilityRecord, withTopics } from '../utility/utility-record-builders.js';

const T = 'manage_networking' as const;
const ONLINE = ['OnlineSubsystem', 'OnlineSubsystemUtils'] as const;
const s = (action: string, summary: string, params: readonly string[] = [], required: readonly string[] = [], outputs: readonly string[] = [], outputRequired: readonly string[] = [], effect: 'read' | 'write' | 'destructive' = 'write', requiredOneOf?: readonly string[], states: readonly ('edit' | 'pie' | 'simulate')[] = ['edit', 'pie']): CapabilityRecordSource => utilityRecord({
  tool: T, action, family: 'session', summary, params, required, requiredOneOf, outputs, outputRequired,
  plugins: ONLINE, states, effect, 
  safeToRetry: effect === 'read', dispatchAction: 'manage_sessions',
});

const SPLIT_SCREEN = 'Turn local split screen on or off and pick a layout by name: None (off), TwoPlayer_Horizontal, TwoPlayer_Vertical, ThreePlayer_FavorTop, ThreePlayer_FavorBottom, ThreePlayer_Vertical, ThreePlayer_Horizontal, FourPlayer_Grid, FourPlayer_Vertical or FourPlayer_Horizontal. Only the layout for that player count changes; written to DefaultEngine.ini and live in PIE.';
const SPLIT_OUT = ['enabled', 'splitScreenType', 'layoutsWritten', 'settingsSaved'] as const;
const SPLIT_OUT_REQUIRED = ['enabled', 'settingsSaved'] as const;

export const NETWORKING_SESSION_RECORDS: readonly CapabilityRecordSource[] = [
  s('configure_split_screen', SPLIT_SCREEN, ['enabled', 'splitScreenType'], [], SPLIT_OUT, SPLIT_OUT_REQUIRED, 'write', ['enabled', 'splitScreenType']),
  // The older name of the same operation; the native handler is configure_split_screen's.
  s('set_split_screen_type', SPLIT_SCREEN, ['splitScreenType', 'enabled'], ['splitScreenType'], SPLIT_OUT, SPLIT_OUT_REQUIRED),
  s('configure_lan_play', 'Set the project default game port ([URL] Port in DefaultEngine.ini) that listen servers bind and joins without a port connect to; packaged games use it from their next launch and the running editor at once.', ['serverPort'], ['serverPort'], ['serverPort', 'previousPort', 'configFile', 'persisted'], ['serverPort', 'previousPort', 'configFile', 'persisted']),
  s('configure_session_interface', 'Choose the online subsystem the project uses ([OnlineSubsystem] DefaultPlatformService in DefaultEngine.ini): Null for LAN and local play, Steam, EOS and others whose OnlineSubsystem plugin is enabled; also turns on [OnlineSubsystem<Name>] bEnabled when the engine config has it off. Takes effect on the next launch (Steam only in standalone and packaged games).', ['interfaceType'], ['interfaceType'], ['interfaceType', 'previous', 'configFile', 'written', 'persisted', 'requiresRestart'], ['interfaceType', 'configFile', 'persisted', 'requiresRestart']),
  s('join_lan_server', 'Make a running Play-In-Editor instance join a server at serverAddress:serverPort (ClientTravel); the connection completes asynchronously. Refused when PIE is not running or every instance is hosting.', ['serverAddress', 'serverPort', 'travelOptions'], ['serverAddress'], ['connectionURL', 'pieWorld', 'travelStarted'], ['connectionURL', 'pieWorld', 'travelStarted']),
  s('configure_voice_settings', 'Configure project VoIP: turn online voice on or off, microphone gain, noise gate and silence detection thresholds, and the VoIP sample rate. Written to DefaultEngine.ini for packaged games; the voice console variables also apply to the running editor.', ['voiceEnabled', 'micInputGain', 'noiseGateThreshold', 'silenceDetectionThreshold', 'sampleRate'], [], ['written', 'configFile', 'liveApplied', 'requiresRestart'], ['written', 'configFile', 'liveApplied', 'requiresRestart'], 'write', ['voiceEnabled', 'micInputGain', 'noiseGateThreshold', 'silenceDetectionThreshold', 'sampleRate']),
  // Local players only exist while a game instance runs (dogfood #180).
  s('add_local_player', 'Add a local player and return its player state.', ['controllerId'], ['controllerId'], ['playerIndex'], ['playerIndex'], 'write', undefined, ['pie']),
  s('remove_local_player', 'Remove a local player.', ['playerIndex'], ['playerIndex'], [], [], 'destructive', undefined, ['pie']),
  s('host_lan_server', 'Build the LAN listen-server travel URL for a map; nothing is hosted unless executeTravel is true, which travels the running world to it.', ['mapName', 'maxPlayers', 'travelOptions', 'executeTravel'], ['mapName'], ['mapPath', 'travelURL'], ['travelURL']),
  s('enable_voice_chat', 'Enable or disable online voice chat.', ['voiceEnabled'], ['voiceEnabled']),
  s('mute_player', 'Mute or unmute an online player through the voice chat or online voice interface; fails with NOT_SUPPORTED when neither applies it.', ['playerName', 'targetPlayerId', 'muted', 'localPlayerNum', 'systemWide'], [], undefined, undefined, undefined, ['playerName', 'targetPlayerId']),
  withTopics(s('get_sessions_info', 'Read the current play session: local player count, whether PIE is running, the split-screen setting and layout, and each PIE instance\'s net mode (Standalone, DedicatedServer, ListenServer or Client) and URL.', [], [], ['sessionsInfo'], ['sessionsInfo'], 'read'),
    ['split screen settings', 'pie net mode']),
];
