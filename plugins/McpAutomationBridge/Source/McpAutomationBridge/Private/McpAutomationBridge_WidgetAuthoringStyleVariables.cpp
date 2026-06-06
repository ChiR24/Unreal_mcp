#include "McpAutomationBridge_WidgetAuthoringActions.h"
#include "McpAutomationBridge_WidgetAuthoringBlueprintLoading.h"

#include "EdGraphSchema_K2.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpBridgeWebSocket.h"
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

        // Create style variables based on type
        if (StyleType.Equals(TEXT("Text"), ESearchCase::IgnoreCase))
        {
            // Font style variable
            FEdGraphPinType FontPinType;
            FontPinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
            FontPinType.PinSubCategoryObject = FSlateFontInfo::StaticStruct();

            FString FontVarName = StyleName + TEXT("_Font");
            FBlueprintEditorUtils::AddMemberVariable(WidgetBP, *FontVarName, FontPinType);
            FBlueprintEditorUtils::SetBlueprintVariableCategory(WidgetBP, *FontVarName, nullptr,
                FText::FromString(TEXT("Widget Styles")));
            CreatedVariables.Add(FontVarName);

            // Color variable
            FEdGraphPinType ColorPinType;
            ColorPinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
            ColorPinType.PinSubCategoryObject = TBaseStructure<FSlateColor>::Get();

            FString ColorVarName = StyleName + TEXT("_Color");
            FBlueprintEditorUtils::AddMemberVariable(WidgetBP, *ColorVarName, ColorPinType);
            FBlueprintEditorUtils::SetBlueprintVariableCategory(WidgetBP, *ColorVarName, nullptr,
                FText::FromString(TEXT("Widget Styles")));
            CreatedVariables.Add(ColorVarName);

            // Shadow color
            FString ShadowVarName = StyleName + TEXT("_ShadowColor");
            FBlueprintEditorUtils::AddMemberVariable(WidgetBP, *ShadowVarName, ColorPinType);
            FBlueprintEditorUtils::SetBlueprintVariableCategory(WidgetBP, *ShadowVarName, nullptr,
                FText::FromString(TEXT("Widget Styles")));
            CreatedVariables.Add(ShadowVarName);
        }
        else if (StyleType.Equals(TEXT("Button"), ESearchCase::IgnoreCase))
        {
            // Button style uses FButtonStyle
            FEdGraphPinType ButtonStylePinType;
            ButtonStylePinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
            ButtonStylePinType.PinSubCategoryObject = FButtonStyle::StaticStruct();

            FString ButtonStyleVarName = StyleName + TEXT("_ButtonStyle");
            FBlueprintEditorUtils::AddMemberVariable(WidgetBP, *ButtonStyleVarName, ButtonStylePinType);
            FBlueprintEditorUtils::SetBlueprintVariableCategory(WidgetBP, *ButtonStyleVarName, nullptr,
                FText::FromString(TEXT("Widget Styles")));
            CreatedVariables.Add(ButtonStyleVarName);

            // Normal/Hovered/Pressed colors
            FEdGraphPinType ColorPinType;
            ColorPinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
            ColorPinType.PinSubCategoryObject = TBaseStructure<FLinearColor>::Get();

            for (const FString& State : { TEXT("Normal"), TEXT("Hovered"), TEXT("Pressed") })
            {
                FString StateVarName = StyleName + TEXT("_") + State + TEXT("Color");
                FBlueprintEditorUtils::AddMemberVariable(WidgetBP, *StateVarName, ColorPinType);
                FBlueprintEditorUtils::SetBlueprintVariableCategory(WidgetBP, *StateVarName, nullptr,
                    FText::FromString(TEXT("Widget Styles")));
                CreatedVariables.Add(StateVarName);
            }
        }
        else if (StyleType.Equals(TEXT("Image"), ESearchCase::IgnoreCase))
        {
            // Brush style
            FEdGraphPinType BrushPinType;
            BrushPinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
            BrushPinType.PinSubCategoryObject = FSlateBrush::StaticStruct();

            FString BrushVarName = StyleName + TEXT("_Brush");
            FBlueprintEditorUtils::AddMemberVariable(WidgetBP, *BrushVarName, BrushPinType);
            FBlueprintEditorUtils::SetBlueprintVariableCategory(WidgetBP, *BrushVarName, nullptr,
                FText::FromString(TEXT("Widget Styles")));
            CreatedVariables.Add(BrushVarName);

            // Tint color
            FEdGraphPinType ColorPinType;
            ColorPinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
            ColorPinType.PinSubCategoryObject = TBaseStructure<FLinearColor>::Get();

            FString TintVarName = StyleName + TEXT("_Tint");
            FBlueprintEditorUtils::AddMemberVariable(WidgetBP, *TintVarName, ColorPinType);
            FBlueprintEditorUtils::SetBlueprintVariableCategory(WidgetBP, *TintVarName, nullptr,
                FText::FromString(TEXT("Widget Styles")));
            CreatedVariables.Add(TintVarName);
        }
        else if (StyleType.Equals(TEXT("ProgressBar"), ESearchCase::IgnoreCase))
        {
            FEdGraphPinType StylePinType;
            StylePinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
            StylePinType.PinSubCategoryObject = FProgressBarStyle::StaticStruct();

            FString ProgressStyleVarName = StyleName + TEXT("_ProgressStyle");
            FBlueprintEditorUtils::AddMemberVariable(WidgetBP, *ProgressStyleVarName, StylePinType);
            FBlueprintEditorUtils::SetBlueprintVariableCategory(WidgetBP, *ProgressStyleVarName, nullptr,
                FText::FromString(TEXT("Widget Styles")));
            CreatedVariables.Add(ProgressStyleVarName);
        }
        else
        {
            // Generic style - create color and margin variables
            FEdGraphPinType ColorPinType;
            ColorPinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
            ColorPinType.PinSubCategoryObject = TBaseStructure<FLinearColor>::Get();

            FString ColorVarName = StyleName + TEXT("_Color");
            FBlueprintEditorUtils::AddMemberVariable(WidgetBP, *ColorVarName, ColorPinType);
            FBlueprintEditorUtils::SetBlueprintVariableCategory(WidgetBP, *ColorVarName, nullptr,
                FText::FromString(TEXT("Widget Styles")));
            CreatedVariables.Add(ColorVarName);

            FEdGraphPinType MarginPinType;
            MarginPinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
            MarginPinType.PinSubCategoryObject = TBaseStructure<FMargin>::Get();

            FString MarginVarName = StyleName + TEXT("_Margin");
            FBlueprintEditorUtils::AddMemberVariable(WidgetBP, *MarginVarName, MarginPinType);
            FBlueprintEditorUtils::SetBlueprintVariableCategory(WidgetBP, *MarginVarName, nullptr,
                FText::FromString(TEXT("Widget Styles")));
            CreatedVariables.Add(MarginVarName);
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
