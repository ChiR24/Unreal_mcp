// Copyright (c) 2025-2026 Winyunq. All rights reserved.
// UmgFileTransformation.h
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "UmgFileTransformation.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogUmgMcp, Log, All);

class UWidget;
class FJsonObject;

/**
 * @brief Handles the "compilation" and "decompilation" of UMG assets to and from JSON.
 *
 * This class provides a set of static utility functions that form the core of the version
 * control workflow. It allows a binary `.uasset` file to be "decompiled" into a human-readable
 * JSON string (ExportUmgAssetToJsonString) and for that JSON string to be "compiled" back
 * into a `.uasset` (ApplyJsonStringToUmgAsset). This enables UMG layouts to be stored, diffed,
 * and merged in source control systems like Git.
 */
UCLASS()
class MCPAUTOMATIONBRIDGE_API UUmgFileTransformation : public UObject
{
    GENERATED_BODY()

public:
    /**
     * Recursively exports a UMG widget and its children to a JSON object.
     * This is a helper function for internal use.
     * @param Widget The UMG widget to export.
     * @return A shared pointer to the FJsonObject representing the widget hierarchy, or nullptr if invalid.
     */
    static TSharedPtr<FJsonObject> ExportWidgetToJson(UWidget* Widget);

    /**
     * Exports a UMG asset (UWidgetBlueprint) to a JSON string.
     * This function loads the asset, converts its widget tree to JSON, and returns the JSON string.
     * @param AssetPath The Unreal Engine asset path (e.g., "/Game/MyWidget.MyMyWidget").
     * @return The JSON string representation of the UMG asset, or an empty string if failed.
     */
    static FString ExportUmgAssetToJsonString(const FString& AssetPath, const FString& TargetWidgetName = TEXT(""));

    /**
     * Applies JSON data to a UMG asset, creating or modifying it.
     * This function will parse the JSON and reconstruct/update the UMG widget tree.
     * @param AssetPath The Unreal Engine asset path to create/modify.
     * @param JsonData The JSON string data to apply.
     * @param TargetWidgetName Optional. The name of the specific widget to replace. If empty or "Root", the entire widget tree (RootWidget) is replaced.
     * @return True if the operation was successful, false otherwise.
     */
    static bool ApplyJsonStringToUmgAsset(const FString& AssetPath, const FString& JsonData, const FString& TargetWidgetName = TEXT(""));

    /**
     * Normalizes JSON keys from camelCase to PascalCase to match C++ UPROPERTY names.
     * This solves the case-sensitivity issue where UE exports JSON with camelCase keys
     * but JsonObjectToUStruct requires PascalCase to match UPROPERTY names.
     * 
     * @param SourceJson The source JSON object with potentially camelCase keys
     * @return A new JSON object with all keys converted to PascalCase
     */
    static TSharedPtr<FJsonObject> NormalizeJsonKeysToPascalCase(const TSharedPtr<FJsonObject>& SourceJson);
};
