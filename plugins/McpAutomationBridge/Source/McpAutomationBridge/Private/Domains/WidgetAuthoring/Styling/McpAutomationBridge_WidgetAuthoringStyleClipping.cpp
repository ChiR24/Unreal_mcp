#include "Domains/WidgetAuthoring/McpAutomationBridge_WidgetAuthoringActions.h"
#include "Domains/WidgetAuthoring/Support/McpAutomationBridge_WidgetAuthoringBlueprintLoading.h"
#include "Domains/WidgetAuthoring/McpAutomationBridge_WidgetAuthoringPayload.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Widget.h"
#include "Components/TextBlock.h"
#include "Domains/WidgetAuthoring/Styling/McpAutomationBridge_WidgetAuthoringStyleColor.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "JsonObjectConverter.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Transport/WebSocket/McpBridgeWebSocket.h"
#include "Core/Compatibility/McpVersionCompatibility.h"
#include "UObject/UnrealType.h"
#include "WidgetBlueprint.h"

namespace WidgetAuthoringHandlers
{
using namespace WidgetAuthoringHelpers;

bool HandleWidgetAuthoringStyleClipping(
    UMcpAutomationBridgeSubsystem& Subsystem,
    const FString& RequestId,
    const FString& SubAction,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket,
    TSharedPtr<FJsonObject> ResultJson)
{
    if (SubAction.Equals(TEXT("set_style"), ESearchCase::IgnoreCase) ||
        SubAction.Equals(TEXT("set_clipping"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        FString SlotName = GetSlotName(Payload);

        if (WidgetPath.IsEmpty() || SlotName.IsEmpty())
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameters: widgetPath and slotName"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP)
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        UWidget* Widget = WidgetBP->WidgetTree->FindWidget(FName(*SlotName));
        if (!Widget)
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Widget not found"), TEXT("WIDGET_NOT_FOUND"));
            return true;
        }
        // Every reply names the Widget Blueprint it saved into. Neither widgetPath nor assetPath was
        // there, so the receipt of a style or clipping change carried no handle and no change.
        ResultJson->SetStringField(TEXT("widgetPath"), WidgetBlueprintPackagePath(WidgetBP));

        if (SubAction.Equals(TEXT("set_clipping"), ESearchCase::IgnoreCase))
        {
            FString ClippingStr = GetJsonStringField(Payload, TEXT("clipping"), TEXT("Inherit"));
            EWidgetClipping Clipping = EWidgetClipping::Inherit;
            if (ClippingStr.Equals(TEXT("Inherit"), ESearchCase::IgnoreCase))
            {
                Clipping = EWidgetClipping::Inherit;
            }
            else if (ClippingStr.Equals(TEXT("ClipToBounds"), ESearchCase::IgnoreCase))
            {
                Clipping = EWidgetClipping::ClipToBounds;
            }
            else if (ClippingStr.Equals(TEXT("ClipToBoundsWithoutIntersecting"), ESearchCase::IgnoreCase))
            {
                Clipping = EWidgetClipping::ClipToBoundsWithoutIntersecting;
            }
            else if (ClippingStr.Equals(TEXT("ClipToBoundsAlways"), ESearchCase::IgnoreCase))
            {
                Clipping = EWidgetClipping::ClipToBoundsAlways;
            }
            else if (ClippingStr.Equals(TEXT("OnDemand"), ESearchCase::IgnoreCase))
            {
                Clipping = EWidgetClipping::OnDemand;
            }
            else
            {
                // An unknown name used to reset clipping to Inherit and report success.
                Subsystem.SendAutomationError(RequestingSocket, RequestId, FString::Printf(
                    TEXT("clipping '%s' is not one of Inherit, ClipToBounds, ClipToBoundsWithoutIntersecting, ClipToBoundsAlways, OnDemand."),
                    *ClippingStr), TEXT("INVALID_ARGUMENT"));
                return true;
            }
            Widget->SetClipping(Clipping);
            WidgetBP->MarkPackageDirty();
            const bool bSaveSucceeded = McpSafeAssetSave(WidgetBP);

            ResultJson->SetStringField(TEXT("mode"), TEXT("write"));
            ResultJson->SetStringField(TEXT("propertyName"), TEXT("Clipping"));
            ResultJson->SetStringField(TEXT("value"), ClippingStr);
            ResultJson->SetStringField(TEXT("widgetName"), SlotName);
            ResultJson->SetStringField(TEXT("widgetClass"), Widget->GetClass()->GetName());
            ResultJson->SetBoolField(TEXT("saveSucceeded"), bSaveSucceeded);
            if (!bSaveSucceeded)
            {
                ResultJson->SetStringField(TEXT("warning"), TEXT("Clipping changed in editor memory, but package save did not complete in the current headless session."));
            }
        }
        else if (SubAction.Equals(TEXT("set_style"), ESearchCase::IgnoreCase))
        {
            // Convenience fields from the published contract (fontSize,
            // colorAndOpacity, text) applied through the widget API; the generic
            // reflection path below still handles propertyName/value.
            if (!Payload->HasField(TEXT("propertyName")))
            {
                TArray<TSharedPtr<FJsonValue>> Applied;
                FString Unsupported;
                if (!McpApplyWidgetStyleConvenience(Widget, Payload, ResultJson, Applied, Unsupported))
                {
                    SendStandardErrorResponse(&Subsystem, RequestingSocket, RequestId,
                        TEXT("STYLE_FIELD_UNSUPPORTED"), Unsupported, ResultJson);
                    return true;
                }
                if (Applied.Num() > 0)
                {
                    Widget->Modify();
                    FBlueprintEditorUtils::MarkBlueprintAsModified(WidgetBP);
                    // Saved like set_clipping and the reflection path below: a
                    // convenience edit (text, colour, rounding, sounds) stayed dirty
                    // in memory, so an editor restart or a package build dropped it.
                    const bool bStyleSaved = McpSafeAssetSave(WidgetBP);
                    ResultJson->SetBoolField(TEXT("saveSucceeded"), bStyleSaved);
                    ResultJson->SetBoolField(TEXT("saved"), bStyleSaved);
                    ResultJson->SetStringField(TEXT("widgetName"), SlotName);
                    ResultJson->SetStringField(TEXT("widgetClass"), Widget->GetClass()->GetName());
                    // `applied` is the layout object every layout setter declares; the list of style
                    // fields written used to go there, so a successful write came back as an
                    // OUTPUT_SCHEMA_VIOLATION error. The list now has its own field.
                    ResultJson->SetArrayField(TEXT("appliedFields"), Applied);
                    ResultJson->SetObjectField(TEXT("applied"), McpDescribeWidgetLayout(Widget));
                    Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Style applied"), ResultJson);
                    return true;
                }
            }
            // Generic property setter via UE reflection — works on any widget class, any property
            FString PropertyName = GetJsonStringField(Payload, TEXT("propertyName"));
            FString Value;
            bool bHasValueField = Payload->HasField(TEXT("value"));
            bool bUseJsonConverter = false;
            TSharedPtr<FJsonValue> RawJsonValue;

            // Extract value from JSON — handle string, number, bool, object, and array types
            if (bHasValueField)
            {
                const TSharedPtr<FJsonValue> ValField = Payload->TryGetField(TEXT("value"));
                if (ValField.IsValid())
                {
                    if (ValField->Type == EJson::String)
                    {
                        Value = ValField->AsString();
                    }
                    else if (ValField->Type == EJson::Number)
                    {
                        Value = FString::SanitizeFloat(ValField->AsNumber());
                    }
                    else if (ValField->Type == EJson::Boolean)
                    {
                        Value = ValField->AsBool() ? TEXT("True") : TEXT("False");
                    }
                    else if (ValField->Type == EJson::Object || ValField->Type == EJson::Array)
                    {
                        // Defer to FJsonObjectConverter for struct-backed properties
                        bUseJsonConverter = true;
                        RawJsonValue = ValField;
                    }
                    else if (ValField->Type == EJson::Null)
                    {
                        Subsystem.SendAutomationError(RequestingSocket, RequestId,
                            TEXT("Null JSON value is not supported for property mutation"), TEXT("UNSUPPORTED_VALUE_TYPE"));
                        return true;
                    }
                }
            }

            if (PropertyName.IsEmpty())
            {
                Subsystem.SendAutomationError(RequestingSocket, RequestId,
                    TEXT("set_style needs at least one of colorAndOpacity, fontSize, text, justification, texturePath, renderOpacity, ")
                    TEXT("cornerRadius, hoverSoundPath, pressSoundPath, or propertyName (with value to write it, without to read it)."),
                    TEXT("MISSING_PARAMETER"));
                return true;
            }

            FProperty* Prop = Widget->GetClass()->FindPropertyByName(FName(*PropertyName));
            if (!Prop && PropertyName.Equals(TEXT("Style"), ESearchCase::IgnoreCase) && Widget)
            {
                Prop = FindWidgetStyleProperty(Widget->GetClass());
                PropertyName = Prop ? Prop->GetName() : PropertyName;
            }
            if (!Prop)
            {
                Subsystem.SendAutomationError(RequestingSocket, RequestId,
                    FString::Printf(TEXT("Property '%s' not found on widget '%s' (class %s)"), *PropertyName, *SlotName, *Widget->GetClass()->GetName()),
                    TEXT("PROPERTY_NOT_FOUND"));
                return true;
            }

            void* ValuePtr = Prop->ContainerPtrToValuePtr<void>(Widget);

            if (!bHasValueField)
            {
                // READ mode — value field not present, export and return current value
                FString ExportedValue;
                MCP_PROPERTY_EXPORT_TEXT(Prop, ExportedValue, ValuePtr, ValuePtr, Widget, PPF_None);

                ResultJson->SetStringField(TEXT("mode"), TEXT("read"));
                McpHandlerUtils::MarkNoAssetsChanged(ResultJson); // a read: the widgetPath above is identity, not a change
                ResultJson->SetStringField(TEXT("propertyName"), PropertyName);
                ResultJson->SetStringField(TEXT("value"), ExportedValue);
                ResultJson->SetStringField(TEXT("widgetName"), SlotName);
                ResultJson->SetStringField(TEXT("widgetClass"), Widget->GetClass()->GetName());
            }
            else
            {
                // WRITE mode — set the property value
                Widget->Modify();

                bool bWriteSuccess = false;
                if (bUseJsonConverter && RawJsonValue.IsValid())
                {
                    // Use FJsonObjectConverter for struct-backed properties (Object/Array JSON)
                    bWriteSuccess = FJsonObjectConverter::JsonValueToUProperty(RawJsonValue, Prop, ValuePtr, 0, 0);
                }
                else
                {
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
                    const TCHAR* ImportResult = Prop->ImportText_Direct(*Value, ValuePtr, Widget, PPF_None);
#else
                    const TCHAR* ImportResult = Prop->ImportText(*Value, ValuePtr, PPF_None, Widget);
#endif
                    bWriteSuccess = (ImportResult != nullptr);
                }
                if (!bWriteSuccess)
                {
                    Subsystem.SendAutomationError(RequestingSocket, RequestId,
                        FString::Printf(TEXT("Failed to set '%s' to '%s' on widget '%s'"), *PropertyName, *Value, *SlotName),
                        TEXT("SET_PROPERTY_FAILED"));
                    return true;
                }

                FPropertyChangedEvent ChangeEvent(Prop);
                Widget->PostEditChangeProperty(ChangeEvent);

                // Export the value back to verify what was actually set
                FString ExportedValue;
                MCP_PROPERTY_EXPORT_TEXT(Prop, ExportedValue, ValuePtr, ValuePtr, Widget, PPF_None);

                ResultJson->SetStringField(TEXT("mode"), TEXT("write"));
                ResultJson->SetStringField(TEXT("propertyName"), PropertyName);
                ResultJson->SetStringField(TEXT("value"), Value);
                ResultJson->SetStringField(TEXT("exportedValue"), ExportedValue);
                ResultJson->SetStringField(TEXT("widgetName"), SlotName);
                ResultJson->SetStringField(TEXT("widgetClass"), Widget->GetClass()->GetName());

                // Property change — mark dirty and save, do NOT recompile (that wipes instance values)
                WidgetBP->MarkPackageDirty();
                McpSafeAssetSave(WidgetBP);
            }
        }

        ResultJson->SetBoolField(TEXT("success"), true);
        FString ModeStr;
        bool bIsRead = ResultJson->TryGetStringField(TEXT("mode"), ModeStr) && ModeStr == TEXT("read");
        FString Msg = bIsRead
            ? FString::Printf(TEXT("%s property read"), *SubAction)
            : FString::Printf(TEXT("%s applied"), *SubAction);
        ResultJson->SetStringField(TEXT("message"), Msg);

        Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true, Msg, ResultJson);
        return true;
    }

    return false;
}
}
