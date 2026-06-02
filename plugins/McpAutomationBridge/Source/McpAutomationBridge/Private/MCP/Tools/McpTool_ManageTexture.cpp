// McpTool_ManageTexture.cpp — manage_texture tool definition (21 actions)

#include "McpVersionCompatibility.h"
#include "MCP/McpToolDefinition.h"
#include "MCP/McpToolRegistry.h"
#include "MCP/McpSchemaBuilder.h"

class FMcpTool_ManageTexture : public FMcpToolDefinition
{
public:
	FString GetName() const override { return TEXT("manage_texture"); }

	FString GetDescription() const override
	{
		return TEXT("Create procedural textures, process images, bake normal/AO maps, "
			"and set compression settings.");
	}

	FString GetCategory() const override { return TEXT("core"); }

	TSharedPtr<FJsonObject> BuildInputSchema() const override
	{
		return FMcpSchemaBuilder()
			.StringEnum(TEXT("action"), {
				TEXT("create_noise_texture"),
				TEXT("create_gradient_texture"),
				TEXT("create_pattern_texture"),
				TEXT("create_normal_from_height"),
				TEXT("create_ao_from_mesh"),
				TEXT("resize_texture"),
				TEXT("adjust_levels"),
				TEXT("adjust_curves"),
				TEXT("blur"),
				TEXT("sharpen"),
				TEXT("invert"),
				TEXT("desaturate"),
				TEXT("channel_pack"),
				TEXT("channel_extract"),
				TEXT("combine_textures"),
				TEXT("set_compression_settings"),
				TEXT("set_texture_group"),
				TEXT("set_lod_bias"),
				TEXT("configure_virtual_texture"),
				TEXT("set_streaming_priority"),
				TEXT("get_texture_info")
			}, TEXT("Texture action to perform"))
			.String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
			.String(TEXT("name"), TEXT("Name identifier."))
			.String(TEXT("path"), TEXT("Directory path for asset creation."))
			.Number(TEXT("width"), TEXT("Width value."))
			.Number(TEXT("height"), TEXT("Height value."))
			.Number(TEXT("newWidth"), TEXT("New width for resize operation."))
			.Number(TEXT("newHeight"), TEXT("New height for resize operation."))
			.StringEnum(TEXT("noiseType"), {
				TEXT("Perlin"),
				TEXT("Simplex"),
				TEXT("Worley"),
				TEXT("Voronoi")
			}, TEXT("Type of noise to generate."))
			.Number(TEXT("scale"), TEXT("Noise scale/frequency."))
			.Number(TEXT("octaves"), TEXT("Number of noise octaves for FBM."))
			.Number(TEXT("persistence"), TEXT("Amplitude falloff per octave."))
			.Number(TEXT("lacunarity"), TEXT("Frequency multiplier per octave."))
			.Number(TEXT("seed"), TEXT("Random seed for procedural generation."))
			.Bool(TEXT("seamless"), TEXT("Generate seamless/tileable texture."))
			.StringEnum(TEXT("gradientType"), {
				TEXT("Linear"),
				TEXT("Radial"),
				TEXT("Angular")
			}, TEXT("Type of gradient."))
			.FreeformObject(TEXT("startColor"), TEXT("Start color {r, g, b, a}."))
			.FreeformObject(TEXT("endColor"), TEXT("End color {r, g, b, a}."))
			.Number(TEXT("angle"), TEXT("Angle in degrees."))
			.Number(TEXT("centerX"),
				TEXT("Center X position (0-1) for radial gradient."))
			.Number(TEXT("centerY"),
				TEXT("Center Y position (0-1) for radial gradient."))
			.Number(TEXT("radius"), TEXT("Radius value."))
			.ArrayOfObjects(TEXT("colorStops"),
				TEXT("Array of {position, color} for multi-color gradients."))
			.StringEnum(TEXT("patternType"), {
				TEXT("Checker"),
				TEXT("Grid"),
				TEXT("Brick"),
				TEXT("Tile"),
				TEXT("Dots"),
				TEXT("Stripes")
			}, TEXT("Type of pattern."))
			.FreeformObject(TEXT("primaryColor"),
				TEXT("Primary pattern color {r, g, b, a}."))
			.FreeformObject(TEXT("secondaryColor"),
				TEXT("Secondary pattern color {r, g, b, a}."))
			.Number(TEXT("tilesX"),
				TEXT("Number of pattern tiles horizontally."))
			.Number(TEXT("tilesY"),
				TEXT("Number of pattern tiles vertically."))
			.Number(TEXT("lineWidth"), TEXT("Line width for grid/stripes (0-1)."))
			.Number(TEXT("brickRatio"),
				TEXT("Width/height ratio for brick pattern."))
			.Number(TEXT("offset"), TEXT("Brick offset ratio (0-1)."))
			.String(TEXT("sourceTexture"),
				TEXT("Source height map texture path."))
			.Number(TEXT("strength"), TEXT("Strength or weight."))
			.StringEnum(TEXT("algorithm"), {
				TEXT("Sobel"),
				TEXT("Prewitt"),
				TEXT("Scharr")
			}, TEXT("Normal calculation algorithm."))
			.Bool(TEXT("flipY"),
				TEXT("Flip green channel for DirectX/OpenGL compatibility."))
			.String(TEXT("meshPath"), TEXT("Mesh asset path."))
			.Number(TEXT("samples"), TEXT("Number of AO samples."))
			.Number(TEXT("rayDistance"), TEXT("Maximum ray distance for AO."))
			.Number(TEXT("bias"), TEXT("AO bias to prevent self-occlusion."))
			.Number(TEXT("uvChannel"), TEXT("UV channel to use for baking."))
			.StringEnum(TEXT("filterMethod"), {
				TEXT("Nearest"),
				TEXT("Bilinear"),
				TEXT("Bicubic"),
				TEXT("Lanczos")
			}, TEXT("Resize filter method."))
			.Bool(TEXT("preserveAspect"),
				TEXT("Preserve aspect ratio when resizing."))
			.String(TEXT("outputPath"), TEXT("Output file or directory path."))
			.Number(TEXT("inputBlackPoint"), TEXT("Input black point (0-1)."))
			.Number(TEXT("inputWhitePoint"), TEXT("Input white point (0-1)."))
			.Number(TEXT("gamma"), TEXT("Gamma correction value."))
			.Number(TEXT("outputBlackPoint"), TEXT("Output black point (0-1)."))
			.Number(TEXT("outputWhitePoint"), TEXT("Output white point (0-1)."))
			.ArrayOfObjects(TEXT("curvePoints"),
				TEXT("Array of {x, y} curve control points."))
			.StringEnum(TEXT("blurType"), {
				TEXT("Gaussian"),
				TEXT("Box"),
				TEXT("Radial")
			}, TEXT("Type of blur."))
			.StringEnum(TEXT("sharpenType"), {
				TEXT("UnsharpMask"),
				TEXT("Laplacian")
			}, TEXT("Type of sharpening."))
			.StringEnum(TEXT("channel"), {
				TEXT("All"),
				TEXT("Red"),
				TEXT("Green"),
				TEXT("Blue"),
				TEXT("Alpha")
			}, TEXT("Target channel."))
			.Bool(TEXT("invertAlpha"),
				TEXT("Whether to invert alpha channel."))
			.Number(TEXT("amount"), TEXT("Effect amount (0-1 for desaturate)."))
			.StringEnum(TEXT("method"), {
				TEXT("Luminance"),
				TEXT("Average"),
				TEXT("Lightness")
			}, TEXT("Desaturation method."))
			.Bool(TEXT("outputAsGrayscale"),
				TEXT("Output extracted channel as grayscale."))
			.String(TEXT("redChannel"),
				TEXT("Source texture for red channel."))
			.String(TEXT("greenChannel"),
				TEXT("Source texture for green channel."))
			.String(TEXT("blueChannel"),
				TEXT("Source texture for blue channel."))
			.String(TEXT("alphaChannel"),
				TEXT("Source texture for alpha channel."))
			.StringEnum(TEXT("redSourceChannel"), {
				TEXT("Red"), TEXT("Green"), TEXT("Blue"), TEXT("Alpha")
			}, TEXT("Which channel to use from red source."))
			.StringEnum(TEXT("greenSourceChannel"), {
				TEXT("Red"), TEXT("Green"), TEXT("Blue"), TEXT("Alpha")
			}, TEXT("Which channel to use from green source."))
			.StringEnum(TEXT("blueSourceChannel"), {
				TEXT("Red"), TEXT("Green"), TEXT("Blue"), TEXT("Alpha")
			}, TEXT("Which channel to use from blue source."))
			.StringEnum(TEXT("alphaSourceChannel"), {
				TEXT("Red"), TEXT("Green"), TEXT("Blue"), TEXT("Alpha")
			}, TEXT("Which channel to use from alpha source."))
			.String(TEXT("baseTexture"),
				TEXT("Base texture path for combining."))
			.String(TEXT("blendTexture"),
				TEXT("Blend texture path for combining."))
			.StringEnum(TEXT("blendMode"), {
				TEXT("Multiply"),
				TEXT("Add"),
				TEXT("Subtract"),
				TEXT("Screen"),
				TEXT("Overlay"),
				TEXT("SoftLight"),
				TEXT("HardLight"),
				TEXT("Difference"),
				TEXT("Normal")
			}, TEXT("Blend mode for combining textures."))
			.Number(TEXT("opacity"), TEXT("Blend opacity (0-1)."))
			.String(TEXT("maskTexture"),
				TEXT("Optional mask texture for blending."))
			.StringEnum(TEXT("compressionSettings"), {
				TEXT("TC_Default"),
				TEXT("TC_Normalmap"),
				TEXT("TC_Masks"),
				TEXT("TC_Grayscale"),
				TEXT("TC_Displacementmap"),
				TEXT("TC_VectorDisplacementmap"),
				TEXT("TC_HDR"),
				TEXT("TC_EditorIcon"),
				TEXT("TC_Alpha"),
				TEXT("TC_DistanceFieldFont"),
				TEXT("TC_HDR_Compressed"),
				TEXT("TC_BC7")
			}, TEXT("Texture compression setting."))
			.String(TEXT("textureGroup"),
				TEXT("Texture group (TEXTUREGROUP_World, TEXTUREGROUP_Character, "
					"TEXTUREGROUP_UI, etc.)."))
			.Number(TEXT("lodBias"),
				TEXT("LOD bias (-2 to 4, lower = higher quality)."))
			.Bool(TEXT("virtualTextureStreaming"),
				TEXT("Enable virtual texture streaming."))
			.Number(TEXT("tileSize"),
				TEXT("Virtual texture tile size (32, 64, 128, 256, 512, 1024)."))
			.Number(TEXT("tileBorderSize"),
				TEXT("Virtual texture tile border size."))
			.Bool(TEXT("neverStream"), TEXT("Disable texture streaming."))
			.Number(TEXT("streamingPriority"),
				TEXT("Streaming priority (-1 to 1, lower = higher priority)."))
			.Bool(TEXT("hdr"), TEXT("Create HDR texture (16-bit float)."))
			.Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))
			.Required({TEXT("action")})
			.Build();
	}
};

MCP_REGISTER_TOOL(FMcpTool_ManageTexture);
