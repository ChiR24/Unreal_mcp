// McpTool_ManageMaterialAuthoring.cpp — manage_material_authoring tool definition (38 actions)

#include "McpVersionCompatibility.h"
#include "MCP/McpToolDefinition.h"
#include "MCP/McpToolRegistry.h"
#include "MCP/McpSchemaBuilder.h"

class FMcpTool_ManageMaterialAuthoring : public FMcpToolDefinition
{
public:
	FString GetName() const override { return TEXT("manage_material_authoring"); }

	FString GetDescription() const override
	{
		return TEXT("Create materials with expressions, parameters, functions, "
			"instances, and landscape blend layers.");
	}

	FString GetCategory() const override { return TEXT("core"); }

	TSharedPtr<FJsonObject> BuildInputSchema() const override
	{
		return FMcpSchemaBuilder()
			.StringEnum(TEXT("action"), {
				TEXT("create_material"),
				TEXT("set_blend_mode"),
				TEXT("set_shading_model"),
				TEXT("set_material_domain"),
				TEXT("add_texture_sample"),
				TEXT("add_texture_coordinate"),
				TEXT("add_scalar_parameter"),
				TEXT("add_vector_parameter"),
				TEXT("add_static_switch_parameter"),
				TEXT("add_math_node"),
				TEXT("add_world_position"),
				TEXT("add_vertex_normal"),
				TEXT("add_pixel_depth"),
				TEXT("add_fresnel"),
				TEXT("add_reflection_vector"),
				TEXT("add_panner"),
				TEXT("add_rotator"),
				TEXT("add_noise"),
				TEXT("add_voronoi"),
				TEXT("add_if"),
				TEXT("add_switch"),
				TEXT("add_custom_expression"),
				TEXT("connect_nodes"),
				TEXT("disconnect_nodes"),
				TEXT("create_material_function"),
				TEXT("add_function_input"),
				TEXT("add_function_output"),
				TEXT("use_material_function"),
				TEXT("create_material_instance"),
				TEXT("set_scalar_parameter_value"),
				TEXT("set_vector_parameter_value"),
				TEXT("set_texture_parameter_value"),
				TEXT("create_landscape_material"),
				TEXT("create_decal_material"),
				TEXT("create_post_process_material"),
				TEXT("add_landscape_layer"),
				TEXT("configure_layer_blend"),
				TEXT("compile_material"),
				TEXT("get_material_info")
			}, TEXT("Material authoring action to perform"))
			.String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
			.String(TEXT("name"), TEXT("Name identifier."))
			.String(TEXT("path"), TEXT("Directory path for asset creation."))
			.StringEnum(TEXT("materialDomain"), {
				TEXT("Surface"),
				TEXT("DeferredDecal"),
				TEXT("LightFunction"),
				TEXT("Volume"),
				TEXT("PostProcess"),
				TEXT("UI")
			}, TEXT("Material domain type."))
			.StringEnum(TEXT("blendMode"), {
				TEXT("Opaque"),
				TEXT("Masked"),
				TEXT("Translucent"),
				TEXT("Additive"),
				TEXT("Modulate"),
				TEXT("AlphaComposite"),
				TEXT("AlphaHoldout")
			}, TEXT("Blend mode."))
			.StringEnum(TEXT("shadingModel"), {
				TEXT("DefaultLit"),
				TEXT("Unlit"),
				TEXT("Subsurface"),
				TEXT("SubsurfaceProfile"),
				TEXT("PreintegratedSkin"),
				TEXT("ClearCoat"),
				TEXT("Hair"),
				TEXT("Cloth"),
				TEXT("Eye"),
				TEXT("TwoSidedFoliage"),
				TEXT("ThinTranslucent")
			}, TEXT("Shading model."))
			.Bool(TEXT("twoSided"), TEXT("Enable two-sided rendering."))
			.Number(TEXT("x"), TEXT("Node X position."))
			.Number(TEXT("y"), TEXT("Node Y position."))
			.String(TEXT("texturePath"), TEXT("Texture asset path."))
			.StringEnum(TEXT("samplerType"), {
				TEXT("Color"),
				TEXT("LinearColor"),
				TEXT("Normal"),
				TEXT("Masks"),
				TEXT("Alpha"),
				TEXT("VirtualColor"),
				TEXT("VirtualNormal")
			}, TEXT("Texture sampler type."))
			.Number(TEXT("coordinateIndex"), TEXT("UV channel index (0-7)."))
			.Number(TEXT("uTiling"), TEXT("U tiling factor."))
			.Number(TEXT("vTiling"), TEXT("V tiling factor."))
			.String(TEXT("parameterName"), TEXT("Name of the parameter."))
			.FreeformObject(TEXT("defaultValue"),
				TEXT("Default value for parameter (number for scalar, object for vector, bool for switch)."))
			.String(TEXT("group"), TEXT("Group name."))
			.FreeformObject(TEXT("value"),
				TEXT("Value to set (number, vector object, or texture path)."))
			.StringEnum(TEXT("operation"), {
				TEXT("Add"),
				TEXT("Subtract"),
				TEXT("Multiply"),
				TEXT("Divide"),
				TEXT("Lerp"),
				TEXT("Clamp"),
				TEXT("Power"),
				TEXT("SquareRoot"),
				TEXT("Abs"),
				TEXT("Floor"),
				TEXT("Ceil"),
				TEXT("Frac"),
				TEXT("Sine"),
				TEXT("Cosine"),
				TEXT("Saturate"),
				TEXT("OneMinus"),
				TEXT("Min"),
				TEXT("Max"),
				TEXT("Dot"),
				TEXT("Cross"),
				TEXT("Normalize"),
				TEXT("Append")
			}, TEXT("Math operation type."))
			.Number(TEXT("constA"), TEXT("Constant A input value."))
			.Number(TEXT("constB"), TEXT("Constant B input value."))
			.String(TEXT("code"), TEXT("Code or expression."))
			.StringEnum(TEXT("outputType"), {
				TEXT("Float1"),
				TEXT("Float2"),
				TEXT("Float3"),
				TEXT("Float4"),
				TEXT("MaterialAttributes")
			}, TEXT("Output type of custom expression."))
			.String(TEXT("description"),
				TEXT("Description for custom expression or function."))
			.String(TEXT("sourceNodeId"), TEXT("Source node ID for connection."))
			.String(TEXT("sourcePin"), TEXT("Source pin name (output)."))
			.String(TEXT("targetNodeId"), TEXT("Target node ID for connection."))
			.String(TEXT("targetPin"), TEXT("Target pin name (input)."))
			.String(TEXT("nodeId"), TEXT("ID of the node."))
			.String(TEXT("pinName"), TEXT("Name of the pin."))
			.String(TEXT("functionPath"), TEXT("Path to function asset."))
			.Bool(TEXT("exposeToLibrary"),
				TEXT("Expose function to material library."))
			.String(TEXT("inputName"), TEXT("Name of the input."))
			.StringEnum(TEXT("inputType"), {
				TEXT("Float1"),
				TEXT("Float2"),
				TEXT("Float3"),
				TEXT("Float4"),
				TEXT("Texture2D"),
				TEXT("TextureCube"),
				TEXT("Bool"),
				TEXT("MaterialAttributes")
			}, TEXT("Type of function input/output."))
			.String(TEXT("parentMaterial"),
				TEXT("Path to parent material for instances."))
			.String(TEXT("layerName"), TEXT("Name of the layer."))
			.StringEnum(TEXT("blendType"), {
				TEXT("LB_WeightBlend"),
				TEXT("LB_AlphaBlend"),
				TEXT("LB_HeightBlend")
			}, TEXT("Landscape layer blend type."))
			.ArrayOfObjects(TEXT("layers"),
				TEXT("Array of layer configurations for layer blend."))
			.Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))
			.Required({TEXT("action")})
			.Build();
	}
};

MCP_REGISTER_TOOL(FMcpTool_ManageMaterialAuthoring);
