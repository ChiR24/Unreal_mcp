// McpTool_ManageAudio.cpp — manage_audio tool definition

#include "McpVersionCompatibility.h"  // IWYU pragma: keep
#include "MCP/McpToolDefinition.h"
#include "MCP/McpToolRegistry.h"
#include "MCP/McpSchemaBuilder.h"

class FMcpTool_ManageAudio : public FMcpToolDefinition
{
public:
	FString GetName() const override { return TEXT("manage_audio"); }

	FString GetDescription() const override
	{
		return TEXT("Play/stop sounds, add audio components, configure mixes, "
			"attenuation, spatial audio, and author Sound Cues/MetaSounds.");
	}

	FString GetCategory() const override { return TEXT("utility"); }


	TSharedPtr<FJsonObject> BuildInputSchema() const override
	{
		return FMcpSchemaBuilder()
			.StringEnum(TEXT("action"), {
				TEXT("create_sound_cue"),
				TEXT("play_sound_at_location"),
				TEXT("play_sound_2d"),
				TEXT("create_audio_component"),
				TEXT("create_sound_mix"),
				TEXT("push_sound_mix"),
				TEXT("pop_sound_mix"),
				TEXT("set_sound_mix_class_override"),
				TEXT("clear_sound_mix_class_override"),
				TEXT("set_base_sound_mix"),
				TEXT("prime_sound"),
				TEXT("play_sound_attached"),
				TEXT("spawn_sound_at_location"),
				TEXT("fade_sound_in"),
				TEXT("fade_sound_out"),
				TEXT("create_ambient_sound"),
				TEXT("create_sound_class"),
				TEXT("set_sound_attenuation"),
				TEXT("create_reverb_zone"),
				TEXT("enable_audio_analysis"),
				TEXT("fade_sound"),
				TEXT("set_doppler_effect"),
				TEXT("set_audio_occlusion"),
				TEXT("add_cue_node"),
				TEXT("connect_cue_nodes"),
				TEXT("set_cue_attenuation"),
				TEXT("set_cue_concurrency"),
				TEXT("create_metasound"),
				TEXT("add_metasound_node"),
				TEXT("connect_metasound_nodes"),
				TEXT("add_metasound_input"),
				TEXT("add_metasound_output"),
				TEXT("set_metasound_default"),
				TEXT("set_class_properties"),
				TEXT("set_class_parent"),
				TEXT("add_mix_modifier"),
				TEXT("configure_mix_eq"),
				TEXT("create_attenuation_settings"),
				TEXT("configure_distance_attenuation"),
				TEXT("configure_spatialization"),
				TEXT("configure_occlusion"),
				TEXT("configure_reverb_send"),
				TEXT("create_dialogue_voice"),
				TEXT("create_dialogue_wave"),
				TEXT("set_dialogue_context"),
				TEXT("create_reverb_effect"),
				TEXT("create_source_effect_chain"),
				TEXT("add_source_effect"),
				TEXT("create_submix_effect"),
				TEXT("get_audio_info")
			}, TEXT("Action"))
			.String(TEXT("name"), TEXT("Name identifier."))
			.String(TEXT("soundPath"), TEXT("Sound asset path."))
			.Object(TEXT("location"), TEXT("3D location (x, y, z)."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
			})
			.Object(TEXT("rotation"), TEXT("3D rotation (pitch, yaw, roll)."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("pitch")).Number(TEXT("yaw")).Number(TEXT("roll"));
			})
			.Number(TEXT("volume"), TEXT(""))
			.Number(TEXT("pitch"), TEXT(""))
			.Number(TEXT("startTime"), TEXT(""))
			.String(TEXT("attenuationPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
			.String(TEXT("concurrencyPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
			.String(TEXT("mixName"), TEXT(""))
			.String(TEXT("soundClassName"), TEXT(""))
			.Number(TEXT("fadeInTime"), TEXT(""))
			.Number(TEXT("fadeOutTime"), TEXT(""))
			.Number(TEXT("fadeTime"), TEXT(""))
			.Number(TEXT("targetVolume"), TEXT(""))
			.String(TEXT("attachPointName"), TEXT("Name of the socket."))
			.String(TEXT("actorName"), TEXT("Name of the actor."))
			.String(TEXT("componentName"), TEXT("Name of the component."))
			.String(TEXT("parentClass"), TEXT(""))
			.FreeformObject(TEXT("properties"), TEXT(""))
			.Number(TEXT("innerRadius"), TEXT(""))
			.Number(TEXT("falloffDistance"), TEXT(""))
			.String(TEXT("attenuationShape"), TEXT(""))
			.String(TEXT("falloffMode"), TEXT(""))
			.String(TEXT("reverbEffect"), TEXT(""))
			.Object(TEXT("size"), TEXT("3D scale (x, y, z)."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
			})
			.String(TEXT("analysisType"), TEXT("Audio analysis type."))
			.Number(TEXT("windowSize"), TEXT("Audio analysis window size."))
			.String(TEXT("outputType"), TEXT(""))
			.String(TEXT("soundName"), TEXT(""))
			.String(TEXT("fadeType"), TEXT(""))
			.Number(TEXT("lowPassFilterFrequency"), TEXT(""))
			.Bool(TEXT("enabled"), TEXT("Whether the item/feature is enabled."))
			.Bool(TEXT("enable"), TEXT("Whether the item/feature is enabled."))
			.String(TEXT("path"), TEXT("Directory path for asset creation."))
			.String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
			.Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))
			.String(TEXT("wavePath"), TEXT("Path to SoundWave asset."))
			.String(TEXT("nodeType"), TEXT(""))
			.String(TEXT("sourceNodeId"), TEXT("ID of the source node."))
			.String(TEXT("targetNodeId"), TEXT("ID of the target node."))
			.Bool(TEXT("looping"), TEXT("Whether to loop."))
			.String(TEXT("inputName"), TEXT("Name of the input."))
			.String(TEXT("inputType"), TEXT(""))
			.String(TEXT("outputName"), TEXT("Name of the output."))
			.String(TEXT("sourceNode"), TEXT("Source node name."))
			.String(TEXT("sourcePin"), TEXT("Name of the source pin."))
			.String(TEXT("targetNode"), TEXT("Target node name."))
			.String(TEXT("targetPin"), TEXT("Name of the target pin."))
			.FreeformObject(TEXT("defaultValue"), TEXT(""))
			.String(TEXT("soundClassPath"), TEXT("Sound class path."))
			.Number(TEXT("dopplerIntensity"), TEXT("Doppler intensity."))
			.String(TEXT("effectType"), TEXT("Effect type."))
			.Bool(TEXT("enableReverbSend"), TEXT("Whether reverb send is enabled."))
			.Number(TEXT("occlusionFilterScale"), TEXT("Occlusion filter scale."))
			.Number(TEXT("occlusionInterpolationTime"), TEXT("Occlusion interpolation time."))
			.Number(TEXT("occlusionVolumeScale"), TEXT("Occlusion volume scale."))
			.Number(TEXT("reverbDistanceMax"), TEXT("Maximum reverb send distance."))
			.Number(TEXT("reverbDistanceMin"), TEXT("Minimum reverb send distance."))
			.Number(TEXT("reverbWetLevelMax"), TEXT("Maximum reverb wet level."))
			.Number(TEXT("reverbWetLevelMin"), TEXT("Minimum reverb wet level."))
			.String(TEXT("sourceOutputName"), TEXT("Source MetaSound output name."))
			.String(TEXT("spatialization"), TEXT("Spatialization algorithm."))
			.String(TEXT("speakerPath"), TEXT("Dialogue speaker path."))
			.String(TEXT("targetInputName"), TEXT("Target MetaSound input name."))
			.Number(TEXT("velocityScale"), TEXT("Velocity scale."))
			.Number(TEXT("volumeAdjuster"), TEXT("Sound mix volume adjuster."))
			.Required({TEXT("action")})
			.Build();
	}
};

MCP_REGISTER_TOOL(FMcpTool_ManageAudio);
