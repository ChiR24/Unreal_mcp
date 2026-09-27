#include "Foundation/HandlerUtils/McpHandlerUtilsJson.h"

#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "Safety/McpSafeOperations.h"
#include "EngineUtils.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

#include "Editor.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Components/ActorComponent.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/AssetRegistryHelpers.h"
#include "EditorAssetLibrary.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_Event.h"
#include "K2Node_VariableGet.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "EdGraphSchema_K2.h"

namespace McpHandlerUtils
{

FString JsonValueToString(const TSharedPtr<FJsonValue>& Value)
{
    if (!Value.IsValid())
    {
        return FString();
    }

    FString Scalar;
    if (McpJsonScalarToString(Value, Scalar) || Value->Type == EJson::Null)
    {
        return Scalar;
    }

    // Handle object and array types by serializing
    FString Serialized;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Serialized);

    if (Value->Type == EJson::Object)
    {
        const TSharedPtr<FJsonObject> Obj = Value->AsObject();
        if (Obj.IsValid())
        {
            FJsonSerializer::Serialize(Obj.ToSharedRef(), *Writer, true);
        }
    }
    else
    {
        FJsonSerializer::Serialize(Value->AsArray(), *Writer, true);
    }

    Writer->Close();
    return Serialized;
}

void FilterRowsByListedNames(
    const TSharedPtr<FJsonObject>& Payload, const FString& ListField,
    TArray<TSharedPtr<FJsonValue>>& Rows, const TSharedPtr<FJsonObject>& Result, const FString& MissingField)
{
    TArray<FString> Wanted = GetStringArrayField(Payload, ListField);
    if (Wanted.Num() == 0)
    {
        return;
    }
    TArray<TSharedPtr<FJsonValue>> Kept;
    for (const TSharedPtr<FJsonValue>& Row : Rows)
    {
        const TSharedPtr<FJsonObject>* Object = nullptr;
        FString Name;
        if (!Row.IsValid() || !Row->TryGetObject(Object) || !Object || !(*Object)->TryGetStringField(TEXT("name"), Name))
        {
            continue;
        }
        const int32 Match = Wanted.IndexOfByPredicate([&Name](const FString& W) { return W.Equals(Name, ESearchCase::IgnoreCase); });
        if (Match != INDEX_NONE)
        {
            Kept.Add(Row);
            Wanted.RemoveAt(Match);
        }
    }
    Rows = MoveTemp(Kept);
    if (Wanted.Num() > 0 && Result.IsValid())
    {
        TArray<TSharedPtr<FJsonValue>> Missing;
        for (const FString& Name : Wanted)
        {
            Missing.Add(MakeShared<FJsonValueString>(Name));
        }
        Result->SetArrayField(MissingField, Missing);
    }
}
}
