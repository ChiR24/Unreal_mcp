#include "McpRuntimeReads.h"

#include "Dom/JsonValue.h"
#include "DynamicRHI.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "Misc/App.h"
#include "Misc/DateTime.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"
#if __has_include("RenderTimer.h")
#include "RenderTimer.h" // GGameThreadTime and GRenderThreadTime moved out of RenderCore.h
#else
#include "RenderCore.h"
#endif

extern ENGINE_API float GAverageFPS; // exported by the engine; its public declaration left UnrealEngine.h

namespace McpRuntimeReadsDetail
{
// The running game's world; in an editor (the automation test), its editor world.
UWorld* RuntimeWorld()
{
    UWorld* Fallback = nullptr;
    if (!GEngine)
    {
        return nullptr;
    }
    for (const FWorldContext& Context : GEngine->GetWorldContexts())
    {
        if (Context.WorldType == EWorldType::Game || Context.WorldType == EWorldType::PIE)
        {
            return Context.World();
        }
        Fallback = Context.WorldType == EWorldType::Editor ? Context.World() : Fallback;
    }
    return Fallback;
}

FString LabelOf(const AActor& Actor)
{
#if WITH_EDITOR
    return Actor.GetActorLabel();
#else
    return Actor.GetName();
#endif
}

TArray<TSharedPtr<FJsonValue>> Vector(const FVector& Value)
{
    return {MakeShared<FJsonValueNumber>(Value.X), MakeShared<FJsonValueNumber>(Value.Y), MakeShared<FJsonValueNumber>(Value.Z)};
}

TSharedPtr<FJsonObject> ListActors(UWorld& World, const TSharedPtr<FJsonObject>& Params)
{
    FString ClassFilter;
    int32 Limit = 200;
    Params->TryGetStringField(TEXT("classFilter"), ClassFilter);
    Params->TryGetNumberField(TEXT("limit"), Limit);
    Limit = FMath::Clamp(Limit, 1, 1000);
    TArray<TSharedPtr<FJsonValue>> Rows;
    int32 Total = 0;
    for (TActorIterator<AActor> It(&World); It; ++It)
    {
        if (!ClassFilter.IsEmpty() && !It->GetClass()->GetName().Contains(ClassFilter))
        {
            continue;
        }
        if (++Total <= Limit)
        {
            const TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
            Row->SetStringField(TEXT("label"), LabelOf(**It));
            Row->SetStringField(TEXT("name"), It->GetName());
            Row->SetStringField(TEXT("class"), It->GetClass()->GetName());
            Row->SetArrayField(TEXT("location"), Vector(It->GetActorLocation()));
            Rows.Add(MakeShared<FJsonValueObject>(Row));
        }
    }
    const TSharedPtr<FJsonObject> Out = MakeShared<FJsonObject>();
    Out->SetArrayField(TEXT("actors"), Rows);
    Out->SetNumberField(TEXT("totalCount"), Total);
    Out->SetStringField(TEXT("world"), World.GetName());
    return Out;
}

TSharedPtr<FJsonObject> GetTransform(UWorld& World, const TSharedPtr<FJsonObject>& Params, FString& OutError)
{
    FString Name;
    Params->TryGetStringField(TEXT("actorName"), Name);
    for (TActorIterator<AActor> It(&World); It && !Name.IsEmpty(); ++It)
    {
        if (LabelOf(**It) == Name || It->GetName() == Name)
        {
            const TSharedPtr<FJsonObject> Out = MakeShared<FJsonObject>();
            Out->SetStringField(TEXT("actorName"), It->GetName());
            Out->SetArrayField(TEXT("location"), Vector(It->GetActorLocation()));
            const FRotator Rotation = It->GetActorRotation();
            Out->SetArrayField(TEXT("rotation"), Vector(FVector(Rotation.Pitch, Rotation.Yaw, Rotation.Roll)));
            Out->SetArrayField(TEXT("scale"), Vector(It->GetActorScale3D()));
            Out->SetArrayField(TEXT("velocity"), Vector(It->GetVelocity()));
            return Out;
        }
    }
    OutError = FString::Printf(TEXT("ACTOR_NOT_FOUND: no actor labelled or named '%s' in %s (runtime.list_actors lists them)."),
                               *Name, *World.GetName());
    return nullptr;
}

TSharedPtr<FJsonObject> FrameCost()
{
    const TSharedPtr<FJsonObject> Out = MakeShared<FJsonObject>();
    Out->SetNumberField(TEXT("frameMs"), FApp::GetDeltaTime() * 1000.0);
    Out->SetNumberField(TEXT("averageFps"), GAverageFPS);
    Out->SetNumberField(TEXT("gameThreadMs"), FPlatformTime::ToMilliseconds(GGameThreadTime));
    Out->SetNumberField(TEXT("renderThreadMs"), FPlatformTime::ToMilliseconds(GRenderThreadTime));
    Out->SetNumberField(TEXT("gpuMs"), FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles()));
    return Out;
}

TSharedPtr<FJsonObject> Screenshot()
{
    const FString Path = FPaths::Combine(FPaths::ScreenShotDir(),
        FString::Printf(TEXT("EngineWireRuntime_%s.png"), *FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S_%s"))));
    FScreenshotRequest::RequestScreenshot(Path, false, false);
    const TSharedPtr<FJsonObject> Out = MakeShared<FJsonObject>();
    Out->SetStringField(TEXT("path"), FPaths::ConvertRelativePathToFull(Path));
    Out->SetStringField(TEXT("note"), TEXT("Written when the next frame is drawn."));
    return Out;
}
}

const TArray<FString>& McpRuntimeCapabilities()
{
    static const TArray<FString> Names = {TEXT("runtime.list_actors"), TEXT("runtime.get_transform"), TEXT("runtime.frame_cost"),
                                          TEXT("runtime.screenshot")};
    return Names;
}

TSharedPtr<FJsonObject> McpRuntimeRead(const FString& Capability, const TSharedPtr<FJsonObject>& Params, FString& OutError)
{
    using namespace McpRuntimeReadsDetail;
    const TSharedPtr<FJsonObject> Args = Params.IsValid() ? Params : MakeShared<FJsonObject>();
    UWorld* World = RuntimeWorld();
    if (!World && Capability != TEXT("runtime.frame_cost"))
    {
        OutError = TEXT("NO_WORLD: no game world is running.");
        return nullptr;
    }
    if (Capability == TEXT("runtime.list_actors")) return ListActors(*World, Args);
    if (Capability == TEXT("runtime.get_transform")) return GetTransform(*World, Args, OutError);
    if (Capability == TEXT("runtime.frame_cost")) return FrameCost();
    if (Capability == TEXT("runtime.screenshot")) return Screenshot();
    OutError = FString::Printf(TEXT("UNKNOWN_CAPABILITY: '%s'; this runtime serves %s."), *Capability,
                               *FString::Join(McpRuntimeCapabilities(), TEXT(", ")));
    return nullptr;
}
