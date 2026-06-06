// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "EditorSubsystem.h"
#include "UmgGetSubsystem.generated.h"

// Define a log category for easy debugging.
DECLARE_LOG_CATEGORY_EXTERN(LogUmgGet, Log, All);

/**
 * @brief Provides "sensing" capabilities for the AI to inspect and query UMG assets.
 *
 * This subsystem acts as the "eyes" of the AI agent. It contains functions for reading
 * information from a UMG asset without modifying it. Responsibilities include retrieving the
 * full widget hierarchy (GetWidgetTree), querying specific properties of a widget
 * (QueryWidgetProperties), and gathering layout information. All functions are read-only
 * and are fundamental for the AI to understand the current state of a UI before making changes.
 */
UCLASS(BlueprintType, Blueprintable)
class MCPAUTOMATIONBRIDGE_API UUmgGetSubsystem : public UEditorSubsystem
{
	GENERATED_BODY()

public:
	// UEditorSubsystem interface
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// MCP Get Functions
	UFUNCTION(BlueprintCallable, Category = "UMG MCP|Get")
	FString GetWidgetTree(class UWidgetBlueprint* WidgetBlueprint);

	UFUNCTION(BlueprintCallable, Category = "UMG MCP|Get")
	FString QueryWidgetProperties(class UWidgetBlueprint* WidgetBlueprint, const FString& WidgetName, const TArray<FString>& Properties);

	UFUNCTION(BlueprintCallable, Category = "UMG MCP|Get")
	FString GetLayoutData(class UWidgetBlueprint* WidgetBlueprint, int32 ResolutionWidth = 1920, int32 ResolutionHeight = 1080);

	UFUNCTION(BlueprintCallable, Category = "UMG MCP|Get")
	bool CheckWidgetOverlap(class UWidgetBlueprint* WidgetBlueprint, const TArray<FString>& WidgetIds);

	UFUNCTION(BlueprintCallable, Category = "UMG MCP|Get")
	FString GetAssetFileSystemPath(const FString& AssetPath);

	UFUNCTION(BlueprintCallable, Category = "UMG MCP|Get")
	FString GetWidgetSchema(const FString& WidgetType);
};
