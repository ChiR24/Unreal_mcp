#include "Domains/WidgetAuthoring/McpAutomationBridge_WidgetAuthoringActions.h"
#include "Domains/WidgetAuthoring/Support/McpAutomationBridge_WidgetAuthoringBlueprintLoading.h"
#include "Domains/WidgetAuthoring/McpAutomationBridge_WidgetAuthoringPayload.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Widget.h"
#include "Components/TextBlock.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "JsonObjectConverter.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
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

        if (SubAction.Equals(TEXT("set_clipping"), ESearchCase::IgnoreCase))
        {
            FString ClippingStr = GetJsonStringField(Payload, TEXT("clipping"), TEXT("Inherit"));
            EWidgetClipping Clipping = EWidgetClipping::Inherit;
            if (ClippingStr.Equals(TEXT("ClipToBounds"), ESearchCase::IgnoreCase))
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
                if (UTextBlock* TextBlock = Cast<UTextBlock>(Widget))
                {
                    double FontSize = 0.0;
                    if (Payload->TryGetNumberField(TEXT("fontSize"), FontSize) && FontSize > 0.0)
                    {
                        FSlateFontInfo Font = TextBlock->GetFont();
                        Font.Size = static_cast<int32>(FontSize);
                        TextBlock->SetFont(Font);
                        Applied.Add(MakeShared<FJsonValueString>(TEXT("fontSize")));
                    }
                    FString Text;
                    if (Payload->TryGetStringField(TEXT("text"), Text))
                    {
                        TextBlock->SetText(FText::FromString(Text));
                        Applied.Add(MakeShared<FJsonValueString>(TEXT("text")));
                    }
                    const TSharedPtr<FJsonObject>* ColorObj = nullptr;
                    if (Payload->TryGetObjectField(TEXT("colorAndOpacity"), ColorObj) && ColorObj && (*ColorObj).IsValid())
                    {
                        FLinearColor Color(
                            (*ColorObj)->HasField(TEXT("r")) ? (*ColorObj)->GetNumberField(TEXT("r")) : 1.0,
                            (*ColorObj)->HasField(TEXT("g")) ? (*ColorObj)->GetNumberField(TEXT("g")) : 1.0,
                            (*ColorObj)->HasField(TEXT("b")) ? (*ColorObj)->GetNumberField(TEXT("b")) : 1.0,
                            (*ColorObj)->HasField(TEXT("a")) ? (*ColorObj)->GetNumberField(TEXT("a")) : 1.0);
                        TextBlock->SetColorAndOpacity(FSlateColor(Color));
                        Applied.Add(MakeShared<FJsonValueString>(TEXT("colorAndOpacity")));
                    }
                }
                double RenderOpacity = 0.0;
                if (Payload->TryGetNumberField(TEXT("renderOpacity"), RenderOpacity))
                {
                    Widget->SetRenderOpacity(static_cast<float>(RenderOpacity));
                    Applied.Add(MakeShared<FJsonValueString>(TEXT("renderOpacity")));
                }
                if (Applied.Num() > 0)
                {
                    Widget->Modify();
                    FBlueprintEditorUtils::MarkBlueprintAsModified(WidgetBP);
                    ResultJson->SetStringField(TEXT("widgetName"), SlotName);
                    ResultJson->SetStringField(TEXT("widgetClass"), Widget->GetClass()->GetName());
                    ResultJson->SetArrayField(TEXT("applied"), Applied);
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
                // Legacy path: if no propertyName given, try "style" param against "Style" property
                // Reset state from any prior "value" field extraction to avoid stale data
                bUseJsonConverter = false;
                RawJsonValue.Reset();
                Value.Empty();

                PropertyName = TEXT("Style");
                bHasValueField = Payload->HasField(TEXT("style"));
                if (bHasValueField)
                {
                    const TSharedPtr<FJsonValue> StyleField = Payload->TryGetField(TEXT("style"));
                    if (StyleField.IsValid() && (StyleField->Type == EJson::Object || StyleField->Type == EJson::Array))
                    {
                        bUseJsonConverter = true;
                        RawJsonValue = StyleField;
                    }
                    else
                    {
                        Value = GetJsonStringField(Payload, TEXT("style"));
                    }
                }
            }

            if (PropertyName.IsEmpty())
            {
                Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameter: propertyName"), TEXT("MISSING_PARAMETER"));
                return true;
            }

            FProperty* Prop = Widget->GetClass()->FindPropertyByName(FName(*PropertyName));
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
