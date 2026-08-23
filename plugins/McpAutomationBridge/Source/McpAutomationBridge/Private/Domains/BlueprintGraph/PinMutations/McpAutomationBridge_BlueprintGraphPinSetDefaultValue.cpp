// Blueprint pin-mutation: SetPinDefaultValue handler, split from
// McpAutomationBridge_BlueprintGraphHandlersPinMutations.cpp.
#include "Domains/BlueprintGraph/McpAutomationBridge_BlueprintGraphHandlersPrivate.h"

#if WITH_EDITOR
#include "EdGraph/EdGraphSchema.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "ScopedTransaction.h"

namespace McpBlueprintGraphHandlers
{
namespace
{
/** Object/class pins carry their value in DefaultObject, not DefaultValue. */
bool IsObjectLikePin(const UEdGraphPin& Pin)
{
    const FName Category = Pin.PinType.PinCategory;
    return Category == UEdGraphSchema_K2::PC_Object
        || Category == UEdGraphSchema_K2::PC_Class
        || Category == UEdGraphSchema_K2::PC_SoftObject
        || Category == UEdGraphSchema_K2::PC_SoftClass
        || Category == UEdGraphSchema_K2::PC_Interface;
}

/** A Blueprint referenced by asset path resolves through its generated `_C` class. */
UObject* ResolvePinObject(const FString& Path)
{
    const FString Trimmed = Path.TrimStartAndEnd();
    if (Trimmed.IsEmpty())
    {
        return nullptr;
    }
    if (UObject* Direct = LoadObject<UObject>(nullptr, *Trimmed))
    {
        return Direct;
    }
    if (Trimmed.EndsWith(TEXT("_C")))
    {
        return nullptr;
    }
    FString ObjectPath = Trimmed;
    if (!Trimmed.Contains(TEXT(".")))
    {
        int32 SlashIndex = INDEX_NONE;
        if (!Trimmed.FindLastChar(TEXT('/'), SlashIndex))
        {
            return nullptr;
        }
        ObjectPath = Trimmed + TEXT(".") + Trimmed.RightChop(SlashIndex + 1);
    }
    return LoadObject<UObject>(nullptr, *(ObjectPath + TEXT("_C")));
}

/** Renders a JSON scalar as the literal a pin expects (ints stay ints). */
FString PinLiteralFromJson(const TSharedPtr<FJsonValue>& Field)
{
    FString AsString;
    if (Field->TryGetString(AsString))
    {
        return AsString;
    }
    bool bAsBool = false;
    if (Field->TryGetBool(bAsBool))
    {
        return bAsBool ? TEXT("true") : TEXT("false");
    }
    double AsNumber = 0.0;
    if (Field->TryGetNumber(AsNumber))
    {
        const double Rounded = FMath::RoundToDouble(AsNumber);
        if (FMath::IsNearlyEqual(AsNumber, Rounded) && FMath::Abs(AsNumber) < 1.0e15)
        {
            return FString::Printf(TEXT("%lld"), static_cast<int64>(Rounded));
        }
        return FString::SanitizeFloat(AsNumber);
    }
    return FString();
}
}

bool SetPinDefaultValue(FActionContext& Context)
{
    if (Context.SubAction != TEXT("set_pin_default_value"))
    {
        return false;
    }

    const FString NodeId = PickFirstNonEmpty(
        Context.Payload, {TEXT("nodeId"), TEXT("nodeGuid"), TEXT("toNodeId"),
                          TEXT("toNode"), TEXT("targetNodeGuid"),
                          TEXT("targetNodeId"), TEXT("targetNode")});
    const FString PinName = PickFirstNonEmpty(
        Context.Payload, {TEXT("pinName"), TEXT("pin"), TEXT("targetPinName"),
                          TEXT("targetPin"), TEXT("inputPin")});

    // The published schema declares `propertyValue` and, being closed, rejects
    // `value` outright. Reading only `value` meant every gateway call silently
    // set an EMPTY default while still reporting success — numeric pins landed
    // on 0 and class pins on None, which surfaces much later as an "Accessed
    // None" runtime error rather than a failed call.
    TSharedPtr<FJsonValue> ValueField = Context.Payload->TryGetField(TEXT("propertyValue"));
    if (!ValueField.IsValid())
    {
        ValueField = Context.Payload->TryGetField(TEXT("value"));
    }
    if (!ValueField.IsValid())
    {
        Context.SendError(
            TEXT("propertyValue (or legacy 'value') field required."),
            TEXT("INVALID_ARGUMENT"));
        return true;
    }
    const FString Value = PinLiteralFromJson(ValueField);

    UEdGraphNode* TargetNode = Context.FindNode(NodeId);
    if (!TargetNode)
    {
        Context.SendError(TEXT("Node not found."), TEXT("NODE_NOT_FOUND"));
        return true;
    }
    UEdGraphPin* Pin = Context.FindPin(TargetNode, PinName);
    if (!Pin)
    {
        Context.SendError(TEXT("Pin not found."), TEXT("PIN_NOT_FOUND"));
        return true;
    }
    if (Pin->Direction != EGPD_Input)
    {
        Context.SendError(
            TEXT("Can only set default values on input pins."),
            TEXT("INVALID_PIN_DIRECTION"));
        return true;
    }

    // Resolve before opening the transaction, so an unresolvable path fails
    // without leaving an empty entry on the undo stack.
    const bool bObjectPin = IsObjectLikePin(*Pin) && !Value.IsEmpty();
    UObject* ResolvedObject = nullptr;
    if (bObjectPin)
    {
        ResolvedObject = ResolvePinObject(Value);
        if (!ResolvedObject)
        {
            Context.SendError(
                FString::Printf(TEXT("Could not resolve '%s' for object pin '%s'."),
                                *Value, *PinName),
                TEXT("OBJECT_NOT_FOUND"));
            return true;
        }
        // A Blueprint asset path resolves to the UBlueprint, but a class pin
        // holds the generated class. Handing the schema the UBlueprint fails its
        // type check and TrySetDefaultObject then does nothing at all — the pin
        // stays None while the call still reports success.
        const FName Category = Pin->PinType.PinCategory;
        const bool bWantsClass = Category == UEdGraphSchema_K2::PC_Class
            || Category == UEdGraphSchema_K2::PC_SoftClass;
        if (bWantsClass && !ResolvedObject->IsA<UClass>())
        {
            if (const UBlueprint* AsBlueprint = Cast<UBlueprint>(ResolvedObject))
            {
                ResolvedObject = AsBlueprint->GeneratedClass;
            }
        }
        if (bWantsClass && !ResolvedObject)
        {
            Context.SendError(
                FString::Printf(TEXT("'%s' has no generated class for class pin '%s'."),
                                *Value, *PinName),
                TEXT("OBJECT_NOT_FOUND"));
            return true;
        }
    }

    const FScopedTransaction Transaction(
        FText::FromString(TEXT("Set Pin Default Value")));
    Context.Blueprint->Modify();
    Context.TargetGraph->Modify();
    TargetNode->Modify();

    const UEdGraphSchema* Schema = Context.TargetGraph->GetSchema();
    if (bObjectPin)
    {
        Schema->TrySetDefaultObject(*Pin, ResolvedObject);
        // TrySetDefaultObject reports nothing when its type check refuses the
        // object; the pin simply stays None. Fail closed instead of handing back
        // a success the caller only discovers as an "Accessed None" at runtime.
        if (Pin->DefaultObject != ResolvedObject)
        {
            Context.SendError(
                FString::Printf(
                    TEXT("Schema refused '%s' for pin '%s' (expects %s)."),
                    *ResolvedObject->GetPathName(), *PinName,
                    *Pin->PinType.PinCategory.ToString()),
                TEXT("PIN_TYPE_MISMATCH"));
            return true;
        }
    }
    else
    {
        Schema->TrySetDefaultValue(*Pin, Value);
    }

    FBlueprintEditorUtils::MarkBlueprintAsModified(Context.Blueprint);
    SaveLoadedAssetThrottled(Context.Blueprint);

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("nodeId"), NodeId);
    Result->SetStringField(TEXT("nodeName"), TargetNode->GetName());
    Result->SetStringField(TEXT("pinName"), PinName);
    Result->SetStringField(TEXT("value"), Value);
    // The applied literal, read back off the pin, so a caller can tell an
    // accepted value from one the schema silently rejected.
    Result->SetStringField(TEXT("appliedValue"),
        Pin->DefaultObject ? Pin->DefaultObject->GetPathName() : Pin->DefaultValue);
    McpHandlerUtils::AddVerification(Result, Context.Blueprint);
    Context.SendResponse(TEXT("Pin default value set."), Result);
    return true;
}
}
#endif
