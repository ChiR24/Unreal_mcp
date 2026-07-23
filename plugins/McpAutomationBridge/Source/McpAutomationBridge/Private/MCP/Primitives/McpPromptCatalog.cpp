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
			TEXT("Walk through inspecting a UObject and correcting a single property, one reviewed call at a time."),
			{ {TEXT("objectPath"), TEXT("object-path"), true, TEXT("Content path of the object to inspect")},
			  {TEXT("propertyName"), TEXT("identifier"), false, TEXT("Name of the property to review")},
			  {TEXT("newValue"), TEXT("text"), false, TEXT("Value you intend to set, for your reference")} },
			{ {TEXT("Read the current editor selection to confirm the target."), TEXT("inspect.get_selected_actors"), TEXT("inspect"), TEXT("get_selected_actors"), TEXT("ue://selection"), TEXT("Confirm the selected actor is the intended target before reading or changing anything.")},
			  {TEXT("Introspect the object and its components."), TEXT("inspect.inspect_object"), TEXT("inspect"), TEXT("inspect_object"), TEXT("ue://object/{objectPath}"), TEXT("Inspection is read-only; use it to understand the object before any edit.")},
			  {TEXT("Read the specific property you plan to change."), TEXT("inspect.get_property"), TEXT("inspect"), TEXT("get_property"), TEXT(""), TEXT("Note the current value so you can revert the change by hand if needed.")},
			  {TEXT("Apply the fix to a single property."), TEXT("inspect.set_property"), TEXT("inspect"), TEXT("set_property"), TEXT(""), TEXT("Review the new value; this edits editor state only when you run the call yourself.")} } },
		FMcpWorkflowPrompt{ TEXT("asset-import"), 1, TEXT("Import an asset into the project"),
			TEXT("Check the destination, import from a source you supply, then validate the result."),
			{ {TEXT("destinationPath"), TEXT("content-path"), true, TEXT("Content folder to import into")},
			  {TEXT("sourceFormat"), TEXT("enum"), false, TEXT("Source file format"), {TEXT("fbx"), TEXT("obj"), TEXT("gltf"), TEXT("png"), TEXT("wav")}} },
			{ {TEXT("List the destination folder contents."), TEXT("asset.list"), TEXT("manage_asset"), TEXT("list"), TEXT("ue://project"), TEXT("Confirm the destination path is correct so you do not overwrite existing assets.")},
			  {TEXT("Check whether the target asset already exists."), TEXT("asset.exists"), TEXT("manage_asset"), TEXT("exists"), TEXT("ue://asset/{assetPath}"), TEXT("If it already exists, decide whether replacing it is intended before importing.")},
			  {TEXT("Import the asset from your chosen source."), TEXT("asset.import"), TEXT("manage_asset"), TEXT("import"), TEXT(""), TEXT("Supply your own source path when you run this; the import applies only on execute.")},
			  {TEXT("Validate the imported asset."), TEXT("asset.validate"), TEXT("manage_asset"), TEXT("validate"), TEXT(""), TEXT("Review the validation report before using the asset in a level.")} } },
		FMcpWorkflowPrompt{ TEXT("level-build"), 1, TEXT("Create and build a level"),
			TEXT("Create a working level, build its lighting, and save it, reviewing each step."),
			{ {TEXT("levelPath"), TEXT("content-path"), true, TEXT("Content path for the level")} },
			{ {TEXT("Read the current level."), TEXT("manage_level.get_current_level"), TEXT("manage_level"), TEXT("get_current_level"), TEXT("ue://level"), TEXT("Confirm which level is active before creating or building anything.")},
			  {TEXT("Create the working level."), TEXT("manage_level.create_level"), TEXT("manage_level"), TEXT("create_level"), TEXT("ue://editor"), TEXT("Creating a level does not save it; the file is written only when you save.")},
			  {TEXT("Build lighting for the level."), TEXT("manage_level.build_lighting"), TEXT("manage_level"), TEXT("build_lighting"), TEXT(""), TEXT("Lighting builds can be slow; start the build yourself when the scene is ready.")},
			  {TEXT("Save the level."), TEXT("manage_level.save"), TEXT("manage_level"), TEXT("save"), TEXT(""), TEXT("Saving routes through the safe save wrapper; review the scene before you save.")} } },
		FMcpWorkflowPrompt{ TEXT("blueprint-edit"), 1, TEXT("Edit a Blueprint"),
			TEXT("Read a Blueprint, add a variable and an SCS component, then compile it."),
			{ {TEXT("blueprintPath"), TEXT("content-path"), true, TEXT("Content path of the Blueprint")},
			  {TEXT("variableName"), TEXT("identifier"), false, TEXT("Name of a variable to add")} },
			{ {TEXT("Read the Blueprint definition."), TEXT("blueprint.get"), TEXT("manage_blueprint"), TEXT("get"), TEXT("ue://object/{objectPath}"), TEXT("Understand the current Blueprint before editing it.")},
			  {TEXT("Add a variable to the Blueprint."), TEXT("blueprint.add_variable"), TEXT("manage_blueprint"), TEXT("add_variable"), TEXT(""), TEXT("Pick a clear variable name; the change applies only when you run the call.")},
			  {TEXT("Add a component through the SCS."), TEXT("blueprint.add_scs_component"), TEXT("manage_blueprint"), TEXT("add_scs_component"), TEXT(""), TEXT("Components are owned by the Simple Construction Script; review the component setup.")},
			  {TEXT("Compile the Blueprint."), TEXT("blueprint.compile"), TEXT("manage_blueprint"), TEXT("compile"), TEXT(""), TEXT("Compile to surface errors; read the compile result yourself before using the Blueprint.")} } },
		FMcpWorkflowPrompt{ TEXT("validation"), 1, TEXT("Validate project assets and level"),
			TEXT("Run read-only validation across the project, a specific asset, and the current level."),
			{ {TEXT("assetPath"), TEXT("content-path"), false, TEXT("Content path of an asset to focus on")} },
			{ {TEXT("Read the project context."), TEXT("inspect.get_project_settings"), TEXT("inspect"), TEXT("get_project_settings"), TEXT("ue://project"), TEXT("Confirm the project and engine version before running validation.")},
			  {TEXT("Run data validation across the project's assets."), TEXT("system_control.validate_assets"), TEXT("system_control"), TEXT("validate_assets"), TEXT("ue://capability/catalog"), TEXT("Validation is read-only; review each reported issue yourself.")},
			  {TEXT("Validate a specific asset."), TEXT("asset.validate"), TEXT("manage_asset"), TEXT("validate"), TEXT("ue://asset/{assetPath}"), TEXT("Use this to focus on one asset flagged by the project scan.")},
			  {TEXT("Validate the current level."), TEXT("manage_level.validate_level"), TEXT("manage_level"), TEXT("validate_level"), TEXT(""), TEXT("Level validation makes no changes; it only reports issues to review.")} } },
		FMcpWorkflowPrompt{ TEXT("sequence-render"), 1, TEXT("Render a level sequence"),
			TEXT("Prepare and run a Movie Render Queue job for a level sequence, reviewing each step."),
			{ {TEXT("sequencePath"), TEXT("content-path"), true, TEXT("Content path of the level sequence")},
			  {TEXT("outputFormat"), TEXT("enum"), false, TEXT("Render output image format"), {TEXT("png"), TEXT("jpeg"), TEXT("exr"), TEXT("custom")}} },
			{ {TEXT("Read the sequence properties."), TEXT("sequence.get_properties"), TEXT("manage_sequence"), TEXT("get_properties"), TEXT("ue://project"), TEXT("Confirm the sequence, frame range, and resolution before rendering.")},
			  {TEXT("Create a Movie Render Queue job."), TEXT("sequence.mrq.create_render_job"), TEXT("manage_sequence"), TEXT("create_render_job"), TEXT(""), TEXT("Creating a job does not render; it only prepares the queue entry.")},
			  {TEXT("Configure the render output settings."), TEXT("sequence.mrq.configure_output_settings"), TEXT("manage_sequence"), TEXT("configure_output_settings"), TEXT("ue://editor"), TEXT("Confirm the output directory and format yourself before rendering.")},
			  {TEXT("Queue the render job."), TEXT("sequence.mrq.queue_render"), TEXT("manage_sequence"), TEXT("queue_render"), TEXT(""), TEXT("Queuing stages the job; nothing is written until you start the render.")},
			  {TEXT("Start the render."), TEXT("sequence.mrq.start_render"), TEXT("manage_sequence"), TEXT("start_render"), TEXT(""), TEXT("Rendering can be long-running and writes files; you start it explicitly.")} } },
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
