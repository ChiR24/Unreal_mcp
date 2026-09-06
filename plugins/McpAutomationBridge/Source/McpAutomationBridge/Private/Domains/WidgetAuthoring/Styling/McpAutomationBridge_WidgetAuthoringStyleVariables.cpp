#include "Domains/WidgetAuthoring/McpAutomationBridge_WidgetAuthoringActions.h"
#include "Domains/WidgetAuthoring/Support/McpAutomationBridge_WidgetAuthoringBlueprintLoading.h"

#include "EdGraphSchema_K2.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Transport/WebSocket/McpBridgeWebSocket.h"
#include "Styling/SlateBrush.h"
#include "Styling/SlateTypes.h"
#include "WidgetBlueprint.h"

namespace WidgetAuthoringHandlers
{
using namespace WidgetAuthoringHelpers;

bool HandleWidgetAuthoringStyleVariables(
    UMcpAutomationBridgeSubsystem& Subsystem,
    const FString& RequestId,
    const FString& SubAction,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket,
    TSharedPtr<FJsonObject> ResultJson)
{
    // create_widget_style - Create reusable widget style (FSlateWidgetStyle equivalent via variables)
    if (SubAction.Equals(TEXT("create_widget_style"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        if (WidgetPath.IsEmpty())
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameter: widgetPath"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        FString StyleName = GetJsonStringField(Payload, TEXT("styleName"));
        if (StyleName.IsEmpty())
        {
            StyleName = TEXT("DefaultStyle");
        }

        FString StyleType = GetJsonStringField(Payload, TEXT("styleType"));
        if (StyleType.IsEmpty())
        {
            StyleType = TEXT("Text");
        }

        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP)
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        TArray<FString> CreatedVariables;
        auto MakeStructPin = [](UScriptStruct* Struct) {
            FEdGraphPinType PinType;
            PinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
            PinType.PinSubCategoryObject = Struct;
            return PinType;
        };
        auto AddStyleVariable = [&](const FString& VarName, const FEdGraphPinType& PinType) {
            FBlueprintEditorUtils::AddMemberVariable(WidgetBP, *VarName, PinType);
            FBlueprintEditorUtils::SetBlueprintVariableCategory(WidgetBP, *VarName, nullptr,
                FText::FromString(TEXT("Widget Styles")));
            CreatedVariables.Add(VarName);
        };

        // Create style variables based on type
        if (StyleType.Equals(TEXT("Text"), ESearchCase::IgnoreCase))
        {
            // Font style variable
            const FEdGraphPinType FontPinType = MakeStructPin(FSlateFontInfo::StaticStruct());

            FString FontVarName = StyleName + TEXT("_Font");
            AddStyleVariable(FontVarName, FontPinType);

            // Color variable
            const FEdGraphPinType ColorPinType = MakeStructPin(TBaseStructure<FSlateColor>::Get());

            FString ColorVarName = StyleName + TEXT("_Color");
            AddStyleVariable(ColorVarName, ColorPinType);

            // Shadow color
            FString ShadowVarName = StyleName + TEXT("_ShadowColor");
            AddStyleVariable(ShadowVarName, ColorPinType);
        }
        else if (StyleType.Equals(TEXT("Button"), ESearchCase::IgnoreCase))
        {
            // Button style uses FButtonStyle
            const FEdGraphPinType ButtonStylePinType = MakeStructPin(FButtonStyle::StaticStruct());

            FString ButtonStyleVarName = StyleName + TEXT("_ButtonStyle");
            AddStyleVariable(ButtonStyleVarName, ButtonStylePinType);

            // Normal/Hovered/Pressed colors
            const FEdGraphPinType ColorPinType = MakeStructPin(TBaseStructure<FLinearColor>::Get());

            for (const FString& State : { TEXT("Normal"), TEXT("Hovered"), TEXT("Pressed") })
            {
                FString StateVarName = StyleName + TEXT("_") + State + TEXT("Color");
                AddStyleVariable(StateVarName, ColorPinType);
            }
        }
        else if (StyleType.Equals(TEXT("Image"), ESearchCase::IgnoreCase))
        {
            // Brush style
            const FEdGraphPinType BrushPinType = MakeStructPin(FSlateBrush::StaticStruct());

            FString BrushVarName = StyleName + TEXT("_Brush");
            AddStyleVariable(BrushVarName, BrushPinType);

            // Tint color
            const FEdGraphPinType ColorPinType = MakeStructPin(TBaseStructure<FLinearColor>::Get());

            FString TintVarName = StyleName + TEXT("_Tint");
            AddStyleVariable(TintVarName, ColorPinType);
        }
        else if (StyleType.Equals(TEXT("ProgressBar"), ESearchCase::IgnoreCase))
        {
            const FEdGraphPinType StylePinType = MakeStructPin(FProgressBarStyle::StaticStruct());

            FString ProgressStyleVarName = StyleName + TEXT("_ProgressStyle");
            AddStyleVariable(ProgressStyleVarName, StylePinType);
        }
        else
        {
            // Generic style - create color and margin variables
            const FEdGraphPinType ColorPinType = MakeStructPin(TBaseStructure<FLinearColor>::Get());

            FString ColorVarName = StyleName + TEXT("_Color");
            AddStyleVariable(ColorVarName, ColorPinType);

            const FEdGraphPinType MarginPinType = MakeStructPin(TBaseStructure<FMargin>::Get());

            FString MarginVarName = StyleName + TEXT("_Margin");
            AddStyleVariable(MarginVarName, MarginPinType);
        }

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);
        McpSafeAssetSave(WidgetBP);

        TArray<TSharedPtr<FJsonValue>> VariablesArray;
        for (const FString& VarName : CreatedVariables)
        {
            VariablesArray.Add(MakeShared<FJsonValueString>(VarName));
        }

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("widgetPath"), WidgetPath);
        ResultJson->SetStringField(TEXT("styleName"), StyleName);
        ResultJson->SetStringField(TEXT("styleType"), StyleType);
        ResultJson->SetArrayField(TEXT("createdVariables"), VariablesArray);
        ResultJson->SetNumberField(TEXT("variableCount"), CreatedVariables.Num());

        Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Widget style variables created"), ResultJson);
        return true;
    }

    return false;
}
}
