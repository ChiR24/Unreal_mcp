#include "McpAutomationBridge_EnvironmentHandlersShared.h"

#if WITH_EDITOR
namespace McpEnvironmentHandlers {

TSharedPtr<FJsonObject> McpMakeVectorObject(const FVector &Vector)
{
    TSharedPtr<FJsonObject> Obj = McpHandlerUtils::CreateResultObject();
    Obj->SetNumberField(TEXT("x"), Vector.X);
    Obj->SetNumberField(TEXT("y"), Vector.Y);
    Obj->SetNumberField(TEXT("z"), Vector.Z);
    return Obj;
}
TSharedPtr<FJsonObject> McpMakeRotatorObject(const FRotator &Rotator)
{
    TSharedPtr<FJsonObject> Obj = McpHandlerUtils::CreateResultObject();
    Obj->SetNumberField(TEXT("pitch"), Rotator.Pitch);
    Obj->SetNumberField(TEXT("yaw"), Rotator.Yaw);
    Obj->SetNumberField(TEXT("roll"), Rotator.Roll);
    return Obj;
}
TSharedPtr<FJsonObject> McpMakeTransformObject(const FTransform &Transform)
{
    TSharedPtr<FJsonObject> Obj = McpHandlerUtils::CreateResultObject();
    Obj->SetObjectField(TEXT("location"), McpMakeVectorObject(Transform.GetLocation()));
    Obj->SetObjectField(TEXT("rotation"), McpMakeRotatorObject(Transform.GetRotation().Rotator()));
    Obj->SetObjectField(TEXT("scale"), McpMakeVectorObject(Transform.GetScale3D()));
    return Obj;
}
FString McpGetFirstStringField(const TSharedPtr<FJsonObject> &Payload, std::initializer_list<const TCHAR *> Fields)
{
    if (!Payload.IsValid())
    {
        return FString();
    }

    for (const TCHAR *Field : Fields)
    {
        FString Value;
        if (Payload->TryGetStringField(Field, Value) && !Value.IsEmpty())
        {
            return Value;
        }
    }
    return FString();
}
FVector McpGetVectorField(const TSharedPtr<FJsonObject> &Payload, const TCHAR *FieldName, const FVector &DefaultValue)
{
    if (!Payload.IsValid())
    {
        return DefaultValue;
    }

    FVector Result = DefaultValue;
    const TSharedPtr<FJsonObject> *Obj = nullptr;
    if (Payload->TryGetObjectField(FieldName, Obj) && Obj && Obj->IsValid())
    {
        McpPropertyReflection::JsonToVector(*Obj, Result);
        return Result;
    }

    const TArray<TSharedPtr<FJsonValue>> *Arr = nullptr;
    if (Payload->TryGetArrayField(FieldName, Arr) && Arr)
    {
        McpPropertyReflection::JsonArrayToVector(*Arr, Result);
    }
    return Result;
}
FRotator McpGetRotatorField(const TSharedPtr<FJsonObject> &Payload, const TCHAR *FieldName, const FRotator &DefaultValue)
{
    if (!Payload.IsValid())
    {
        return DefaultValue;
    }

    FRotator Result = DefaultValue;
    const TSharedPtr<FJsonObject> *Obj = nullptr;
    if (Payload->TryGetObjectField(FieldName, Obj) && Obj && Obj->IsValid())
    {
        McpPropertyReflection::JsonToRotator(*Obj, Result);
        return Result;
    }

    const TArray<TSharedPtr<FJsonValue>> *Arr = nullptr;
    if (Payload->TryGetArrayField(FieldName, Arr) && Arr)
    {
        McpPropertyReflection::JsonArrayToRotator(*Arr, Result);
    }
    return Result;
}
bool McpGetFirstNumberField(const TSharedPtr<FJsonObject> &Payload, std::initializer_list<const TCHAR *> Fields, double &OutValue)
{
    if (!Payload.IsValid())
    {
        return false;
    }

    for (const TCHAR *Field : Fields)
    {
        double Value = 0.0;
        if (Payload->TryGetNumberField(Field, Value))
        {
            OutValue = Value;
            return true;
        }
    }
    return false;
}
void McpAddStringArrayField(TSharedPtr<FJsonObject> Obj, const TCHAR *FieldName, const TArray<FString> &Values)
{
    TArray<TSharedPtr<FJsonValue>> JsonValues;
    for (const FString &Value : Values)
    {
        JsonValues.Add(MakeShared<FJsonValueString>(Value));
    }
    Obj->SetArrayField(FieldName, JsonValues);
}

} // namespace McpEnvironmentHandlers
#endif
