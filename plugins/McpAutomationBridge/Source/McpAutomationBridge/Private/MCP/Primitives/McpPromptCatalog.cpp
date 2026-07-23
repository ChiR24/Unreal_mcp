// McpPromptCatalog.cpp
// Task 32 (native mirror): the six versioned, user-selected workflow prompt
// definitions and the closed-allowlist + secret-name guards. This is DATA ONLY,
// mirroring `src/server/mcp-primitives/prompts/workflow-prompts.ts`: no
// execution, no stored state, no autonomy or memory instruction. Every
// CapabilityId exists in the generated canonical registry and every ResourceUri
// is a Task 31 approved uri; the source-contract test asserts TS/native parity.
// This translation unit is compiled by a later combined BuildPlugin (Task 37).
#include "McpPromptCatalog.h"

const TArray<FMcpWorkflowPrompt>& McpWorkflowPrompts()
{
	static const TArray<FMcpWorkflowPrompt> Prompts = {
		FMcpWorkflowPrompt{ TEXT("inspect-fix"), 1, TEXT("Inspect and fix an object property"),
			{ {TEXT("objectPath"), TEXT("object-path"), true}, {TEXT("propertyName"), TEXT("identifier"), false}, {TEXT("newValue"), TEXT("text"), false} },
			{ {TEXT("inspect.get_selected_actors"), TEXT("inspect"), TEXT("get_selected_actors"), TEXT("ue://selection")},
			  {TEXT("inspect.inspect_object"), TEXT("inspect"), TEXT("inspect_object"), TEXT("ue://object/{objectPath}")},
			  {TEXT("inspect.get_property"), TEXT("inspect"), TEXT("get_property")},
			  {TEXT("inspect.set_property"), TEXT("inspect"), TEXT("set_property")} } },
		FMcpWorkflowPrompt{ TEXT("asset-import"), 1, TEXT("Import an asset into the project"),
			{ {TEXT("destinationPath"), TEXT("content-path"), true}, {TEXT("sourceFormat"), TEXT("enum"), false, {TEXT("fbx"), TEXT("obj"), TEXT("gltf"), TEXT("png"), TEXT("wav")}} },
			{ {TEXT("asset.list"), TEXT("manage_asset"), TEXT("list"), TEXT("ue://project")},
			  {TEXT("asset.exists"), TEXT("manage_asset"), TEXT("exists"), TEXT("ue://asset/{assetPath}")},
			  {TEXT("asset.import"), TEXT("manage_asset"), TEXT("import")},
			  {TEXT("asset.validate"), TEXT("manage_asset"), TEXT("validate")} } },
		FMcpWorkflowPrompt{ TEXT("level-build"), 1, TEXT("Create and build a level"),
			{ {TEXT("levelPath"), TEXT("content-path"), true} },
			{ {TEXT("manage_level.get_current_level"), TEXT("manage_level"), TEXT("get_current_level"), TEXT("ue://level")},
			  {TEXT("manage_level.create_level"), TEXT("manage_level"), TEXT("create_level"), TEXT("ue://editor")},
			  {TEXT("manage_level.build_lighting"), TEXT("manage_level"), TEXT("build_lighting")},
			  {TEXT("manage_level.save"), TEXT("manage_level"), TEXT("save")} } },
		FMcpWorkflowPrompt{ TEXT("blueprint-edit"), 1, TEXT("Edit a Blueprint"),
			{ {TEXT("blueprintPath"), TEXT("content-path"), true}, {TEXT("variableName"), TEXT("identifier"), false} },
			{ {TEXT("blueprint.get"), TEXT("manage_blueprint"), TEXT("get"), TEXT("ue://object/{objectPath}")},
			  {TEXT("blueprint.add_variable"), TEXT("manage_blueprint"), TEXT("add_variable")},
			  {TEXT("blueprint.add_scs_component"), TEXT("manage_blueprint"), TEXT("add_scs_component")},
			  {TEXT("blueprint.compile"), TEXT("manage_blueprint"), TEXT("compile")} } },
		FMcpWorkflowPrompt{ TEXT("validation"), 1, TEXT("Validate project assets and level"),
			{ {TEXT("assetPath"), TEXT("content-path"), false} },
			{ {TEXT("inspect.get_project_settings"), TEXT("inspect"), TEXT("get_project_settings"), TEXT("ue://project")},
			  {TEXT("system_control.validate_assets"), TEXT("system_control"), TEXT("validate_assets"), TEXT("ue://capability/catalog")},
			  {TEXT("asset.validate"), TEXT("manage_asset"), TEXT("validate"), TEXT("ue://asset/{assetPath}")},
			  {TEXT("manage_level.validate_level"), TEXT("manage_level"), TEXT("validate_level")} } },
		FMcpWorkflowPrompt{ TEXT("sequence-render"), 1, TEXT("Render a level sequence"),
			{ {TEXT("sequencePath"), TEXT("content-path"), true}, {TEXT("outputFormat"), TEXT("enum"), false, {TEXT("png"), TEXT("jpeg"), TEXT("exr"), TEXT("custom")}} },
			{ {TEXT("sequence.get_properties"), TEXT("manage_sequence"), TEXT("get_properties"), TEXT("ue://project")},
			  {TEXT("sequence.mrq.create_render_job"), TEXT("manage_sequence"), TEXT("create_render_job")},
			  {TEXT("sequence.mrq.configure_output_settings"), TEXT("manage_sequence"), TEXT("configure_output_settings"), TEXT("ue://editor")},
			  {TEXT("sequence.mrq.queue_render"), TEXT("manage_sequence"), TEXT("queue_render")},
			  {TEXT("sequence.mrq.start_render"), TEXT("manage_sequence"), TEXT("start_render")} } },
	};
	return Prompts;
}

const TArray<FString>& McpWorkflowPromptIds()
{
	static const TArray<FString> Ids = {
		TEXT("inspect-fix"), TEXT("asset-import"), TEXT("level-build"),
		TEXT("blueprint-edit"), TEXT("validation"), TEXT("sequence-render"),
	};
	return Ids;
}

bool McpIsWorkflowPromptId(const FString& Name)
{
	return McpWorkflowPromptIds().Contains(Name);
}

bool McpPromptArgumentNamesSecret(const FString& ArgumentName)
{
	const FString Lower = ArgumentName.ToLower();
	static const TArray<FString> Fragments = {
		TEXT("token"), TEXT("secret"), TEXT("password"), TEXT("passwd"),
		TEXT("apikey"), TEXT("api_key"), TEXT("credential"),
		TEXT("privatekey"), TEXT("private_key"), TEXT("bearer"), TEXT("auth"),
	};
	for (const FString& Fragment : Fragments)
	{
		if (Lower.Contains(Fragment))
		{
			return true;
		}
	}
	return false;
}
