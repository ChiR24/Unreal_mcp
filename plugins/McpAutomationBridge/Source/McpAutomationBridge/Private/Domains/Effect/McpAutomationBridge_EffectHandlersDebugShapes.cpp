#include "Core/Compatibility/McpVersionCompatibility.h"

#include "Domains/Effect/McpAutomationBridge_EffectHandlersPrivate.h"

#include "DrawDebugHelpers.h"

#if WITH_EDITOR
#include "Components/LineBatchComponent.h"
#include "Editor.h"
#include "Engine/World.h"
#endif

namespace McpEffectHandlers
{
#if WITH_EDITOR
static void AppendLineBatcherStatus(
    const TCHAR* Label,
    const ULineBatchComponent* Batcher,
    TArray<TSharedPtr<FJsonValue>>& OutBatchers,
    int32& Lines,
    int32& Points,
    int32& Meshes)
{
    if (!Batcher)
    {
        return;
    }
    TSharedPtr<FJsonObject> Entry = McpHandlerUtils::CreateResultObject();
    Entry->SetStringField(TEXT("batcher"), Label);
    Entry->SetNumberField(TEXT("lines"), Batcher->BatchedLines.Num());
    Entry->SetNumberField(TEXT("points"), Batcher->BatchedPoints.Num());
    Entry->SetNumberField(TEXT("meshes"), Batcher->BatchedMeshes.Num());
    OutBatchers.Add(MakeShared<FJsonValueObject>(Entry));
    Lines += Batcher->BatchedLines.Num();
    Points += Batcher->BatchedPoints.Num();
    Meshes += Batcher->BatchedMeshes.Num();
}
#endif

static bool DrawShape(
    const FEffectActionContext& Context,
    const FString& ShapeType,
    const FVector& Location,
    float Size,
    float Duration,
    float Thickness,
    const FColor& Color)
{
#if WITH_EDITOR
    if (!GEditor)
    {
        TSharedPtr<FJsonObject> Response = McpHandlerUtils::CreateResultObject();
        Response->SetBoolField(TEXT("success"), false);
        Response->SetStringField(TEXT("error"), TEXT("Editor not available for debug drawing"));
        Context.Bridge.SendAutomationResponse(
            Context.Socket, Context.RequestId, false, TEXT("Editor not available"),
            Response, TEXT("EDITOR_NOT_AVAILABLE"));
        return true;
    }

    UWorld* World = GetEditorWorld();
    if (!World)
    {
        TSharedPtr<FJsonObject> Response = McpHandlerUtils::CreateResultObject();
        Response->SetBoolField(TEXT("success"), false);
        Response->SetStringField(TEXT("error"), TEXT("No world available for debug drawing"));
        Context.Bridge.SendAutomationResponse(
            Context.Socket, Context.RequestId, false, TEXT("No world available"),
            Response, TEXT("NO_WORLD"));
        return true;
    }

    const FString LowerShapeType = ShapeType.ToLower();
    if (LowerShapeType == TEXT("sphere"))
    {
        DrawDebugSphere(World, Location, Size, 16, Color, false, Duration, 0, Thickness);
    }
    else if (LowerShapeType == TEXT("box"))
    {
        FVector BoxSize = FVector(Size);
        const TArray<TSharedPtr<FJsonValue>>* BoxSizeValues = nullptr;
        if (Context.Payload->TryGetArrayField(TEXT("boxSize"), BoxSizeValues) &&
            BoxSizeValues && BoxSizeValues->Num() >= 3)
        {
            BoxSize = FVector(
                static_cast<float>((*BoxSizeValues)[0]->AsNumber()),
                static_cast<float>((*BoxSizeValues)[1]->AsNumber()),
                static_cast<float>((*BoxSizeValues)[2]->AsNumber()));
        }
        DrawDebugBox(World, Location, BoxSize, FRotator::ZeroRotator.Quaternion(), Color, false, Duration, 0, Thickness);
    }
    else if (LowerShapeType == TEXT("circle"))
    {
        DrawDebugCircle(World, Location, Size, 32, Color, false, Duration, 0, Thickness, FVector::UpVector);
    }
    else if (LowerShapeType == TEXT("line"))
    {
        const FVector EndLocation = ReadVectorField(Context.Payload, TEXT("endLocation"), Location + FVector(100, 0, 0));
        DrawDebugLine(World, Location, EndLocation, Color, false, Duration, 0, Thickness);
    }
    else if (LowerShapeType == TEXT("point"))
    {
        DrawDebugPoint(World, Location, Size, Color, false, Duration);
    }
    else if (LowerShapeType == TEXT("coordinate"))
    {
        DrawDebugCoordinateSystem(World, Location, ReadRotatorField(Context.Payload, TEXT("rotation")), Size, false, Duration, 0, Thickness);
    }
    else if (LowerShapeType == TEXT("cylinder"))
    {
        const FVector EndLocation = ReadVectorField(Context.Payload, TEXT("endLocation"), Location + FVector(0, 0, 100));
        DrawDebugCylinder(World, Location, EndLocation, Size, 16, Color, false, Duration, 0, Thickness);
    }
    else if (LowerShapeType == TEXT("cone"))
    {
        const FVector Direction = ReadVectorField(Context.Payload, TEXT("direction"), FVector::UpVector);
        float Length = Context.Payload->HasField(TEXT("length"))
            ? static_cast<float>(GetJsonNumberField(Context.Payload, TEXT("length")))
            : Size * 2.0f;
        float AngleWidth = FMath::DegreesToRadians(45.0f);
        float AngleHeight = FMath::DegreesToRadians(45.0f);
        if (Context.Payload->HasField(TEXT("angle")))
        {
            const float AngleDeg = static_cast<float>(GetJsonNumberField(Context.Payload, TEXT("angle")));
            AngleWidth = AngleHeight = FMath::DegreesToRadians(AngleDeg);
        }
        DrawDebugCone(World, Location, Direction, Length, AngleWidth, AngleHeight, 16, Color, false, Duration, 0, Thickness);
    }
    else if (LowerShapeType == TEXT("capsule"))
    {
        float HalfHeight = Context.Payload->HasField(TEXT("halfHeight"))
            ? static_cast<float>(GetJsonNumberField(Context.Payload, TEXT("halfHeight")))
            : Size;
        DrawDebugCapsule(World, Location, HalfHeight, Size, ReadRotatorField(Context.Payload, TEXT("rotation")).Quaternion(), Color, false, Duration, 0, Thickness);
    }
    else if (LowerShapeType == TEXT("arrow"))
    {
        const FVector EndLocation = ReadVectorField(Context.Payload, TEXT("endLocation"), Location + FVector(100, 0, 0));
        DrawDebugDirectionalArrow(World, Location, EndLocation, Size > 0 ? Size : 10.0f, Color, false, Duration, 0, Thickness);
    }
    else if (LowerShapeType == TEXT("plane"))
    {
        DrawDebugBox(World, Location, FVector(Size, Size, 1.0f), ReadRotatorField(Context.Payload, TEXT("rotation")).Quaternion(), Color, false, Duration, 0, Thickness);
    }
    else
    {
        TSharedPtr<FJsonObject> Response = McpHandlerUtils::CreateResultObject();
        Response->SetBoolField(TEXT("success"), false);
        Response->SetStringField(TEXT("error"), FString::Printf(TEXT("Unsupported shape type: %s"), *ShapeType));
        Response->SetStringField(TEXT("supportedShapes"), TEXT("sphere, box, circle, line, point, arrow, capsule, cylinder, cone, coordinate, plane"));
        Context.Bridge.SendAutomationResponse(
            Context.Socket, Context.RequestId, false, TEXT("Unsupported shape type"),
            Response, TEXT("UNSUPPORTED_SHAPE"));
        return true;
    }

    TSharedPtr<FJsonObject> Response = McpHandlerUtils::CreateResultObject();
    Response->SetBoolField(TEXT("success"), true);
    Response->SetStringField(TEXT("shapeType"), ShapeType);
    Response->SetStringField(TEXT("location"), FString::Printf(TEXT("%.2f,%.2f,%.2f"), Location.X, Location.Y, Location.Z));
    Response->SetNumberField(TEXT("duration"), Duration);
    // Non-realtime editor viewports only show batched debug lines after a redraw (dogfood #109).
    if (GEditor) { GEditor->RedrawLevelEditingViewports(false); }
    Context.Bridge.SendAutomationResponse(
        Context.Socket, Context.RequestId, true, TEXT("Debug shape drawn"), Response);
    return true;
#else
    TSharedPtr<FJsonObject> Response = McpHandlerUtils::CreateResultObject();
    Response->SetBoolField(TEXT("success"), false);
    Response->SetStringField(TEXT("error"), TEXT("Debug shape drawing requires editor build"));
    Response->SetStringField(TEXT("shapeType"), ShapeType);
    Context.Bridge.SendAutomationResponse(
        Context.Socket, Context.RequestId, false,
        TEXT("Debug shape drawing not available in non-editor build"),
        Response, TEXT("NOT_AVAILABLE"));
    return true;
#endif
}

bool HandleEffectDiscoveryAction(const FEffectActionContext& Context)
{
    if (Context.Lower.Equals(TEXT("list_debug_shapes")))
    {
        TArray<TSharedPtr<FJsonValue>> Shapes;
        for (const TCHAR* Shape : {
                 TEXT("sphere"), TEXT("box"), TEXT("circle"), TEXT("line"),
                 TEXT("point"), TEXT("coordinate"), TEXT("cylinder"),
                 TEXT("cone"), TEXT("capsule"), TEXT("arrow"), TEXT("plane")})
        {
            Shapes.Add(MakeShared<FJsonValueString>(Shape));
        }
        // The catalogue of drawable types plus what is actually batched right now
        // (dogfood #103): the world/persistent/foreground line batchers are the
        // only place the engine keeps debug primitives, so their buffers are the
        // live count. Batched primitives carry no shape names, hence counts.
        TSharedPtr<FJsonObject> Response = McpHandlerUtils::CreateResultObject();
        Response->SetArrayField(TEXT("shapeTypes"), Shapes);
        Response->SetArrayField(TEXT("shapes"), Shapes);
        Response->SetNumberField(TEXT("count"), Shapes.Num());
        int32 Lines = 0;
        int32 Points = 0;
        int32 Meshes = 0;
        TArray<TSharedPtr<FJsonValue>> Batchers;
        bool bWorldAvailable = false;
#if WITH_EDITOR
        if (UWorld* World = GetEditorWorld())
        {
            bWorldAvailable = true;
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 6
            AppendLineBatcherStatus(TEXT("World"), World->GetLineBatcher(UWorld::ELineBatcherType::World), Batchers, Lines, Points, Meshes);
            AppendLineBatcherStatus(TEXT("WorldPersistent"), World->GetLineBatcher(UWorld::ELineBatcherType::WorldPersistent), Batchers, Lines, Points, Meshes);
            AppendLineBatcherStatus(TEXT("Foreground"), World->GetLineBatcher(UWorld::ELineBatcherType::Foreground), Batchers, Lines, Points, Meshes);
#else
            AppendLineBatcherStatus(TEXT("World"), World->LineBatcher, Batchers, Lines, Points, Meshes);
            AppendLineBatcherStatus(TEXT("WorldPersistent"), World->PersistentLineBatcher, Batchers, Lines, Points, Meshes);
            AppendLineBatcherStatus(TEXT("Foreground"), World->ForegroundLineBatcher, Batchers, Lines, Points, Meshes);
#endif
        }
#endif
        TSharedPtr<FJsonObject> Active = McpHandlerUtils::CreateResultObject();
        Active->SetNumberField(TEXT("lines"), Lines); Active->SetNumberField(TEXT("points"), Points); Active->SetNumberField(TEXT("meshes"), Meshes);
        Active->SetNumberField(TEXT("total"), Lines + Points + Meshes); Active->SetBoolField(TEXT("worldAvailable"), bWorldAvailable);
        Response->SetObjectField(TEXT("active"), Active); Response->SetArrayField(TEXT("batchers"), Batchers);
        Response->SetNumberField(TEXT("activeCount"), Lines + Points + Meshes);
        Context.Bridge.SendAutomationResponse(
            Context.Socket, Context.RequestId, true,
            TEXT("Debug shape status retrieved"), Response);
        return true;
    }

    if (!Context.Lower.Equals(TEXT("clear_debug_shapes")))
    {
        return false;
    }

#if WITH_EDITOR
    if (GEditor && GetEditorWorld())
    {
        FlushPersistentDebugLines(GetEditorWorld());
        TSharedPtr<FJsonObject> Response = McpHandlerUtils::CreateResultObject();
        Response->SetBoolField(TEXT("success"), true);
        Context.Bridge.SendAutomationResponse(
            Context.Socket, Context.RequestId, true,
            TEXT("Debug shapes cleared"), Response);
        return true;
    }
    Context.Bridge.SendAutomationResponse(
        Context.Socket, Context.RequestId, false,
        TEXT("Editor world not available"), nullptr, TEXT("NO_WORLD"));
#else
    Context.Bridge.SendAutomationResponse(
        Context.Socket, Context.RequestId, false,
        TEXT("Debug shape clearing requires editor build"), nullptr,
        TEXT("NOT_IMPLEMENTED"));
#endif
    return true;
}

bool HandleDrawDebugShape(const FEffectActionContext& Context)
{
    FString ShapeType = TEXT("sphere");
    Context.Payload->TryGetStringField(TEXT("shapeType"), ShapeType);
    if (ShapeType.Equals(TEXT("sphere"), ESearchCase::IgnoreCase) && Context.Payload->HasField(TEXT("shape")))
    {
        Context.Payload->TryGetStringField(TEXT("shape"), ShapeType);
    }
    if (!Context.Payload->HasField(TEXT("location")))
    {
        TSharedPtr<FJsonObject> Response = McpHandlerUtils::CreateResultObject();
        Response->SetBoolField(TEXT("success"), false);
        Response->SetStringField(TEXT("error"), TEXT("location parameter is required for debug_shape"));
        Context.Bridge.SendAutomationResponse(
            Context.Socket, Context.RequestId, false,
            TEXT("Missing required parameter: location"), Response, TEXT("INVALID_ARGUMENT"));
        return true;
    }
    const float Duration = Context.Payload->HasField(TEXT("duration"))
        ? static_cast<float>(GetJsonNumberField(Context.Payload, TEXT("duration")))
        : 5.0f;
    const float Size = Context.Payload->HasField(TEXT("radius"))
        ? static_cast<float>(GetJsonNumberField(Context.Payload, TEXT("radius")))
        : static_cast<float>(Context.Payload->HasField(TEXT("size")) ? GetJsonNumberField(Context.Payload, TEXT("size")) : 100.0);
    const float Thickness = Context.Payload->HasField(TEXT("thickness"))
        ? static_cast<float>(GetJsonNumberField(Context.Payload, TEXT("thickness")))
        : 2.0f;
    return DrawShape(Context, ShapeType, ReadVectorField(Context.Payload, TEXT("location")), Size, Duration, Thickness, ReadColorField(Context.Payload, TEXT("color")));
}
}
