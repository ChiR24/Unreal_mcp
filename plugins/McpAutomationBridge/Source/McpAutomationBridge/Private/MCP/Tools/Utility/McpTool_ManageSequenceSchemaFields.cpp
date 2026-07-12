#include "MCP/Tools/Utility/McpTool_ManageSequenceSchemaFields.h"

#include "MCP/Registry/McpSchemaBuilder.h"

namespace {
TSharedPtr<FJsonObject> GetSchemaProperties(FMcpSchemaBuilder &Schema) {
	return Schema.GetProperties();
}

void AddAnyValue(FMcpSchemaBuilder &Schema, const TCHAR *Name,
				 const TCHAR *Description) {
	TSharedPtr<FJsonObject> Field = MakeShared<FJsonObject>();
	Field->SetStringField(TEXT("description"), Description);
	GetSchemaProperties(Schema)->SetObjectField(Name, Field);
}

void AddTypeUnion(FMcpSchemaBuilder &Schema, const TCHAR *Name,
				  const TArray<FString> &Types, const TCHAR *Description) {
	TSharedPtr<FJsonObject> Field = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> TypeValues;
	for (const FString &Type : Types) {
		TypeValues.Add(MakeShared<FJsonValueString>(Type));
	}
	Field->SetArrayField(TEXT("type"), TypeValues);
	Field->SetStringField(TEXT("description"), Description);
	GetSchemaProperties(Schema)->SetObjectField(Name, Field);
}
}

namespace McpManageSequenceSchemaFields {

void AddManageSequenceFields(FMcpSchemaBuilder &Schema) {
	Schema
		.String(TEXT("name"), TEXT("Name identifier."))
		.String(TEXT("path"), TEXT("Asset path or folder path."))
		.String(TEXT("assetPath"), TEXT("Asset path."))
		.String(TEXT("sequencePath"), TEXT("Level Sequence asset path."))
		.String(TEXT("masterSequencePath"), TEXT("Master Level Sequence asset path."))
		.String(TEXT("subsequencePath"), TEXT("Subsequence asset path."))
		.String(TEXT("shotSequencePath"), TEXT("Shot sequence asset path."))
		.String(TEXT("mapPath"), TEXT("Map asset path."))
		.String(TEXT("mediaPlayerPath"), TEXT("Media Player asset path."))
		.String(TEXT("mediaSourcePath"), TEXT("Media Source asset path."))
		.String(TEXT("playlistPath"), TEXT("Media Playlist asset path."))
		.String(TEXT("playerPath"), TEXT("Media Player asset path alias."))
		.String(TEXT("sourcePath"), TEXT("Media Source asset path alias."))
		.String(TEXT("defaultSourcePath"), TEXT("Default platform media source asset path."))
		.String(TEXT("animationPath"), TEXT("Animation asset path."))
		.String(TEXT("animationSequencePath"), TEXT("Animation Sequence asset path."))
		.String(TEXT("skeletalMeshPath"), TEXT("Skeletal mesh asset path."))
		.String(TEXT("materialPath"), TEXT("Optional expected material asset for the selected component slot."))
		.String(TEXT("cameraShakePath"), TEXT("Camera shake asset path."))
		.String(TEXT("cameraShakeClass"), TEXT("Camera shake class path."))
		.String(TEXT("takePresetPath"), TEXT("Take Recorder preset asset path."))
		.String(TEXT("recordingSequencePath"), TEXT("Recorded Level Sequence asset path."))
		.String(TEXT("takeSequencePath"), TEXT("Take Recorder target Level Sequence asset path."))
		.String(TEXT("actorName"), TEXT("Actor name."))
		.String(TEXT("cameraActorName"), TEXT("Cine camera actor name."))
		.String(TEXT("cameraName"), TEXT("Cine camera actor name alias."))
		.String(TEXT("targetActor"), TEXT("Target actor name alias."))
		.String(TEXT("bindingGuid"), TEXT("MovieScene binding GUID."))
		.String(TEXT("sectionName"), TEXT("Section name."))
		.String(TEXT("shotName"), TEXT("Shot name."))
		.String(TEXT("label"), TEXT("Actor label."))
		.String(TEXT("displayName"), TEXT("Display name."))
		.String(TEXT("renderJobName"), TEXT("Movie Render Queue job name."))
		.String(TEXT("jobName"), TEXT("Movie Render Queue job name alias."))
		.String(TEXT("jobId"), TEXT("Movie Render Queue job identifier."))
		.String(TEXT("renderJobId"), TEXT("Movie Render Queue job identifier alias."))
		.String(TEXT("replayName"), TEXT("Replay/demo name."))
		.String(TEXT("friendlyName"), TEXT("Replay friendly name."))
		.String(TEXT("sourceType"), TEXT("Media source type."))
		.String(TEXT("filePath"), TEXT("Allowed local media file path."))
		.String(TEXT("mediaPath"), TEXT("Allowed local media file path alias."))
		.String(TEXT("url"), TEXT("HTTP/HTTPS media URL."))
		.String(TEXT("streamUrl"), TEXT("HTTP/HTTPS stream media URL."))
		.String(TEXT("outputDirectory"), TEXT("Allowed local render output directory."))
		.String(TEXT("fileNameFormat"), TEXT("Render output file-name format."))
		.String(TEXT("renderPass"), TEXT("Render pass identifier."))
		.String(TEXT("antiAliasingMethod"), TEXT("Anti-aliasing method."))
		.String(TEXT("method"), TEXT("Anti-aliasing method alias."))
		.String(TEXT("executorClass"), TEXT("Movie Render Queue executor class."))
		.String(TEXT("componentName"), TEXT("Component name."))
		.String(TEXT("demoName"), TEXT("Replay/demo name alias."))
		.String(TEXT("property"), TEXT("Property name alias."))
		.String(TEXT("propertyName"), TEXT("Property name."))
		.String(TEXT("propertyPath"), TEXT("Property path."))
		.String(TEXT("propertyType"), TEXT("Property type."))
		.String(TEXT("parameterName"), TEXT("Parameter name."))
		.String(TEXT("destinationPath"), TEXT("Destination path for move/copy."))
		.String(TEXT("newName"), TEXT("New name for renaming."))
		.String(TEXT("loopMode"), TEXT("Media loop mode."))
		.String(TEXT("className"), TEXT("Class path or name."))
		.String(TEXT("trackType"), TEXT("MovieScene track type."))
		.String(TEXT("trackName"), TEXT("Track name."))
		.String(TEXT("recordType"), TEXT("Take Recorder record type."))
		.String(TEXT("visibility"), TEXT("Level visibility state."))
		.String(TEXT("resolution"), TEXT("Resolution setting."))
		.Array(TEXT("sourcePaths"), TEXT("Media source asset paths."))
		.Array(TEXT("actorNames"), TEXT("Actor names."))
		.Array(TEXT("actors"), TEXT("Actor names alias."))
		.Array(TEXT("sourceClasses"), TEXT("Take Recorder source classes."))
		.Array(TEXT("sourceActors"), TEXT("Take Recorder source actor names."))
		.Array(TEXT("filePaths"), TEXT("Allowed local media file paths."))
		.Array(TEXT("urls"), TEXT("HTTP/HTTPS media URLs."))
		.Array(TEXT("renderPasses"), TEXT("Render pass identifiers."))
		.Array(TEXT("tracks"), TEXT("Recorded track names."))
		.Array(TEXT("trackNames"), TEXT("Recorded track names alias."))
		.Array(TEXT("levelNames"), TEXT("Level names."))
		.Array(TEXT("properties"), TEXT("Property names."))
		.Array(TEXT("additionalOptions"), TEXT("Replay additional options."))
		.Integer(TEXT("frame"), TEXT(""))
		.Number(TEXT("speed"), TEXT(""))
		.Number(TEXT("playbackSpeed"), TEXT(""))
		.Number(TEXT("focalLength"), TEXT(""))
		.Number(TEXT("aperture"), TEXT(""))
		.Number(TEXT("focusDistance"), TEXT(""))
		.Number(TEXT("currentFocalLength"), TEXT(""))
		.Number(TEXT("currentAperture"), TEXT(""))
		.Number(TEXT("manualFocusDistance"), TEXT(""))
		.Number(TEXT("sensorWidth"), TEXT(""))
		.Number(TEXT("sensorHeight"), TEXT(""))
		.Number(TEXT("positionOnRail"), TEXT(""))
		.Number(TEXT("cranePitch"), TEXT(""))
		.Number(TEXT("craneYaw"), TEXT(""))
		.Number(TEXT("craneArmLength"), TEXT(""))
		.Integer(TEXT("width"), TEXT(""))
		.Integer(TEXT("height"), TEXT(""))
		.Number(TEXT("seekTime"), TEXT(""))
		.Number(TEXT("time"), TEXT(""))
		.Number(TEXT("timeSeconds"), TEXT(""))
		.Number(TEXT("seconds"), TEXT(""))
		.Number(TEXT("duration"), TEXT(""))
		.Number(TEXT("durationSeconds"), TEXT(""))
		.Number(TEXT("startTime"), TEXT(""))
		.Number(TEXT("endTime"), TEXT(""))
		.Number(TEXT("from"), TEXT(""))
		.Number(TEXT("to"), TEXT(""))
		.Integer(TEXT("startFrame"), TEXT(""))
		.Integer(TEXT("endFrame"), TEXT(""))
		.Number(TEXT("checkpointSaveMaxMSPerFrame"), TEXT(""))
		.Number(TEXT("maxRecordTimeSeconds"), TEXT(""))
		.Number(TEXT("timeoutMs"), TEXT(""))
		.Number(TEXT("start"), TEXT(""))
		.Number(TEXT("end"), TEXT(""))
		.Integer(TEXT("lengthInFrames"), TEXT(""))
		.Integer(TEXT("playbackStart"), TEXT(""))
		.Integer(TEXT("playbackEnd"), TEXT(""))
		.Integer(TEXT("temporalSampleCount"), TEXT(""))
		.Integer(TEXT("spatialSampleCount"), TEXT(""))
		.Integer(TEXT("durationFrames"), TEXT(""))
		.Integer(TEXT("rowIndex"), TEXT(""))
		.Integer(TEXT("sectionIndex"), TEXT(""))
		.Integer(TEXT("materialIndex"), TEXT(""))
		.Integer(TEXT("playlistIndex"), TEXT(""))
		.Bool(TEXT("spawnable"), TEXT(""))
		.Bool(TEXT("activate"), TEXT(""))
		.Bool(TEXT("autoClear"), TEXT(""))
		.Bool(TEXT("autoPlay"), TEXT(""))
		.Bool(TEXT("playOnOpen"), TEXT(""))
		.Bool(TEXT("loop"), TEXT(""))
		.Bool(TEXT("looping"), TEXT(""))
		.Bool(TEXT("enabled"), TEXT(""))
		.Bool(TEXT("paused"), TEXT(""))
		.Bool(TEXT("clearSources"), TEXT(""))
		.Bool(TEXT("disableOthers"), TEXT(""))
		.Bool(TEXT("includeTranslucentObjects"), TEXT(""))
		.Bool(TEXT("loadDefaultMapOnStop"), TEXT(""))
		.Bool(TEXT("onlyJob"), TEXT(""))
		.Bool(TEXT("precacheFile"), TEXT(""))
		.Bool(TEXT("prioritizeActors"), TEXT(""))
		.Bool(TEXT("recordParentHierarchy"), TEXT(""))
		.Bool(TEXT("recordInto"), TEXT(""))
		.Bool(TEXT("reduceKeys"), TEXT(""))
		.Bool(TEXT("save"), TEXT(""))
		.Bool(TEXT("useCurrentLevel"), TEXT(""))
		.Bool(TEXT("muted"), TEXT(""))
		.Bool(TEXT("solo"), TEXT(""))
		.Bool(TEXT("locked"), TEXT(""))
		.Object(TEXT("location"), TEXT(""), [](FMcpSchemaBuilder &Location) {
			Location
				.Number(TEXT("x"), TEXT(""))
				.Number(TEXT("y"), TEXT(""))
				.Number(TEXT("z"), TEXT(""));
		})
		.Object(TEXT("rotation"), TEXT(""), [](FMcpSchemaBuilder &Rotation) {
			Rotation
				.Number(TEXT("pitch"), TEXT(""))
				.Number(TEXT("yaw"), TEXT(""))
				.Number(TEXT("roll"), TEXT(""));
		})
		.FreeformObject(TEXT("consoleVariables"), TEXT(""))
		.Object(TEXT("burnIn"), TEXT(""), [](FMcpSchemaBuilder &BurnIn) {
			BurnIn
				.Bool(TEXT("enabled"), TEXT(""))
				.Bool(TEXT("compositeOntoFinalImage"), TEXT(""))
				.String(TEXT("classPath"), TEXT(""));
		})
		.FreeformObject(TEXT("filmback"), TEXT(""))
		.FreeformObject(TEXT("lens"), TEXT(""))
		.FreeformObject(TEXT("focus"), TEXT(""))
		.Object(TEXT("settings"), TEXT(""), [](FMcpSchemaBuilder &Settings) {
			Settings
				.Integer(TEXT("handleFrameCount"), TEXT(""))
				.Integer(TEXT("zeroPadFrameNumbers"), TEXT(""))
				.Integer(TEXT("spatialSampleCount"), TEXT(""))
				.Integer(TEXT("temporalSampleCount"), TEXT(""))
				.String(TEXT("antiAliasingMethod"), TEXT(""))
				.String(TEXT("method"), TEXT(""));
		})
		.FreeformObject(TEXT("platformSources"), TEXT(""))
		.FreeformObject(TEXT("metadata"), TEXT(""));
	AddAnyValue(Schema, TEXT("value"), TEXT("Generic keyframe or parameter value."));
	AddTypeUnion(
		Schema, TEXT("frameRate"), {TEXT("number"), TEXT("string")},
		TEXT("Frames per second or a rate string such as 24fps or 24000/1001."));
}

}
