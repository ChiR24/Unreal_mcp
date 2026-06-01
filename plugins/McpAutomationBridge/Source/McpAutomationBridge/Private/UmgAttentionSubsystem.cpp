// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#include "FileManage/UmgAttentionSubsystem.h"
#include "Editor.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "WidgetBlueprint.h"
#include "Logging/LogMacros.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "WidgetBlueprintFactory.h"

// Define a log category for easy debugging.
DEFINE_LOG_CATEGORY_STATIC(LogUmgAttention, Log, All);

void UUmgAttentionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
    CachedTargetWidgetBlueprint = nullptr;

	// Register the delegate to be called when an asset is opened.
	if (GEditor)
	{
		UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
		if (AssetEditorSubsystem)
		{
			AssetEditorSubsystem->OnAssetOpenedInEditor().AddUObject(this, &UUmgAttentionSubsystem::HandleAssetOpened);
		}
	}

	UE_LOG(LogUmgAttention, Log, TEXT("UmgAttentionSubsystem Initialized."));
}

void UUmgAttentionSubsystem::Deinitialize()
{
	// Unregister the delegate.
	if (GEditor)
	{
		UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
		if (AssetEditorSubsystem)
		{
			AssetEditorSubsystem->OnAssetOpenedInEditor().RemoveAll(this);
		}
	}

	UE_LOG(LogUmgAttention, Log, TEXT("UmgAttentionSubsystem Deinitialized."));

	Super::Deinitialize();
}

void UUmgAttentionSubsystem::HandleAssetOpened(UObject* Asset, class IAssetEditorInstance* EditorInstance)
{
	if (!Asset)
	{
		return;
	}

	if (UWidgetBlueprint* WidgetBP = Cast<UWidgetBlueprint>(Asset))
	{
		FString AssetPath = WidgetBP->GetPathName();
		UE_LOG(LogUmgAttention, Log, TEXT("UMG Asset Opened: %s"), *AssetPath);

		// Update history
		UmgAssetHistory.Remove(AssetPath);
		UmgAssetHistory.Insert(AssetPath, 0);

        // If the opened asset is our current target, update the cached object pointer for free.
        if (AssetPath == AttentionTargetAssetPath)
        {
            UE_LOG(LogUmgAttention, Log, TEXT("Opened asset matches current attention target. Updating cached object."));
            CachedTargetWidgetBlueprint = WidgetBP;
        }
	}
}

bool UUmgAttentionSubsystem::SetTargetUmgAsset(const FString& AssetPath)
{
    // Validation: Strict check for package path format
    // Must start with /Game, /Engine, or /PluginName (usually /Script or others, but for UMG it's mostly /Game)
    // To be safe, we check if it starts with /
    if (AssetPath.IsEmpty() || !AssetPath.StartsWith(TEXT("/")))
    {
        UE_LOG(LogUmgAttention, Warning, TEXT("SetTargetUmgAsset: Invalid AssetPath '%s'. Must start with '/'. Clearing target."), *AssetPath);
        AttentionTargetAssetPath.Empty();
        CachedTargetWidgetBlueprint = nullptr;
        return false;
    }

    UWidgetBlueprint* TargetBP = nullptr;

    // 1. Try to find the asset in an open editor FIRST
    if (GEditor)
    {
        if (UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>())
        {
             FSoftObjectPath ObjectPath(AssetPath);
             UObject* AssetObj = ObjectPath.ResolveObject();
             
             if (AssetObj)
             {
                 IAssetEditorInstance* Editor = AssetEditorSubsystem->FindEditorForAsset(AssetObj, false);
                 if (Editor)
                 {
                     TargetBP = Cast<UWidgetBlueprint>(AssetObj);
                     if (TargetBP)
                     {
                         UE_LOG(LogUmgAttention, Log, TEXT("SetTargetUmgAsset: Found open editor for %s. Using live instance."), *AssetPath);
                     }
                 }
             }
        }
    }

    // 2. Fallback to LoadObject if not found in editor
    if (!TargetBP)
    {
        // Use LoadObject but handle failure gracefully without blocking ensure
        TargetBP = LoadObject<UWidgetBlueprint>(nullptr, *AssetPath, nullptr, LOAD_NoWarn);
    }

    // 3. Conditional Creation
    // Only create if it looks like a valid path but just missing
    if (!TargetBP)
    {
        // Basic sanity check before creating: does it look like a valid package path?
        if (FPackageName::IsValidPath(AssetPath))
        {
            UE_LOG(LogUmgAttention, Log, TEXT("SetTargetUmgAsset: Asset '%s' not found. Creating new WidgetBlueprint..."), *AssetPath);
            
            FString PackageName = AssetPath;
            FString AssetName = FPaths::GetBaseFilename(AssetPath);
            FString PackagePath = FPaths::GetPath(AssetPath);

            // Ensure AssetTools module is loaded
            IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
            
            // Create a factory for WidgetBlueprint
            UWidgetBlueprintFactory* Factory = NewObject<UWidgetBlueprintFactory>();
            
            UObject* NewAsset = AssetTools.CreateAsset(AssetName, PackagePath, UWidgetBlueprint::StaticClass(), Factory);
            TargetBP = Cast<UWidgetBlueprint>(NewAsset);
            
            if (TargetBP)
            {
                 UE_LOG(LogUmgAttention, Log, TEXT("SetTargetUmgAsset: Successfully created new asset '%s'."), *AssetPath);
            }
            else
            {
                 UE_LOG(LogUmgAttention, Error, TEXT("SetTargetUmgAsset: Failed to create asset '%s'."), *AssetPath);
            }
        }
        else
        {
             UE_LOG(LogUmgAttention, Warning, TEXT("SetTargetUmgAsset: Path '%s' is not a valid package path. Skipping creation."), *AssetPath);
        }
    }

    if (TargetBP)
    {
        // SUCCESS: The path is valid and the object was loaded/found.
        UE_LOG(LogUmgAttention, Log, TEXT("Setting Attention Target Path to: %s"), *AssetPath);
        AttentionTargetAssetPath = AssetPath;
        CachedTargetWidgetBlueprint = TargetBP;
        CurrentWidgetName.Empty(); // Clear stale widget scope when switching assets unless explicitly set later.

        // Also treat setting a target as an 'edit' action to update the history, ensuring consistency.
        UmgAssetHistory.Remove(AssetPath);
        UmgAssetHistory.Insert(AssetPath, 0);
        UE_LOG(LogUmgAttention, Log, TEXT("Successfully cached UMG asset object."));
        return true;
    }
    else
    {
        // FAILURE: The path was invalid. Clear any existing target to prevent errors.
        UE_LOG(LogUmgAttention, Warning, TEXT("Failed to load or create UMG asset from path: %s. Clearing attention target."), *AssetPath);
        AttentionTargetAssetPath.Empty();
        CachedTargetWidgetBlueprint = nullptr;
        return false;
    }
}

FString UUmgAttentionSubsystem::GetTargetUmgAsset() const
{
    // Non-const mutable copy for potential lazy-loading
    UUmgAttentionSubsystem* MutableThis = const_cast<UUmgAttentionSubsystem*>(this);

    // If we have an explicit target path, but our cached object is invalid, try to reload it.
    if (!AttentionTargetAssetPath.IsEmpty() && !CachedTargetWidgetBlueprint.IsValid())
    {
        UE_LOG(LogUmgAttention, Log, TEXT("Cached target object is invalid. Attempting to reload from path: %s"), *AttentionTargetAssetPath);
        UWidgetBlueprint* ReloadedBP = LoadObject<UWidgetBlueprint>(nullptr, *AttentionTargetAssetPath);
        if (ReloadedBP)
        {
            UE_LOG(LogUmgAttention, Log, TEXT("Successfully reloaded and re-cached UMG asset object."));
            MutableThis->CachedTargetWidgetBlueprint = ReloadedBP;
        }
        else
        {
             UE_LOG(LogUmgAttention, Warning, TEXT("Failed to reload UMG asset object."));
        }
    }

    if (!AttentionTargetAssetPath.IsEmpty())
    {
        return AttentionTargetAssetPath;
    }
    
    // NEW: Check currently open UMG editors
    if (GEditor)
    {
        UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
        if (AssetEditorSubsystem)
        {
            TArray<UObject*> EditedAssets = AssetEditorSubsystem->GetAllEditedAssets();
            for (UObject* Asset : EditedAssets)
            {
                if (UWidgetBlueprint* WidgetBP = Cast<UWidgetBlueprint>(Asset))
                {
                    FString AssetPath = WidgetBP->GetPathName();
                    UE_LOG(LogUmgAttention, Log, TEXT("Found currently open UMG editor: %s"), *AssetPath);
                    
                    // Update our cache
                    MutableThis->CachedTargetWidgetBlueprint = WidgetBP;
                    
                    // Return the first found UMG asset
                    return AssetPath;
                }
            }
        }
    }
    
    // Fallback to history
    return GetLastEditedUMGAsset();
}

UWidgetBlueprint* UUmgAttentionSubsystem::GetCachedTargetWidgetBlueprint() const
{
    // Before returning, call GetTargetUmgAsset() to ensure lazy-load/reload logic is triggered.
    GetTargetUmgAsset(); 
    
    if (CachedTargetWidgetBlueprint.IsValid())
    {
        return CachedTargetWidgetBlueprint.Get();
    }

    // If there's no explicit target, maybe the last edited one is what we want.
    if (AttentionTargetAssetPath.IsEmpty())
    {
        FString LastEditedPath = GetLastEditedUMGAsset();
        if (!LastEditedPath.IsEmpty())
        {
            // Non-const mutable copy for lazy-loading
            UUmgAttentionSubsystem* MutableThis = const_cast<UUmgAttentionSubsystem*>(this);
            UWidgetBlueprint* LoadedBP = LoadObject<UWidgetBlueprint>(nullptr, *LastEditedPath);
            if (LoadedBP)
            {
                UE_LOG(LogUmgAttention, Log, TEXT("No explicit target. Lazy loading last edited asset: %s"), *LastEditedPath);
                MutableThis->CachedTargetWidgetBlueprint = LoadedBP;
                return LoadedBP;
            }
        }
    }

    UE_LOG(LogUmgAttention, Warning, TEXT("Returning nullptr because cached target is not valid and could not be loaded."));
    return nullptr;
}


FString UUmgAttentionSubsystem::GetLastEditedUMGAsset() const
{
    if (UmgAssetHistory.Num() > 0)
    {
        return UmgAssetHistory[0];
    }
    return FString();
}

TArray<FString> UUmgAttentionSubsystem::GetRecentlyEditedUMGAssets(int32 MaxCount) const
{
    TArray<FString> Result;
    int32 Count = FMath::Min(UmgAssetHistory.Num(), MaxCount);
    for (int32 i = 0; i < Count; ++i)
    {
        // Return the direct asset path, consistent with GetLastEditedUMGAsset
        Result.Add(UmgAssetHistory[i]);
    }
    return Result;
}

void UUmgAttentionSubsystem::SetTargetAnimation(const FString& AnimationName)
{
	CurrentAnimationName = AnimationName;
	UE_LOG(LogUmgAttention, Log, TEXT("Context: Focused Animation set to '%s'"), *CurrentAnimationName);
}

FString UUmgAttentionSubsystem::GetTargetAnimation() const
{
	return CurrentAnimationName;
}

void UUmgAttentionSubsystem::SetTargetWidget(const FString& WidgetName)
{
	CurrentWidgetName = WidgetName;
	UE_LOG(LogUmgAttention, Log, TEXT("Context: Focused Widget set to '%s'"), *CurrentWidgetName);
}

FString UUmgAttentionSubsystem::GetTargetWidget() const
{
	return CurrentWidgetName;
}

void UUmgAttentionSubsystem::SetTargetGraph(const FString& GraphName)
{
    CurrentGraphName = GraphName;
    // Reset cursor when switching graphs, or maybe try to find the "End" of the graph?
    // For now, simpler is safer: reset to empty so we don't link across graphs.
    LastEditedNodeId.Empty(); 
    CurrentNodePosition = FVector2D(0, 0); 
    UE_LOG(LogUmgAttention, Log, TEXT("Context: Focused Graph set to '%s'"), *CurrentGraphName);
}

FString UUmgAttentionSubsystem::GetTargetGraph() const
{
    // If empty, default to "EventGraph" as a sensible default for UMG
    if (CurrentGraphName.IsEmpty())
    {
        return TEXT("EventGraph");
    }
    return CurrentGraphName;
}

void UUmgAttentionSubsystem::SetCursorNode(const FString& NodeId)
{
    LastEditedNodeId = NodeId;
    UE_LOG(LogUmgAttention, Log, TEXT("Context: Cursor Node set to '%s'"), *LastEditedNodeId);
}

FString UUmgAttentionSubsystem::GetCursorNode() const
{
    return LastEditedNodeId;
}

FVector2D UUmgAttentionSubsystem::GetAndAdvanceCursorPosition()
{
    FVector2D Result = CurrentNodePosition;
    // Simple auto-layout strategy: move right by 250 units
    CurrentNodePosition.X += 250.0f;
    return Result;
}

void UUmgAttentionSubsystem::SetCursorPosition(const FVector2D& NewPosition)
{
    CurrentNodePosition = NewPosition;
}
