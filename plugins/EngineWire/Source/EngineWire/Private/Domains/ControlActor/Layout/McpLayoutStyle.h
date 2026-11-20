#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Templates/Function.h"

class AActor;
class FMcpBridgeWebSocket;
class UEngineWireSubsystem;

// control_actor learn_layout_style and suggest_companions (S3): how this project's levels place props around each
// other, learned from the levels themselves (no genre recipe), then used to propose what goes with a prop and where.
// Plain counts: per pair of meshes and relation, how often, how far apart, in which direction and turned how.
namespace McpLayoutStyle
{
constexpr int32 DistanceBins = 10;
constexpr double DistanceBinCm = 40.0; // 0-400 cm, the last bin holds everything further
constexpr int32 AngleBins = 8;         // 45 degree bins centred on 0, 45, 90 ...

/** One seen pair: Other stands in relation Kind (on, nextTo, near) to Anchor. */
struct FObservation
{
    FString Anchor; // mesh paths
    FString Other;
    FString Kind;
    double Distance = 0.0; // cm between their box centres, seen from above
    double Bearing = 0.0;  // degrees: where Other stands, in the anchor's turned frame
    double Yaw = 0.0;      // degrees: Other's turn relative to the anchor's
};

struct FPairStats
{
    int32 Count = 0;
    int32 Distance[DistanceBins] = {};
    int32 Bearing[AngleBins] = {};
    int32 Yaw[AngleBins] = {};
};

/** One level's counts: copies of each mesh, and the pairs keyed "anchor|other|kind". */
struct FLevelStats
{
    TMap<FString, int32> Copies;
    TMap<FString, FPairStats> Pairs;
};

struct FSuggestion
{
    FString Mesh;
    FString Kind;
    double PerAnchor = 0.0; // how many go with one copy of the anchor, on average
    double Distance = 0.0;
    double Bearing = 0.0;
    double Yaw = 0.0;
};

int32 DistanceBin(double Distance);
int32 AngleBin(double Degrees);
void Add(FLevelStats& Stats, const FObservation& Seen);

/** What goes with AnchorMesh across the levels, best first, one row per mesh (its most common relation), at most Count;
 *  a mesh seen with fewer than one anchor in ten is left out. */
TArray<FSuggestion> Suggest(const TArray<FLevelStats>& Levels, const FString& AnchorMesh, int32 Count);

TSharedPtr<FJsonObject> ToJson(const FLevelStats& Stats);
FLevelStats FromJson(const TSharedPtr<FJsonObject>& Json);

bool HandleLearn(UEngineWireSubsystem* Bridge, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
                 TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleSuggest(UEngineWireSubsystem* Bridge, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
                   TSharedPtr<FMcpBridgeWebSocket> Socket, TFunctionRef<AActor*(const FString&)> Find);
} // namespace McpLayoutStyle
