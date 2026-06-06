#include "McpVersionCompatibility.h"

#include "McpAutomationBridge_EffectHandlersPrivate.h"

#include "DrawDebugHelpers.h"

#if WITH_EDITOR
#include "Editor.h"
#endif

namespace McpEffectHandlers
{
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
        TSharedPtr<FJsonObject> Response = McpHandlerUtils::CreateResultObject();
        Response->SetArrayField(TEXT("shapes"), Shapes);
        Response->SetNumberField(TEXT("count"), Shapes.Num());
        Context.Bridge.SendAutomationResponse(
            Context.Socket, Context.RequestId, true,
            TEXT("Available debug shape types"), Response);
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

bool HandleParticleDebugShape(const FEffectActionContext& Context)
{
    FString Preset;
    Context.Payload->TryGetStringField(TEXT("preset"), Preset);
    if (Preset.IsEmpty())
    {
        TSharedPtr<FJsonObject> Response = McpHandlerUtils::CreateResultObject();
        Response->SetBoolField(TEXT("success"), false);
        Response->SetStringField(TEXT("error"), TEXT("preset parameter required for particle spawning"));
        Context.Bridge.SendAutomationResponse(
            Context.Socket, Context.RequestId, false,
            TEXT("Preset path required"), Response, TEXT("INVALID_ARGUMENT"));
        return true;
    }
    FString ShapeType = TEXT("sphere");
    Context.Payload->TryGetStringField(TEXT("shapeType"), ShapeType);
    const float Duration = Context.Payload->HasField(TEXT("duration"))
        ? static_cast<float>(GetJsonNumberField(Context.Payload, TEXT("duration")))
        : 5.0f;
    const float Size = Context.Payload->HasField(TEXT("size"))
        ? static_cast<float>(GetJsonNumberField(Context.Payload, TEXT("size")))
        : 100.0f;
    const float Thickness = Context.Payload->HasField(TEXT("thickness"))
        ? static_cast<float>(GetJsonNumberField(Context.Payload, TEXT("thickness")))
        : 2.0f;
    return DrawShape(Context, ShapeType, ReadVectorField(Context.Payload, TEXT("location")), Size, Duration, Thickness, ReadColorField(Context.Payload, TEXT("color")));
}
}
