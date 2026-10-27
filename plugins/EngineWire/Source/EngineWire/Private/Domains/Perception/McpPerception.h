// Copyright (c) 2024 EngineWire Contributors
#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

class AActor;
class FEditorViewportClient;
class FMcpBridgeWebSocket;
class UEngineWireSubsystem;
class UWorld;
namespace McpEnvironmentHandlers
{
struct FMcpViewCapture;
}

// inspect analyze_view: the level viewport's picture judged as a reviewer would, in numbers a model that cannot see it
// can act on: where the eye goes (saliency, and the share each actor draws), the palette and how its hues sit together,
// how the light falls (exposure, contrast, the key and fill on a subject, a colour cast) and what costs most to shade.
namespace McpPerception
{
/** The frame on a coarse grid of cells (block averages of the viewport's pixels): each cell's colour as shown (sRGB,
 *  0-1), its OKLab colour (L 0-1, a and b about -0.4 to 0.4) and the actor drawn at its centre (null for none). */
struct FFrame
{
    int32 Width = 0;
    int32 Height = 0;
    int32 Block = 1; // viewport pixels per cell side
    TArray<FLinearColor> Color;
    TArray<FVector3f> Lab;
    TArray<AActor*> Actor;
};

/** The frame of a viewport capture, on a grid of cells about 240 across (McpPerceptionAnalyze). */
FFrame BuildFrame(const McpEnvironmentHandlers::FMcpViewCapture& View);

/** OKLab (Ottosson 2020) of an sRGB colour (0-1, as displayed). */
FVector3f OkLab(const FLinearColor& Srgb);

/** Saliency by Boolean Map Saliency (Zhang and Sclaroff 2016) on OKLab: the share of the frame's saliency each actor
 *  draws (top 5), where the most salient cell is, and the subject's share and rank when named (McpPerceptionSaliency). */
TSharedPtr<FJsonObject> Saliency(const FFrame& Frame, const AActor* Subject);

/** The palette (OKLab k-means, 5 colours with shares), the best-fitting hue template of Cohen-Or et al. (2006) with how
 *  far the hues sit outside it, and the colour cast of near-neutral surfaces (McpPerceptionColor). A score only. */
TSharedPtr<FJsonObject> Palette(const FFrame& Frame);

/** Exposure (a lightness histogram, clipped and crushed shares, contrast, flat lighting) and, for a subject lit by the
 *  scene's sun, its key to fill ratio in stops from the lit and shadowed sides (McpPerceptionLight). Normals are world
 *  normals, one per cell (empty when there is no subject or no sun); ToSun points at the sun. */
TSharedPtr<FJsonObject> Light(const FFrame& Frame, const AActor* Subject, const TArray<FVector3f>& Normals, const FVector3f& ToSun);

/** How much of the frame, and of each actor, the shader complexity view draws at a high cost: the viewport is drawn
 *  once in that view mode and put back (McpPerceptionCost). */
TSharedPtr<FJsonObject> ShaderCost(FEditorViewportClient& Client, const FFrame& Frame);

/** describe_view compareWith: OutCamera names the camera the frame saved under that name was taken from, so the capture
 *  is taken from there (it stays Payload without compareWith). False with OutError and OutCode: VIEW_BASELINE_NOT_FOUND,
 *  or CAMERA_MOVED when the payload names another location or rotation (McpPerceptionDiff). */
bool PrepareViewDiff(const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FJsonObject>& OutCamera, FString& OutError, FString& OutCode);

/** describe_view saveAs and compareWith on a frame drawn as the game shows it: OutChange is what changed since the frame
 *  saved under compareWith (null without it), then the frame is kept under saveAs with the renderer's own noise, read
 *  from a second capture. False with OutError and OutCode: VIEW_SIZE_CHANGED, CAMERA_MOVED (another field of view), or
 *  a capture refusal. */
bool DiffView(const TSharedPtr<FJsonObject>& Payload, const McpEnvironmentHandlers::FMcpViewCapture& View,
              TSharedPtr<FJsonObject>& OutChange, FString& OutError, FString& OutCode);

/** audit_materials' pbr findings on one actor's cells (base colour as shown, sRGB; metallic and roughness 0-1): too_dark,
 *  too_bright, dark_metal, partial_metallic and mirror_roughness, each when a quarter of the cells (half for a mirror)
 *  shows it (McpPerceptionPbr). */
TArray<FString> PbrFindings(const TArray<FColor>& Base, const TArray<float>& Metal, const TArray<float>& Rough);

/** The actors in view as the G-buffer holds them: the viewport drawn in the Buffer Visualization view mode for base
 *  colour, metallic and roughness and put back; lit, opaque actors judged by PbrFindings, the others skipped. */
TSharedPtr<FJsonObject> PbrAsRendered(FEditorViewportClient& Client, const FFrame& Frame);

/** 1 when a slot's texel density is four times the view's median or more, -1 at a quarter or less, else 0. */
int32 TexelVerdict(double Ratio);

/** Texel density of each material slot of the static meshes in view, against the view's median (McpPerceptionTexel). */
TSharedPtr<FJsonObject> TexelDensity(const FFrame& Frame);

/** Groups of the level's static mesh actors drawing one mesh with one set of materials, worth instancing. */
TSharedPtr<FJsonObject> InstancingGroups(UWorld* World);

/** inspect audit_materials (McpPerceptionAudit). */
bool HandleAuditMaterials(UEngineWireSubsystem& Bridge, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
                          TSharedPtr<FMcpBridgeWebSocket> Socket);

/** inspect contact_sheet: an actor from several sides in one labelled image (McpPerceptionSheet). */
bool HandleContactSheet(UEngineWireSubsystem& Bridge, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
                        TSharedPtr<FMcpBridgeWebSocket> Socket);

/** inspect analyze_view. */
bool HandleAnalyzeView(UEngineWireSubsystem& Bridge, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
                       TSharedPtr<FMcpBridgeWebSocket> Socket);
} // namespace McpPerception
