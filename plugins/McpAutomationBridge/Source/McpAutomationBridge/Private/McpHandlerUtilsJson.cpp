#include "McpHandlerUtils.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpSafeOperations.h"
#include "EngineUtils.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

#if WITH_EDITOR
#include "Editor.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Components/ActorComponent.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/AssetRegistryHelpers.h"
#if __has_include("EditorAssetLibrary.h")
#include "EditorAssetLibrary.h"
#else
#include "Editor/EditorAssetLibrary.h"
#endif
#include "K2Node_CustomEvent.h"
#include "K2Node_Event.h"
#include "K2Node_VariableGet.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "EdGraphSchema_K2.h"
#endif

namespace McpHandlerUtils
{

FString JsonValueToString(const TSharedPtr<FJsonValue>& Value)
{
    if (!Value.IsValid())
    {
        return FString();
    }

    switch (Value->Type)
    {
    case EJson::String:
        return Value->AsString();
    case EJson::Number:
        return LexToString(Value->AsNumber());
    case EJson::Boolean:
        return Value->AsBool() ? TEXT("true") : TEXT("false");
    case EJson::Null:
        return FString();
    default:
        break;
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
    else if (Value->Type == EJson::Array)
    {
        FJsonSerializer::Serialize(Value->AsArray(), *Writer, true);
    }
    else
    {
        Writer->WriteValue(Value->AsString());
    }

    Writer->Close();
    return Serialized;
}
}
