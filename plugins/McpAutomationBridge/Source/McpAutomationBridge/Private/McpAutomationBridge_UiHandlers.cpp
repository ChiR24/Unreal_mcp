// =============================================================================
// McpAutomationBridge_UiHandlers.cpp
// =============================================================================
// Handler implementations for UI/Widget and Editor control operations.
//
// HANDLERS IMPLEMENTED:
// ---------------------
// system_control / manage_ui:
//   - create_widget: Create UMG widget blueprint
//   - add_widget_child: Add child widget to widget tree
//   - screenshot: Capture viewport screenshot with base64 encoding
//   - play_in_editor: Start PIE session
//   - stop_play: Stop PIE session
//   - save_all: Save all assets
//   - simulate_input: Simulate keyboard input events
//   - create_hud: Create and add widget to viewport
//   - set_widget_text: Set text on TextBlock widgets
//   - set_widget_image: Set image on Image widgets
//   - set_widget_visibility: Toggle widget visibility
//   - remove_widget_from_viewport: Remove widgets from viewport
//
// VERSION COMPATIBILITY:
// ----------------------
// UE 5.0: FImageUtils::CompressImageArray (no ThumbnailCompressImageArray)
// UE 5.1+: FImageUtils::ThumbnailCompressImageArray available
// WidgetBlueprintFactory: Header location varies by UE version
//
// SECURITY:
// ---------
// - Screenshot paths validated and sanitized
// - No arbitrary code execution via widget operations
// =============================================================================

// =============================================================================
// Version Compatibility Header (MUST BE FIRST)
// =============================================================================
#include "McpVersionCompatibility.h"

// =============================================================================
// Core Headers
// =============================================================================
#include "Dom/JsonObject.h"
#include "McpAutomationBridgeGlobals.h"
#include "McpHandlerUtils.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSettings.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

// =============================================================================
// Editor-Only Headers
// =============================================================================
#if WITH_EDITOR

// Asset Management
#include "AssetToolsModule.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "EditorAssetLibrary.h"
#include "EditorUtilitySubsystem.h"
#include "EditorUtilityWidget.h"
#include "EditorUtilityWidgetBlueprint.h"
#include "EditorUtilityWidgetBlueprintFactory.h"

// Widget Support
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "WidgetBlueprint.h"

// Engine & Rendering
#include "Engine/GameViewportClient.h"
#include "Engine/Texture2D.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Commands/UIAction.h"
#include "Framework/Docking/TabManager.h"
#include "HAL/FileManager.h"
#include "Misc/ConfigCacheIni.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "ImageUtils.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Base64.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "Styling/AppStyle.h"
#include "ToolMenus.h"
#include "UnrealClient.h"

#if __has_include("Interfaces/IMainFrameModule.h")
#include "Interfaces/IMainFrameModule.h"
#define MCP_HAS_MAINFRAME_MODULE 1
#else
#define MCP_HAS_MAINFRAME_MODULE 0
#endif

#if __has_include("LevelEditor.h")
#include "LevelEditor.h"
#define MCP_HAS_LEVEL_EDITOR_MODULE 1
#else
#define MCP_HAS_LEVEL_EDITOR_MODULE 0
#endif

// Widget Factory (version-dependent header location)
#if __has_include("Factories/WidgetBlueprintFactory.h")
#include "Factories/WidgetBlueprintFactory.h"
#define MCP_HAS_WIDGET_FACTORY 1
#else
#define MCP_HAS_WIDGET_FACTORY 0
#endif

#endif // WITH_EDITOR

#if WITH_EDITOR
namespace
{

  enum class EMcpEditorCommandKind : uint8
  {
    ConsoleCommand,
    OpenAsset,
    RunEditorUtility,
    OpenEditorUtilityWidget,
  };

  struct FMcpEditorCommandDefinition
  {
    FName Name;
    FString Label;
    FString Tooltip;
    FString IconName;
    EMcpEditorCommandKind Kind = EMcpEditorCommandKind::ConsoleCommand;
    FString Command;
    FString AssetPath;
    FString TabId;
  };

  struct FMcpUiDiscoverySettings
  {
    TArray<FString> JsonDefinitionRoots;
    TArray<FString> KnownMenuNames;
    FString JsonToolTabIdPrefix;
  };

  struct FMcpDiscoveredUiTargetDefinition
  {
    FString SourceType;
    FString DisplayName;
    FString Identifier;
    FString TabId;
    FString AssetPath;
    FString DefinitionPath;
    FString StatusFileName;
    FString StateFileName;
    FString Tooltip;
    FString MenuName;
    FString SourceMenuName;
    FString SectionName;
    FString EntryName;
    FString EntryType;
    int32 SectionIndex = INDEX_NONE;
    int32 EntryIndex = INDEX_NONE;
    bool bCanOpenDirectly = false;
    TArray<FString> MenuHierarchy;
    TArray<FString> Aliases;
  };

  static TMap<FName, TSharedPtr<FMcpEditorCommandDefinition>> GMcpEditorCommands;
  static const FName GMcpToolMenuOwnerName(TEXT("McpAutomationBridge"));

  FString NormalizeMcpLookupKey(const FString &Value)
  {
    FString Result;
    Result.Reserve(Value.Len());
    for (const TCHAR Character : Value)
    {
      if (FChar::IsAlnum(Character))
      {
        Result.AppendChar(FChar::ToLower(Character));
      }
    }
    return Result;
  }

  void AddUniqueTrimmedValue(TArray<FString> &Values, const FString &Value)
  {
    const FString Trimmed = Value.TrimStartAndEnd();
    if (!Trimmed.IsEmpty() && !Values.Contains(Trimmed))
    {
      Values.Add(Trimmed);
    }
  }

  FString MakeProjectRelativeDefinitionPath(const FString &ResolvedFilePath)
  {
    FString AbsolutePath = FPaths::ConvertRelativePathToFull(ResolvedFilePath);
    FString ProjectRoot = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
    FPaths::MakeStandardFilename(AbsolutePath);
    FPaths::MakeStandardFilename(ProjectRoot);

    if (!ProjectRoot.EndsWith(TEXT("/")))
    {
      ProjectRoot += TEXT("/");
    }

    if (!AbsolutePath.StartsWith(ProjectRoot))
    {
      return FString();
    }

    return SanitizeProjectFilePath(AbsolutePath.RightChop(ProjectRoot.Len()));
  }

  FName MakeOwnedToolMenuEntryName(const FName EntryName)
  {
    const FString TrimmedName = EntryName.ToString().TrimStartAndEnd();
    if (TrimmedName.StartsWith(TEXT("McpAutomationBridge.")))
    {
      return FName(*TrimmedName);
    }

    return FName(*FString::Printf(TEXT("McpAutomationBridge.%s"), *TrimmedName));
  }

  const FMcpUiDiscoverySettings &GetUiDiscoverySettings()
  {
    static FMcpUiDiscoverySettings Settings;
    Settings.JsonToolTabIdPrefix = TEXT("");
    Settings.JsonDefinitionRoots.Empty();
    Settings.KnownMenuNames.Empty();

    if (const UMcpAutomationBridgeSettings *BridgeSettings =
            GetDefault<UMcpAutomationBridgeSettings>())
    {
      Settings.JsonToolTabIdPrefix =
          BridgeSettings->JsonToolTabIdPrefix.TrimStartAndEnd();

      for (const FString &Root : BridgeSettings->UiDefinitionRoots)
      {
        AddUniqueTrimmedValue(Settings.JsonDefinitionRoots, Root);
      }
      for (const FString &MenuName : BridgeSettings->KnownToolMenuNames)
      {
        AddUniqueTrimmedValue(Settings.KnownMenuNames, MenuName);
      }
    }

    return Settings;
  }

  FString ResolveProjectScopedPath(const FString &RelativePath)
  {
    const FString Normalized = RelativePath.TrimStartAndEnd();
    if (Normalized.IsEmpty())
    {
      return FPaths::ProjectDir();
    }
    if (FPaths::IsRelative(Normalized))
    {
      return FPaths::ConvertRelativePathToFull(
          FPaths::Combine(FPaths::ProjectDir(), Normalized));
    }
    return FPaths::ConvertRelativePathToFull(Normalized);
  }

  bool LoadJsonObjectFromFile(const FString &FilePath,
                              TSharedPtr<FJsonObject> &OutObject)
  {
    FString JsonText;
    if (!FFileHelper::LoadFileToString(JsonText, *FilePath))
    {
      return false;
    }

    const TSharedRef<TJsonReader<>> Reader =
        TJsonReaderFactory<>::Create(JsonText);
    return FJsonSerializer::Deserialize(Reader, OutObject) &&
           OutObject.IsValid();
  }

  FString MakeDefaultStatusFileName(const FString &ToolName)
  {
    FString BaseName = ToolName;
    BaseName.RemoveFromEnd(TEXT("Tool"));
    if (BaseName.IsEmpty())
    {
      BaseName = ToolName;
    }
    return BaseName + TEXT("Status.txt");
  }

  FString MakeDefaultStateFileName(const FString &ToolName)
  {
    FString BaseName = ToolName;
    BaseName.RemoveFromEnd(TEXT("Tool"));
    if (BaseName.IsEmpty())
    {
      BaseName = ToolName;
    }
    return BaseName + TEXT("State.json");
  }

  FName MakeJsonToolTabId(const FString &ToolName,
                          const FString &ExplicitTabId)
  {
    const FString TrimmedExplicitTabId = ExplicitTabId.TrimStartAndEnd();
    if (!TrimmedExplicitTabId.IsEmpty())
    {
      return FName(*TrimmedExplicitTabId);
    }

    const FString Prefix = GetUiDiscoverySettings().JsonToolTabIdPrefix.TrimStartAndEnd();
    return Prefix.IsEmpty()
               ? FName(*ToolName)
               : FName(*FString::Printf(TEXT("%s.%s"), *Prefix, *ToolName));
  }

  TArray<FString> BuildUiTargetAliases(
      const FMcpDiscoveredUiTargetDefinition &Target)
  {
    TArray<FString> Aliases;
    AddUniqueTrimmedValue(Aliases, Target.Identifier);
    AddUniqueTrimmedValue(Aliases, Target.DisplayName);
    AddUniqueTrimmedValue(Aliases, Target.TabId);
    AddUniqueTrimmedValue(Aliases, Target.AssetPath);
    AddUniqueTrimmedValue(Aliases, Target.MenuName);
    AddUniqueTrimmedValue(Aliases, Target.SourceMenuName);
    AddUniqueTrimmedValue(Aliases, Target.SectionName);
    AddUniqueTrimmedValue(Aliases, Target.EntryName);

    FString WithoutSuffix = Target.Identifier;
    WithoutSuffix.RemoveFromEnd(TEXT("Tool"));
    AddUniqueTrimmedValue(Aliases, WithoutSuffix);

    return Aliases;
  }

  FString BuildMenuDisplayName(const FString &MenuName)
  {
    int32 SeparatorIndex = INDEX_NONE;
    if (MenuName.FindLastChar(TEXT('.'), SeparatorIndex) &&
        SeparatorIndex + 1 < MenuName.Len())
    {
      return MenuName.Mid(SeparatorIndex + 1);
    }

    return MenuName;
  }

  FString GetResolvedTextAttribute(const TAttribute<FText> &TextAttribute)
  {
    return TextAttribute.Get().ToString().TrimStartAndEnd();
  }

  FString GetToolMenuEntryDisplayName(const FToolMenuEntry &Entry)
  {
    FString DisplayName = GetResolvedTextAttribute(Entry.Label);
    if (!DisplayName.IsEmpty())
    {
      return DisplayName;
    }

    DisplayName = GetResolvedTextAttribute(Entry.ToolBarData.LabelOverride);
    if (!DisplayName.IsEmpty())
    {
      return DisplayName;
    }

    DisplayName = Entry.Name.ToString().TrimStartAndEnd();
    if (!DisplayName.IsEmpty())
    {
      return DisplayName;
    }

    return TEXT("Unnamed Menu Entry");
  }

  FString GetToolMenuEntryTooltip(const FToolMenuEntry &Entry)
  {
    const FString Tooltip = GetResolvedTextAttribute(Entry.ToolTip);
    if (!Tooltip.IsEmpty())
    {
      return Tooltip;
    }

    return Entry.IsSubMenu() ? TEXT("Runtime ToolMenus submenu")
                             : TEXT("Runtime ToolMenus entry");
  }

  FString DescribeToolMenuEntryType(const FToolMenuEntry &Entry)
  {
    if (Entry.IsSubMenu())
    {
      return TEXT("submenu");
    }

    switch (Entry.Type)
    {
    case EMultiBlockType::MenuEntry:
      return TEXT("menu_entry");
    case EMultiBlockType::ToolBarButton:
      return TEXT("toolbar_button");
    case EMultiBlockType::ToolBarComboButton:
      return TEXT("toolbar_combo_button");
    case EMultiBlockType::Separator:
      return TEXT("separator");
    case EMultiBlockType::Heading:
      return TEXT("heading");
    case EMultiBlockType::EditableText:
      return TEXT("editable_text");
    case EMultiBlockType::Widget:
      return TEXT("widget");
    case EMultiBlockType::ButtonRow:
      return TEXT("button_row");
    case EMultiBlockType::None:
    default:
      return TEXT("menu_block");
    }
  }

  bool CanDirectlyOpenToolMenuEntry(const FToolMenuEntry &Entry)
  {
    return !Entry.IsSubMenu() && Entry.Type != EMultiBlockType::None &&
           Entry.Type != EMultiBlockType::Separator &&
           Entry.Type != EMultiBlockType::Heading &&
           Entry.Type != EMultiBlockType::EditableText &&
           Entry.Type != EMultiBlockType::Widget &&
           Entry.Type != EMultiBlockType::ButtonRow;
  }

  bool RequiresExplicitToolMenuContext(const FString &MenuName)
  {
    return MenuName.Contains(TEXT("ContentBrowser."),
                             ESearchCase::IgnoreCase) ||
           MenuName.Contains(TEXT("ContextMenu"),
                             ESearchCase::IgnoreCase);
  }

  bool BuildToolMenuExecutionContext(const FString &MenuName,
                                     FToolMenuContext &OutContext)
  {
#if MCP_HAS_MAINFRAME_MODULE && MCP_HAS_LEVEL_EDITOR_MODULE
    if (!MenuName.StartsWith(TEXT("LevelEditor.MainMenu.")))
    {
      return false;
    }

    IMainFrameModule *MainFrameModule =
        FModuleManager::GetModulePtr<IMainFrameModule>(TEXT("MainFrame"));
    FLevelEditorModule *LevelEditorModule =
        FModuleManager::GetModulePtr<FLevelEditorModule>(TEXT("LevelEditor"));
    if (!MainFrameModule || !LevelEditorModule)
    {
      return false;
    }

    TSharedPtr<FTabManager> LevelEditorTabManager =
        LevelEditorModule->GetLevelEditorTabManager();
    if (!LevelEditorTabManager.IsValid())
    {
      return false;
    }

    OutContext.AppendCommandList(LevelEditorModule->GetGlobalLevelEditorActions());
    MainFrameModule->MakeMainMenu(LevelEditorTabManager, FName(*MenuName),
                                  OutContext);
    return true;
#else
    return false;
#endif
  }

  FString MakeToolMenuPath(const FString &BaseMenuName, const FName &EntryName)
  {
    if (BaseMenuName.IsEmpty() || EntryName.IsNone())
    {
      return FString();
    }

    return UToolMenus::JoinMenuPaths(FName(*BaseMenuName), EntryName).ToString();
  }

  FString MakeToolMenuEntryIdentifier(const FString &RequestedMenuName,
                                      const FString &SourceMenuName,
                                      const FToolMenuSection &Section,
                                      const int32 SectionIndex,
                                      const int32 EntryIndex)
  {
    const FString SectionKey = Section.Name.IsNone()
                                   ? FString::Printf(TEXT("section-%d"), SectionIndex)
                                   : Section.Name.ToString();
    return FString::Printf(TEXT("%s::%s::%s::%d"), *RequestedMenuName,
                           *SourceMenuName, *SectionKey, EntryIndex);
  }

  void AppendToolMenuEntryTargets(
      const FString &RequestedMenuName, const UToolMenu *Menu,
      TArray<FMcpDiscoveredUiTargetDefinition> &OutTargets)
  {
    if (!Menu)
    {
      return;
    }

    const FString MenuDisplayName = BuildMenuDisplayName(RequestedMenuName);
    const FString SourceMenuName = Menu->GetMenuName().ToString();
    TArray<FString> MenuHierarchy;
    for (const FName &HierarchyName : Menu->GetMenuHierarchyNames(true))
    {
      MenuHierarchy.Add(HierarchyName.ToString());
    }

    for (int32 SectionIndex = 0; SectionIndex < Menu->Sections.Num();
         ++SectionIndex)
    {
      const FToolMenuSection &Section = Menu->Sections[SectionIndex];
      const FString SectionName = Section.Name.ToString();

      for (int32 EntryIndex = 0; EntryIndex < Section.Blocks.Num(); ++EntryIndex)
      {
        const FToolMenuEntry &Entry = Section.Blocks[EntryIndex];

        FMcpDiscoveredUiTargetDefinition Target;
        Target.SourceType = TEXT("tool_menu_entry");
        Target.Identifier = MakeToolMenuEntryIdentifier(RequestedMenuName,
                                                        SourceMenuName,
                                                        Section, SectionIndex,
                                                        EntryIndex);
        Target.DisplayName = GetToolMenuEntryDisplayName(Entry);
        Target.Tooltip = GetToolMenuEntryTooltip(Entry);
        Target.MenuName = RequestedMenuName;
        Target.SourceMenuName = SourceMenuName;
        Target.SectionName = SectionName;
        Target.EntryName = Entry.Name.ToString();
        Target.EntryType = DescribeToolMenuEntryType(Entry);
        Target.SectionIndex = SectionIndex;
        Target.EntryIndex = EntryIndex;
        Target.bCanOpenDirectly =
            CanDirectlyOpenToolMenuEntry(Entry) &&
            !RequiresExplicitToolMenuContext(RequestedMenuName) &&
            !RequiresExplicitToolMenuContext(SourceMenuName);
        Target.MenuHierarchy = MenuHierarchy;
        Target.Aliases = BuildUiTargetAliases(Target);
        AddUniqueTrimmedValue(Target.Aliases,
                              FString::Printf(TEXT("%s > %s"), *MenuDisplayName,
                                              *Target.DisplayName));
        AddUniqueTrimmedValue(Target.Aliases,
                              FString::Printf(TEXT("%s > %s"),
                                              *RequestedMenuName,
                                              *Target.DisplayName));
        if (!SectionName.IsEmpty())
        {
          AddUniqueTrimmedValue(Target.Aliases,
                                FString::Printf(TEXT("%s > %s > %s"),
                                                *MenuDisplayName, *SectionName,
                                                *Target.DisplayName));
        }

        OutTargets.Add(Target);
      }
    }
  }

  bool ExecuteToolMenuEntryTarget(
      const FMcpDiscoveredUiTargetDefinition &Target, FString &OutMessage,
      FString &OutErrorCode)
  {
    if (!FSlateApplication::IsInitialized())
    {
      OutMessage = TEXT("Slate application is not initialized");
      OutErrorCode = TEXT("SLATE_NOT_AVAILABLE");
      return false;
    }

    if (Target.MenuName.IsEmpty() || Target.SectionIndex == INDEX_NONE ||
        Target.EntryIndex == INDEX_NONE)
    {
      OutMessage =
          FString::Printf(TEXT("UI target %s is missing ToolMenus metadata"),
                          *Target.Identifier);
      OutErrorCode = TEXT("EXECUTION_FAILED");
      return false;
    }

    if (!Target.bCanOpenDirectly)
    {
      OutMessage = FString::Printf(TEXT("UI target %s cannot be opened directly"),
                                   *Target.Identifier);
      OutErrorCode = TEXT("INVALID_ARGUMENT");
      return false;
    }

    UToolMenus *ToolMenus = UToolMenus::TryGet();
    if (!ToolMenus)
    {
      OutMessage = TEXT("ToolMenus subsystem is not available");
      OutErrorCode = TEXT("EDITOR_NOT_AVAILABLE");
      return false;
    }

    const FString SourceMenuName = Target.SourceMenuName.IsEmpty()
                                       ? Target.MenuName
                                       : Target.SourceMenuName;
    FToolMenuContext ExecutionContext;
    const bool bHasExecutionContext =
        BuildToolMenuExecutionContext(Target.MenuName, ExecutionContext);

    auto ResolveSection = [&Target](UToolMenu *CandidateMenu)
        -> FToolMenuSection *
    {
      if (!CandidateMenu)
      {
        return nullptr;
      }

      if (!Target.SectionName.IsEmpty())
      {
        if (FToolMenuSection *NamedSection =
                CandidateMenu->FindSection(FName(*Target.SectionName)))
        {
          return NamedSection;
        }
      }

      if (CandidateMenu->Sections.IsValidIndex(Target.SectionIndex))
      {
        return &CandidateMenu->Sections[Target.SectionIndex];
      }

      return nullptr;
    };

    auto ResolveEntry = [&Target, &ResolveSection](UToolMenu *CandidateMenu)
        -> FToolMenuEntry *
    {
      FToolMenuSection *Section = ResolveSection(CandidateMenu);
      if (!Section)
      {
        return nullptr;
      }

      if (!Target.EntryName.IsEmpty())
      {
        if (FToolMenuEntry *NamedEntry =
                Section->FindEntry(FName(*Target.EntryName)))
        {
          return NamedEntry;
        }
      }

      if (Section->Blocks.IsValidIndex(Target.EntryIndex))
      {
        return &Section->Blocks[Target.EntryIndex];
      }

      for (FToolMenuEntry &Entry : Section->Blocks)
      {
        if (GetToolMenuEntryDisplayName(Entry).Equals(Target.DisplayName,
                                                      ESearchCase::CaseSensitive))
        {
          return &Entry;
        }
      }

      return nullptr;
    };

    auto TryExecuteAction = [&OutMessage, &OutErrorCode,
                             &Target](const FUIAction &Action,
                                      const TCHAR *FailureReason) -> bool
    {
      if (!Action.ExecuteAction.IsBound())
      {
        return false;
      }

      bool bCanExecute = true;
      if (Action.CanExecuteAction.IsBound())
      {
        bCanExecute = Action.CanExecuteAction.Execute();
      }

      if (!bCanExecute)
      {
        OutMessage = FString::Printf(
            TEXT("UI target %s is not executable in the current editor context (%s)"),
            *Target.DisplayName, FailureReason);
        OutErrorCode = TEXT("EXECUTION_FAILED");
        return false;
      }

      Action.ExecuteAction.Execute();
      return true;
    };

    auto TryExecuteFromMenu =
        [&Target, &OutMessage, &OutErrorCode, &ResolveSection, &ResolveEntry,
         &TryExecuteAction](UToolMenu *CandidateMenu,
                            const FString &CandidateMenuName) -> bool
    {
      if (!CandidateMenu)
      {
        return false;
      }

      FToolMenuSection *CandidateSection = ResolveSection(CandidateMenu);
      if (!CandidateSection)
      {
        OutMessage = FString::Printf(
            TEXT("UI target %s section '%s' was not found in menu %s"),
            *Target.DisplayName, *Target.SectionName, *CandidateMenuName);
        OutErrorCode = TEXT("NOT_FOUND");
        return false;
      }

      FToolMenuEntry *CandidateEntry = ResolveEntry(CandidateMenu);
      if (!CandidateEntry)
      {
        OutMessage = FString::Printf(
            TEXT("UI target %s entry '%s' was not found in menu %s"),
            *Target.DisplayName, *Target.EntryName, *CandidateMenuName);
        OutErrorCode = TEXT("NOT_FOUND");
        return false;
      }

      if (CandidateEntry->TryExecuteToolUIAction(CandidateMenu->Context))
      {
        return true;
      }

      TSharedPtr<const FUICommandList> CommandList;
      if (const FUIAction *CommandAction =
              CandidateEntry->GetActionForCommand(CandidateMenu->Context,
                                                  CommandList))
      {
        if (TryExecuteAction(*CommandAction, TEXT("command binding")))
        {
          return true;
        }
      }

      OutMessage = FString::Printf(
          TEXT("UI target %s has no executable tool or command action in menu %s"),
          *Target.DisplayName, *CandidateMenuName);
      OutErrorCode = TEXT("EXECUTION_FAILED");
      return false;
    };

    const bool bRequestedMenuNeedsContext =
        RequiresExplicitToolMenuContext(Target.MenuName);
    const bool bSourceMenuNeedsContext =
        RequiresExplicitToolMenuContext(SourceMenuName);

    if (!bRequestedMenuNeedsContext)
    {
      if (UToolMenu *GeneratedRequestedMenu =
              ToolMenus->GenerateMenu(FName(*Target.MenuName),
                                      bHasExecutionContext ? ExecutionContext
                                                           : FToolMenuContext()))
      {
        if (TryExecuteFromMenu(GeneratedRequestedMenu, Target.MenuName))
        {
          OutMessage = FString::Printf(TEXT("Executed UI target %s"),
                                       *Target.DisplayName);
          return true;
        }
      }
    }

    if (UToolMenu *RequestedMenu = ToolMenus->FindMenu(FName(*Target.MenuName)))
    {
      if (TryExecuteFromMenu(RequestedMenu, Target.MenuName))
      {
        OutMessage = FString::Printf(TEXT("Executed UI target %s"),
                                     *Target.DisplayName);
        return true;
      }
    }

    if (UToolMenu *SourceMenu = ToolMenus->FindMenu(FName(*SourceMenuName)))
    {
      if (TryExecuteFromMenu(SourceMenu, SourceMenuName))
      {
        OutMessage = FString::Printf(TEXT("Executed UI target %s"),
                                     *Target.DisplayName);
        return true;
      }
    }

    if (!bSourceMenuNeedsContext && SourceMenuName != Target.MenuName)
    {
      if (UToolMenu *GeneratedSourceMenu =
              ToolMenus->GenerateMenu(FName(*SourceMenuName),
                                      bHasExecutionContext ? ExecutionContext
                                                           : FToolMenuContext()))
      {
        if (TryExecuteFromMenu(GeneratedSourceMenu, SourceMenuName))
        {
          OutMessage = FString::Printf(TEXT("Executed UI target %s"),
                                       *Target.DisplayName);
          return true;
        }
      }
    }

    if (OutErrorCode.IsEmpty())
    {
      OutMessage = FString::Printf(
          TEXT("Failed to execute UI target %s from menus %s and %s"),
          *Target.DisplayName, *Target.MenuName, *SourceMenuName);
      OutErrorCode = TEXT("EXECUTION_FAILED");
    }

    return false;
  }

  TArray<FMcpDiscoveredUiTargetDefinition> DiscoverJsonUiTargets()
  {
    TArray<FMcpDiscoveredUiTargetDefinition> Result;
    for (const FString &ConfiguredRoot : GetUiDiscoverySettings().JsonDefinitionRoots)
    {
      const FString UiRoot = ResolveProjectScopedPath(ConfiguredRoot);
      if (!IFileManager::Get().DirectoryExists(*UiRoot))
      {
        continue;
      }

      TArray<FString> JsonFiles;
      IFileManager::Get().FindFiles(JsonFiles,
                                    *FPaths::Combine(UiRoot, TEXT("*.json")),
                                    true, false);
      JsonFiles.Sort();

      for (const FString &JsonFileName : JsonFiles)
      {
        const FString JsonPath = FPaths::Combine(UiRoot, JsonFileName);
        TSharedPtr<FJsonObject> RootObject;
        if (!LoadJsonObjectFromFile(JsonPath, RootObject))
        {
          continue;
        }

        FMcpDiscoveredUiTargetDefinition Target;
        const FString ToolName = FPaths::GetBaseFilename(JsonFileName);
        Target.SourceType = TEXT("json_tool");
        Target.Identifier = ToolName;
        Target.DefinitionPath = MakeProjectRelativeDefinitionPath(JsonPath);
        Target.DisplayName = ToolName;
        Target.DisplayName.RemoveFromEnd(TEXT("Tool"));
        Target.StatusFileName = MakeDefaultStatusFileName(ToolName);
        Target.StateFileName = MakeDefaultStateFileName(ToolName);
        Target.Tooltip =
            FString::Printf(TEXT("Open the %s tool."), *Target.DisplayName);

        FString ExplicitTabId;
        RootObject->TryGetStringField(TEXT("TabLabel"), Target.DisplayName);
        RootObject->TryGetStringField(TEXT("TabId"), ExplicitTabId);
        RootObject->TryGetStringField(TEXT("StatusFile"), Target.StatusFileName);
        RootObject->TryGetStringField(TEXT("StateFile"), Target.StateFileName);
        RootObject->TryGetStringField(TEXT("Tooltip"), Target.Tooltip);

        Target.TabId = MakeJsonToolTabId(ToolName, ExplicitTabId).ToString();
        Target.Aliases = BuildUiTargetAliases(Target);
        Result.Add(Target);
      }
    }

    Result.Sort([](const FMcpDiscoveredUiTargetDefinition &Left,
                   const FMcpDiscoveredUiTargetDefinition &Right)
                {
      const int32 LabelCompare =
          Left.DisplayName.Compare(Right.DisplayName, ESearchCase::IgnoreCase);
      return LabelCompare == 0
                 ? Left.Identifier.Compare(Right.Identifier,
                                           ESearchCase::IgnoreCase) < 0
                 : LabelCompare < 0; });
    return Result;
  }

  TArray<FMcpDiscoveredUiTargetDefinition> DiscoverKnownToolMenuTargets(
      const TArray<FString> &AdditionalMenuNames,
      TArray<FString> &OutResolvedMenuNames,
      TArray<FString> &OutMissingMenuNames)
  {
    TArray<FMcpDiscoveredUiTargetDefinition> Result;

    TArray<FString> RequestedMenuNames;
    for (const FString &ConfiguredMenuName : GetUiDiscoverySettings().KnownMenuNames)
    {
      AddUniqueTrimmedValue(RequestedMenuNames, ConfiguredMenuName);
    }
    for (const FString &RequestedMenuName : AdditionalMenuNames)
    {
      AddUniqueTrimmedValue(RequestedMenuNames, RequestedMenuName);
    }

    UToolMenus *ToolMenus = UToolMenus::TryGet();
    if (!ToolMenus)
    {
      return Result;
    }

    for (const FString &RequestedMenuName : RequestedMenuNames)
    {
      const FName MenuName(*RequestedMenuName);
      UToolMenu *Menu = ToolMenus->FindMenu(MenuName);
      if (!Menu)
      {
        AddUniqueTrimmedValue(OutMissingMenuNames, RequestedMenuName);
        continue;
      }

      FMcpDiscoveredUiTargetDefinition Target;
      Target.SourceType = TEXT("tool_menu");
      Target.Identifier = RequestedMenuName;
      Target.DisplayName = BuildMenuDisplayName(RequestedMenuName);
      Target.MenuName = RequestedMenuName;
      Target.SourceMenuName = RequestedMenuName;
      Target.Tooltip = TEXT("Known ToolMenus entry point");

      const TArray<FName> HierarchyNames = Menu->GetMenuHierarchyNames(true);
      for (const FName &HierarchyName : HierarchyNames)
      {
        Target.MenuHierarchy.Add(HierarchyName.ToString());
      }

      Target.Aliases = BuildUiTargetAliases(Target);
      Result.Add(Target);

      const TArray<UToolMenu *> HierarchyMenus = ToolMenus->CollectHierarchy(MenuName);
      for (UToolMenu *HierarchyMenu : HierarchyMenus)
      {
        AppendToolMenuEntryTargets(RequestedMenuName, HierarchyMenu, Result);
      }
      AddUniqueTrimmedValue(OutResolvedMenuNames, RequestedMenuName);
    }

    return Result;
  }

  TArray<FMcpDiscoveredUiTargetDefinition> DiscoverMainMenuDropdownTargets(
      TArray<FString> &OutDiscoveredMenuNames)
  {
    TArray<FMcpDiscoveredUiTargetDefinition> Result;

    UToolMenus *ToolMenus = UToolMenus::TryGet();
    if (!ToolMenus)
    {
      return Result;
    }

    const FString MenuBarName = TEXT("LevelEditor.MainMenu");
    FToolMenuContext MenuContext;
    const bool bHasMenuContext =
        BuildToolMenuExecutionContext(MenuBarName, MenuContext);

    UToolMenu *GeneratedMenuBar = nullptr;
    if (bHasMenuContext)
    {
      GeneratedMenuBar = ToolMenus->GenerateMenu(FName(*MenuBarName), MenuContext);
    }

    TArray<UToolMenu *> MenuBars = ToolMenus->CollectHierarchy(FName(*MenuBarName));
    if (GeneratedMenuBar && !MenuBars.Contains(GeneratedMenuBar))
    {
      MenuBars.Add(GeneratedMenuBar);
    }
    if (MenuBars.Num() == 0)
    {
      if (UToolMenu *FoundMenuBar = ToolMenus->FindMenu(FName(*MenuBarName)))
      {
        MenuBars.Add(FoundMenuBar);
      }
    }
    if (MenuBars.Num() == 0)
    {
      return Result;
    }

    for (UToolMenu *MenuBar : MenuBars)
    {
      if (!MenuBar)
      {
        continue;
      }

      for (const FToolMenuSection &Section : MenuBar->Sections)
      {
        for (const FToolMenuEntry &Entry : Section.Blocks)
        {
          if (!Entry.IsSubMenu())
          {
            continue;
          }

          const FString CandidateMenuName = MakeToolMenuPath(MenuBarName, Entry.Name);
          const FString DisplayName = GetToolMenuEntryDisplayName(Entry);
          if (DisplayName.IsEmpty())
          {
            continue;
          }

          FMcpDiscoveredUiTargetDefinition Target;
          Target.SourceType = TEXT("tool_menu");
          Target.Identifier = CandidateMenuName.IsEmpty()
                                  ? FString::Printf(TEXT("%s::%s"), *MenuBarName,
                                                    *DisplayName)
                                  : CandidateMenuName;
          Target.DisplayName = DisplayName;
          Target.MenuName = CandidateMenuName;
          Target.SourceMenuName = CandidateMenuName;
          Target.EntryName = Entry.Name.ToString();
          Target.EntryType = TEXT("submenu");
          Target.Tooltip = GetToolMenuEntryTooltip(Entry);
          Target.MenuHierarchy.Add(MenuBar->GetMenuName().ToString());

          const FName CandidateMenuFName(*CandidateMenuName);
          UToolMenu *ResolvedMenu = nullptr;
          if (!CandidateMenuName.IsEmpty())
          {
            ResolvedMenu = ToolMenus->FindMenu(CandidateMenuFName);
            if (!ResolvedMenu && bHasMenuContext &&
                !RequiresExplicitToolMenuContext(CandidateMenuName))
            {
              ResolvedMenu = ToolMenus->GenerateMenu(CandidateMenuFName, MenuContext);
            }
          }

          if (ResolvedMenu)
          {
            Target.MenuHierarchy.Reset();
            for (const FName &HierarchyName :
                 ResolvedMenu->GetMenuHierarchyNames(true))
            {
              Target.MenuHierarchy.Add(HierarchyName.ToString());
            }
            AddUniqueTrimmedValue(OutDiscoveredMenuNames, CandidateMenuName);
          }

          Target.Aliases = BuildUiTargetAliases(Target);
          AddUniqueTrimmedValue(Target.Aliases,
                                FString::Printf(TEXT("%s > %s"), *MenuBarName,
                                                *Target.DisplayName));
          if (Entry.Name == TEXT("Actions"))
          {
            AddUniqueTrimmedValue(Target.Aliases, TEXT("Actor"));
            AddUniqueTrimmedValue(Target.Aliases, TEXT("Actions"));
            AddUniqueTrimmedValue(Target.Aliases, TEXT("Actor Menu"));
          }

          const bool bAlreadyPresent = Result.ContainsByPredicate(
              [&Target](const FMcpDiscoveredUiTargetDefinition &ExistingTarget)
              {
                return ExistingTarget.SourceType == Target.SourceType &&
                       ExistingTarget.Identifier.Equals(Target.Identifier,
                                                        ESearchCase::IgnoreCase);
              });
          if (!bAlreadyPresent)
          {
            Result.Add(Target);
          }
        }
      }
    }

    return Result;
  }

  const FMcpDiscoveredUiTargetDefinition *FindDiscoveredUiTargetByIdentifier(
      const TArray<FMcpDiscoveredUiTargetDefinition> &Targets,
      const FString &Identifier)
  {
    const FString LookupKey = NormalizeMcpLookupKey(Identifier);
    if (LookupKey.IsEmpty())
    {
      return nullptr;
    }

    for (const FMcpDiscoveredUiTargetDefinition &Target : Targets)
    {
      for (const FString &Alias : Target.Aliases)
      {
        if (NormalizeMcpLookupKey(Alias) == LookupKey)
        {
          return &Target;
        }
      }
    }

    return nullptr;
  }

  TArray<const FMcpDiscoveredUiTargetDefinition *> FindDiscoveredUiTargetsByIdentifier(
      const TArray<FMcpDiscoveredUiTargetDefinition> &Targets,
      const FString &Identifier)
  {
    TArray<const FMcpDiscoveredUiTargetDefinition *> Result;
    const FString LookupKey = NormalizeMcpLookupKey(Identifier);
    if (LookupKey.IsEmpty())
    {
      return Result;
    }

    for (const FMcpDiscoveredUiTargetDefinition &Target : Targets)
    {
      for (const FString &Alias : Target.Aliases)
      {
        if (NormalizeMcpLookupKey(Alias) == LookupKey)
        {
          Result.Add(&Target);
          break;
        }
      }
    }

    return Result;
  }

  bool IsDiscoveredUiTargetOpenable(const FMcpDiscoveredUiTargetDefinition &Target)
  {
    return Target.SourceType == TEXT("registered_command") ||
           Target.SourceType == TEXT("tool_menu_entry") ||
           !Target.TabId.IsEmpty();
  }

  TSharedPtr<SDockTab> FindLiveDockTabById(const FString &TabId)
  {
    const FString TrimmedTabId = TabId.TrimStartAndEnd();
    if (TrimmedTabId.IsEmpty())
    {
      return nullptr;
    }

    return FGlobalTabmanager::Get()->FindExistingLiveTab(FName(*TrimmedTabId));
  }

  void ApplyWindowBoundsToResponse(const TSharedPtr<SWindow> &Window,
                                   const TSharedPtr<FJsonObject> &Response)
  {
    if (!Window.IsValid() || !Response.IsValid())
    {
      return;
    }

    const FSlateRect WindowRect = Window->GetRectInScreen();
    const FSlateRect ClientRect = Window->GetClientRectInScreen();
    const FVector2D WindowSize = Window->GetSizeInScreen();
    const FVector2D ClientSize = Window->GetClientSizeInScreen();

    Response->SetNumberField(TEXT("x"), WindowRect.Left);
    Response->SetNumberField(TEXT("y"), WindowRect.Top);
    Response->SetNumberField(TEXT("width"), WindowSize.X);
    Response->SetNumberField(TEXT("height"), WindowSize.Y);
    Response->SetNumberField(TEXT("clientX"), ClientRect.Left);
    Response->SetNumberField(TEXT("clientY"), ClientRect.Top);
    Response->SetNumberField(TEXT("clientWidth"), ClientSize.X);
    Response->SetNumberField(TEXT("clientHeight"), ClientSize.Y);
  }

  void ApplyRequestedUiTargetFields(const FString &RequestedIdentifier,
                                    const FString &RequestedTabId,
                                    const FString &RequestedWindowTitle,
                                    const TSharedPtr<FJsonObject> &Response)
  {
    if (!Response.IsValid())
    {
      return;
    }

    if (!RequestedIdentifier.IsEmpty())
    {
      Response->SetStringField(TEXT("requestedIdentifier"), RequestedIdentifier);
    }

    if (!RequestedTabId.IsEmpty())
    {
      Response->SetStringField(TEXT("requestedTabId"), RequestedTabId);
    }

    if (!RequestedWindowTitle.IsEmpty())
    {
      Response->SetStringField(TEXT("requestedWindowTitle"), RequestedWindowTitle);
    }
  }

  FName MakeEditorUtilityRegistrationId(const UObject *Asset,
                                        const FString &RequestedTabId)
  {
    if (!Asset)
    {
      return NAME_None;
    }

    const FString AssetPath = Asset->GetPathName();
    const FString TrimmedTabId = RequestedTabId.TrimStartAndEnd();
    if (!TrimmedTabId.IsEmpty() &&
        TrimmedTabId.StartsWith(AssetPath, ESearchCase::CaseSensitive))
    {
      return FName(*TrimmedTabId);
    }

    return TrimmedTabId.IsEmpty()
               ? FName(*(AssetPath + TEXT("_ActiveTab")))
               : FName(*(AssetPath + TrimmedTabId));
  }

  bool CloseSlateTabById(const FName TabId)
  {
    if (TabId.IsNone())
    {
      return false;
    }

    const TSharedPtr<SDockTab> LiveTab =
        FGlobalTabmanager::Get()->FindExistingLiveTab(TabId);
    if (!LiveTab.IsValid())
    {
      return false;
    }

    LiveTab->RequestCloseTab();
    return true;
  }

  TSharedPtr<FMcpEditorCommandDefinition> BuildEditorCommandDefinition(
      const TSharedPtr<FJsonObject> &Payload, const FName DefaultName,
      FString &OutError)
  {
    TSharedPtr<FMcpEditorCommandDefinition> Definition =
        MakeShared<FMcpEditorCommandDefinition>();
    Definition->Name = DefaultName;

    FString Label;
    Payload->TryGetStringField(TEXT("label"), Label);
    Definition->Label = Label.IsEmpty() ? DefaultName.ToString() : Label;

    Payload->TryGetStringField(TEXT("tooltip"), Definition->Tooltip);
    Payload->TryGetStringField(TEXT("icon"), Definition->IconName);

    FString KindString;
    Payload->TryGetStringField(TEXT("commandType"), KindString);
    KindString = KindString.ToLower();

    if (KindString.IsEmpty())
    {
      if (Payload->HasField(TEXT("assetPath")))
      {
        Definition->Kind = EMcpEditorCommandKind::OpenAsset;
      }
      else if (Payload->HasField(TEXT("utilityPath")) ||
               Payload->HasField(TEXT("editorUtilityPath")))
      {
        Definition->Kind = EMcpEditorCommandKind::RunEditorUtility;
      }
      else if (Payload->HasField(TEXT("widgetPath")) ||
               Payload->HasField(TEXT("editorUtilityWidgetPath")))
      {
        Definition->Kind = EMcpEditorCommandKind::OpenEditorUtilityWidget;
      }
      else
      {
        Definition->Kind = EMcpEditorCommandKind::ConsoleCommand;
      }
    }
    else if (KindString == TEXT("open_asset"))
    {
      Definition->Kind = EMcpEditorCommandKind::OpenAsset;
    }
    else if (KindString == TEXT("run_editor_utility"))
    {
      Definition->Kind = EMcpEditorCommandKind::RunEditorUtility;
    }
    else if (KindString == TEXT("open_editor_utility_widget"))
    {
      Definition->Kind = EMcpEditorCommandKind::OpenEditorUtilityWidget;
    }
    else
    {
      Definition->Kind = EMcpEditorCommandKind::ConsoleCommand;
    }

    Payload->TryGetStringField(TEXT("command"), Definition->Command);
    Payload->TryGetStringField(TEXT("assetPath"), Definition->AssetPath);
    if (Definition->AssetPath.IsEmpty())
    {
      Payload->TryGetStringField(TEXT("utilityPath"), Definition->AssetPath);
    }
    if (Definition->AssetPath.IsEmpty())
    {
      Payload->TryGetStringField(TEXT("editorUtilityPath"), Definition->AssetPath);
    }
    if (Definition->AssetPath.IsEmpty())
    {
      Payload->TryGetStringField(TEXT("widgetPath"), Definition->AssetPath);
    }
    if (Definition->AssetPath.IsEmpty())
    {
      Payload->TryGetStringField(TEXT("editorUtilityWidgetPath"), Definition->AssetPath);
    }
    Payload->TryGetStringField(TEXT("tabId"), Definition->TabId);

    if (Definition->Kind == EMcpEditorCommandKind::ConsoleCommand &&
        Definition->Command.IsEmpty())
    {
      OutError = TEXT("command is required for console-backed editor commands");
      return nullptr;
    }

    if ((Definition->Kind == EMcpEditorCommandKind::OpenAsset ||
         Definition->Kind == EMcpEditorCommandKind::RunEditorUtility ||
         Definition->Kind == EMcpEditorCommandKind::OpenEditorUtilityWidget) &&
        Definition->AssetPath.IsEmpty())
    {
      OutError = TEXT("assetPath, utilityPath, or widgetPath is required for this editor command");
      return nullptr;
    }

    return Definition;
  }

  /** @brief Contract: opens an asset editor for a resolved asset path and reports editor-availability failures distinctly from execution failures. */
  bool OpenAssetEditorByPath(const FString &AssetPath, FString &OutMessage,
                             FString &OutErrorCode)
  {
    if (!UEditorAssetLibrary::DoesAssetExist(AssetPath))
    {
      OutMessage = FString::Printf(TEXT("Asset not found: %s"), *AssetPath);
      OutErrorCode = TEXT("NOT_FOUND");
      return false;
    }

    UObject *Asset = UEditorAssetLibrary::LoadAsset(AssetPath);
    if (!Asset)
    {
      OutMessage = FString::Printf(TEXT("Failed to load asset: %s"), *AssetPath);
      OutErrorCode = TEXT("EXECUTION_FAILED");
      return false;
    }

    if (!GEditor)
    {
      OutMessage = TEXT("Editor is not available");
      OutErrorCode = TEXT("EDITOR_NOT_AVAILABLE");
      return false;
    }

    UAssetEditorSubsystem *AssetEditorSubsystem =
        GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
    if (!AssetEditorSubsystem)
    {
      OutMessage = TEXT("AssetEditorSubsystem not available");
      OutErrorCode = TEXT("EDITOR_NOT_AVAILABLE");
      return false;
    }

    if (!AssetEditorSubsystem->OpenEditorForAsset(Asset))
    {
      OutMessage = FString::Printf(TEXT("Failed to open asset editor for %s"), *AssetPath);
      OutErrorCode = TEXT("EXECUTION_FAILED");
      return false;
    }

    OutMessage = FString::Printf(TEXT("Opened asset editor for %s"), *AssetPath);
    return true;
  }

  /** @brief Contract: runs or opens a resolved editor utility asset while preserving the specific reason the target could not execute. */
  bool RunEditorUtilityAsset(const FString &AssetPath, const FString &RequestedTabId,
                             const bool bRequireWidget, FString &OutMessage,
                             FString &OutExecutedTabId,
                             FString &OutErrorCode)
  {
    if (!UEditorAssetLibrary::DoesAssetExist(AssetPath))
    {
      OutMessage = FString::Printf(TEXT("Editor utility asset not found: %s"), *AssetPath);
      OutErrorCode = TEXT("NOT_FOUND");
      return false;
    }

    UObject *Asset = UEditorAssetLibrary::LoadAsset(AssetPath);
    if (!Asset)
    {
      OutMessage = FString::Printf(TEXT("Failed to load editor utility asset: %s"), *AssetPath);
      OutErrorCode = TEXT("EXECUTION_FAILED");
      return false;
    }

    if (!GEditor)
    {
      OutMessage = TEXT("Editor is not available");
      OutErrorCode = TEXT("EDITOR_NOT_AVAILABLE");
      return false;
    }

    UEditorUtilitySubsystem *UtilitySubsystem =
        GEditor->GetEditorSubsystem<UEditorUtilitySubsystem>();
    if (!UtilitySubsystem)
    {
      OutMessage = TEXT("EditorUtilitySubsystem not available");
      OutErrorCode = TEXT("EDITOR_NOT_AVAILABLE");
      return false;
    }

    if (UEditorUtilityWidgetBlueprint *WidgetBlueprint =
            Cast<UEditorUtilityWidgetBlueprint>(Asset))
    {
      FName RegistrationId =
          MakeEditorUtilityRegistrationId(WidgetBlueprint, RequestedTabId);
      bool bOpened = false;
      if (RequestedTabId.TrimStartAndEnd().IsEmpty())
      {
        UEditorUtilityWidget *Widget =
            UtilitySubsystem->SpawnAndRegisterTabAndGetID(WidgetBlueprint,
                                                          RegistrationId);
        bOpened = Widget != nullptr || UtilitySubsystem->DoesTabExist(RegistrationId);
      }
      else
      {
        RegistrationId = FName(*RequestedTabId.TrimStartAndEnd());
        UtilitySubsystem->RegisterTabAndGetID(WidgetBlueprint, RegistrationId);
        bOpened = UtilitySubsystem->SpawnRegisteredTabByID(RegistrationId);
      }

      if (!bOpened)
      {
        OutMessage = FString::Printf(TEXT("Failed to open editor utility widget: %s"), *AssetPath);
        OutErrorCode = TEXT("EXECUTION_FAILED");
        return false;
      }

      OutExecutedTabId = RegistrationId.ToString();
      OutMessage = FString::Printf(TEXT("Opened editor utility widget %s"), *AssetPath);
      return true;
    }

    if (bRequireWidget)
    {
      OutMessage = FString::Printf(TEXT("Asset is not an Editor Utility Widget: %s"), *AssetPath);
      OutErrorCode = TEXT("INVALID_ARGUMENT");
      return false;
    }

    if (!UtilitySubsystem->TryRun(Asset))
    {
      OutMessage = FString::Printf(TEXT("Failed to run editor utility asset: %s"), *AssetPath);
      OutErrorCode = TEXT("EXECUTION_FAILED");
      return false;
    }

    OutMessage = FString::Printf(TEXT("Ran editor utility asset %s"), *AssetPath);
    return true;
  }

  /** @brief Contract: executes a resolved registered command definition and returns the most specific error code available when execution fails. */
  bool ExecuteEditorCommandDefinition(const FMcpEditorCommandDefinition &Definition,
                                      FString &OutMessage,
                                      FString &OutExecutedTabId,
                                      FString &OutErrorCode)
  {
    switch (Definition.Kind)
    {
    case EMcpEditorCommandKind::OpenAsset:
      return OpenAssetEditorByPath(Definition.AssetPath, OutMessage,
                                   OutErrorCode);
    case EMcpEditorCommandKind::RunEditorUtility:
      return RunEditorUtilityAsset(Definition.AssetPath, Definition.TabId, false,
                                   OutMessage, OutExecutedTabId,
                                   OutErrorCode);
    case EMcpEditorCommandKind::OpenEditorUtilityWidget:
      return RunEditorUtilityAsset(Definition.AssetPath, Definition.TabId, true,
                                   OutMessage, OutExecutedTabId,
                                   OutErrorCode);
    case EMcpEditorCommandKind::ConsoleCommand:
    default:
    {
      if (!GEditor)
      {
        OutMessage = TEXT("Editor is not available");
        OutErrorCode = TEXT("EDITOR_NOT_AVAILABLE");
        return false;
      }

      UWorld *World = GEditor->GetEditorWorldContext().World();
      if (GEditor->Exec(World, *Definition.Command))
      {
        OutMessage = FString::Printf(TEXT("Executed command %s"), *Definition.Command);
        return true;
      }

      OutMessage = FString::Printf(TEXT("Failed to execute command %s"), *Definition.Command);
      OutErrorCode = TEXT("EXECUTION_FAILED");
      return false;
    }
    }
  }

  FSlateIcon MakeRegisteredIcon(const FString &IconName)
  {
    return IconName.IsEmpty()
               ? FSlateIcon()
               : FSlateIcon(FAppStyle::GetAppStyleSetName(), FName(*IconName));
  }

  FString ToCommandKindString(const EMcpEditorCommandKind Kind)
  {
    switch (Kind)
    {
    case EMcpEditorCommandKind::OpenAsset:
      return TEXT("open_asset");
    case EMcpEditorCommandKind::RunEditorUtility:
      return TEXT("run_editor_utility");
    case EMcpEditorCommandKind::OpenEditorUtilityWidget:
      return TEXT("open_editor_utility_widget");
    case EMcpEditorCommandKind::ConsoleCommand:
    default:
      return TEXT("console_command");
    }
  }

  TSharedPtr<FMcpEditorCommandDefinition> FindEditorCommandDefinition(
      const FName CommandName)
  {
    if (const TSharedPtr<FMcpEditorCommandDefinition> *Found =
            GMcpEditorCommands.Find(CommandName))
    {
      return *Found;
    }
    return nullptr;
  }

  TArray<FMcpDiscoveredUiTargetDefinition> DiscoverRegisteredCommandTargets()
  {
    TArray<FMcpDiscoveredUiTargetDefinition> Result;

    TArray<FName> CommandNames;
    GMcpEditorCommands.GetKeys(CommandNames);
    CommandNames.Sort(FNameLexicalLess());

    for (const FName &CommandName : CommandNames)
    {
      const TSharedPtr<FMcpEditorCommandDefinition> Definition =
          FindEditorCommandDefinition(CommandName);
      if (!Definition.IsValid())
      {
        continue;
      }

      FMcpDiscoveredUiTargetDefinition Target;
      Target.SourceType = TEXT("registered_command");
      Target.Identifier = CommandName.ToString();
      Target.DisplayName = Definition->Label.IsEmpty() ? Target.Identifier
                                                       : Definition->Label;
      Target.TabId = Definition->TabId;
      Target.AssetPath = Definition->AssetPath;
      Target.Tooltip = Definition->Tooltip;
      Target.Aliases = BuildUiTargetAliases(Target);
      Result.Add(Target);
    }

    return Result;
  }

  void AppendDiscoveredTargets(
      TArray<FMcpDiscoveredUiTargetDefinition> &Targets,
      const TArray<FMcpDiscoveredUiTargetDefinition> &AdditionalTargets)
  {
    for (const FMcpDiscoveredUiTargetDefinition &Target : AdditionalTargets)
    {
      const bool bAlreadyPresent = Targets.ContainsByPredicate(
          [&Target](const FMcpDiscoveredUiTargetDefinition &ExistingTarget)
          {
            return ExistingTarget.SourceType == Target.SourceType &&
                   ExistingTarget.Identifier.Equals(Target.Identifier,
                                                    ESearchCase::IgnoreCase);
          });
      if (!bAlreadyPresent)
      {
        Targets.Add(Target);
      }
    }
  }

  TArray<FString> ReadRequestedMenuNames(const TSharedPtr<FJsonObject> &Payload)
  {
    TArray<FString> RequestedMenuNames;
    if (!Payload.IsValid())
    {
      return RequestedMenuNames;
    }

    FString MenuName;
    if (Payload->TryGetStringField(TEXT("menuName"), MenuName))
    {
      AddUniqueTrimmedValue(RequestedMenuNames, MenuName);
    }

    const TArray<TSharedPtr<FJsonValue>> *MenuValues = nullptr;
    if (Payload->TryGetArrayField(TEXT("menuNames"), MenuValues))
    {
      for (const TSharedPtr<FJsonValue> &Value : *MenuValues)
      {
        FString RequestedMenuName;
        if (Value.IsValid() && Value->TryGetString(RequestedMenuName))
        {
          AddUniqueTrimmedValue(RequestedMenuNames, RequestedMenuName);
        }
      }
    }

    return RequestedMenuNames;
  }

  TArray<FMcpDiscoveredUiTargetDefinition> DiscoverUiTargets(
      const TSharedPtr<FJsonObject> &Payload, TArray<FString> &OutResolvedMenuNames,
      TArray<FString> &OutMissingMenuNames)
  {
    TArray<FMcpDiscoveredUiTargetDefinition> Targets = DiscoverJsonUiTargets();
    AppendDiscoveredTargets(Targets, DiscoverRegisteredCommandTargets());

    TArray<FString> RequestedMenuNames = ReadRequestedMenuNames(Payload);
    TArray<FString> AutoDiscoveredMenuNames;
    AppendDiscoveredTargets(Targets,
                            DiscoverMainMenuDropdownTargets(AutoDiscoveredMenuNames));
    for (const FString &MenuName : AutoDiscoveredMenuNames)
    {
      AddUniqueTrimmedValue(RequestedMenuNames, MenuName);
    }

    AppendDiscoveredTargets(Targets, DiscoverKnownToolMenuTargets(
                                         RequestedMenuNames,
                                         OutResolvedMenuNames,
                                         OutMissingMenuNames));
    return Targets;
  }

  TSharedPtr<FJsonObject> BuildUiTargetObject(
      const FMcpDiscoveredUiTargetDefinition &Target)
  {
    TSharedPtr<FJsonObject> TargetObject =
        McpHandlerUtils::CreateResultObject();
    TargetObject->SetStringField(TEXT("sourceType"), Target.SourceType);
    TargetObject->SetStringField(TEXT("displayName"), Target.DisplayName);
    TargetObject->SetStringField(TEXT("identifier"), Target.Identifier);
    if (!Target.TabId.IsEmpty())
    {
      TargetObject->SetStringField(TEXT("tabId"), Target.TabId);
    }
    if (!Target.AssetPath.IsEmpty())
    {
      TargetObject->SetStringField(TEXT("assetPath"), Target.AssetPath);
    }
    if (!Target.DefinitionPath.IsEmpty())
    {
      TargetObject->SetStringField(TEXT("definitionPath"), Target.DefinitionPath);
    }
    if (!Target.StatusFileName.IsEmpty())
    {
      TargetObject->SetStringField(TEXT("statusFile"), Target.StatusFileName);
    }
    if (!Target.StateFileName.IsEmpty())
    {
      TargetObject->SetStringField(TEXT("stateFile"), Target.StateFileName);
    }
    if (!Target.Tooltip.IsEmpty())
    {
      TargetObject->SetStringField(TEXT("tooltip"), Target.Tooltip);
    }
    if (!Target.MenuName.IsEmpty())
    {
      TargetObject->SetStringField(TEXT("menuName"), Target.MenuName);
    }
    if (!Target.SourceMenuName.IsEmpty() &&
        Target.SourceMenuName != Target.MenuName)
    {
      TargetObject->SetStringField(TEXT("sourceMenuName"),
                                   Target.SourceMenuName);
    }
    if (!Target.SectionName.IsEmpty())
    {
      TargetObject->SetStringField(TEXT("sectionName"), Target.SectionName);
    }
    if (!Target.EntryName.IsEmpty())
    {
      TargetObject->SetStringField(TEXT("entryName"), Target.EntryName);
    }
    if (!Target.EntryType.IsEmpty())
    {
      TargetObject->SetStringField(TEXT("entryType"), Target.EntryType);
    }
    if (Target.SectionIndex != INDEX_NONE)
    {
      TargetObject->SetNumberField(TEXT("sectionIndex"), Target.SectionIndex);
    }
    if (Target.EntryIndex != INDEX_NONE)
    {
      TargetObject->SetNumberField(TEXT("entryIndex"), Target.EntryIndex);
    }
    if (Target.SourceType == TEXT("tool_menu_entry"))
    {
      TargetObject->SetBoolField(TEXT("canOpenDirectly"),
                                 Target.bCanOpenDirectly);
    }

    TArray<TSharedPtr<FJsonValue>> AliasValues;
    for (const FString &Alias : Target.Aliases)
    {
      AliasValues.Add(MakeShared<FJsonValueString>(Alias));
    }
    TargetObject->SetArrayField(TEXT("aliases"), AliasValues);

    if (!Target.MenuHierarchy.IsEmpty())
    {
      TArray<TSharedPtr<FJsonValue>> HierarchyValues;
      for (const FString &HierarchyName : Target.MenuHierarchy)
      {
        HierarchyValues.Add(MakeShared<FJsonValueString>(HierarchyName));
      }
      TargetObject->SetArrayField(TEXT("menuHierarchy"), HierarchyValues);
    }

    if (!Target.TabId.IsEmpty())
    {
      TargetObject->SetBoolField(TEXT("isOpen"),
                                 FGlobalTabmanager::Get()
                                     ->FindExistingLiveTab(FName(*Target.TabId))
                                     .IsValid());
    }

    return TargetObject;
  }

  FUIAction MakeRegisteredToolAction(const FName CommandName)
  {
    return FUIAction(FExecuteAction::CreateLambda([CommandName]()
                                                  {
    const TSharedPtr<FMcpEditorCommandDefinition> Definition =
        FindEditorCommandDefinition(CommandName);
    if (!Definition.IsValid()) {
      UE_LOG(LogTemp, Warning,
             TEXT("McpAutomationBridge registered command missing: %s"),
             *CommandName.ToString());
      return;
    }

    FString ExecutionMessage;
    FString ExecutedTabId;
    FString ExecutionErrorCode;
    if (!ExecuteEditorCommandDefinition(*Definition.Get(), ExecutionMessage,
                      ExecutedTabId, ExecutionErrorCode)) {
      UE_LOG(LogTemp, Warning,
         TEXT("McpAutomationBridge command '%s' failed [%s]: %s"),
         *CommandName.ToString(), *ExecutionErrorCode, *ExecutionMessage);
    } }));
  }

  bool RegisterCommandMenuEntry(const FName MenuName, const FName SectionName,
                                const FName EntryName, const FName CommandName,
                                const bool bToolbar, FString &OutError)
  {
    const TSharedPtr<FMcpEditorCommandDefinition> Definition =
        FindEditorCommandDefinition(CommandName);
    if (!Definition.IsValid())
    {
      OutError = FString::Printf(TEXT("Unknown registered command: %s"),
                                 *CommandName.ToString());
      return false;
    }

    UToolMenus *ToolMenus = UToolMenus::TryGet();
    if (!ToolMenus)
    {
      OutError = TEXT("ToolMenus subsystem is not available");
      return false;
    }

    FToolMenuOwnerScoped OwnerScope{FToolMenuOwner(GMcpToolMenuOwnerName)};
    UToolMenu *Menu = ToolMenus->ExtendMenu(MenuName);
    if (!Menu)
    {
      OutError = FString::Printf(TEXT("Failed to extend menu: %s"),
                                 *MenuName.ToString());
      return false;
    }

    FToolMenuSection &Section = Menu->FindOrAddSection(SectionName);
    const FName OwnedEntryName = MakeOwnedToolMenuEntryName(EntryName);
    ToolMenus->RemoveEntry(MenuName, SectionName, OwnedEntryName);

    const FUIAction Action = MakeRegisteredToolAction(CommandName);
    FToolMenuEntry Entry = bToolbar
                               ? FToolMenuEntry::InitToolBarButton(
                                     OwnedEntryName, Action, FText::FromString(Definition->Label),
                                     FText::FromString(Definition->Tooltip),
                                     MakeRegisteredIcon(Definition->IconName))
                               : FToolMenuEntry::InitMenuEntry(
                                     OwnedEntryName, FText::FromString(Definition->Label),
                                     FText::FromString(Definition->Tooltip),
                                     MakeRegisteredIcon(Definition->IconName), Action);
    Entry.Owner = FToolMenuOwner(GMcpToolMenuOwnerName);
    Section.AddEntry(Entry);
    ToolMenus->RefreshMenuWidget(MenuName);
    return true;
  }

  TArray<TSharedRef<SWindow>> GetVisibleSlateWindows()
  {
    TArray<TSharedRef<SWindow>> Windows;
    if (FSlateApplication::IsInitialized())
    {
      FSlateApplication::Get().GetAllVisibleWindowsOrdered(Windows);
    }
    return Windows;
  }

  TArray<TSharedPtr<FJsonValue>> BuildVisibleWindowObjects()
  {
    TArray<TSharedPtr<FJsonValue>> WindowValues;
    const TSharedPtr<SWindow> ActiveWindow =
        FSlateApplication::IsInitialized()
            ? FSlateApplication::Get().GetActiveTopLevelRegularWindow()
            : nullptr;

    int32 WindowIndex = 0;
    for (const TSharedRef<SWindow> &Window : GetVisibleSlateWindows())
    {
      TSharedPtr<FJsonObject> WindowObject = McpHandlerUtils::CreateResultObject();
      const FString Title = Window->GetTitle().ToString();
      const FSlateRect WindowRect = Window->GetRectInScreen();
      const FSlateRect ClientRect = Window->GetClientRectInScreen();
      const FVector2D WindowSize = Window->GetSizeInScreen();
      const FVector2D ClientSize = Window->GetClientSizeInScreen();
      WindowObject->SetNumberField(TEXT("index"), WindowIndex++);
      WindowObject->SetStringField(TEXT("title"), Title);
      WindowObject->SetBoolField(TEXT("isActive"), ActiveWindow == Window);
      WindowObject->SetBoolField(TEXT("isVisible"), Window->IsVisible());
      WindowObject->SetNumberField(TEXT("x"), WindowRect.Left);
      WindowObject->SetNumberField(TEXT("y"), WindowRect.Top);
      WindowObject->SetNumberField(TEXT("width"), WindowSize.X);
      WindowObject->SetNumberField(TEXT("height"), WindowSize.Y);
      WindowObject->SetNumberField(TEXT("clientX"), ClientRect.Left);
      WindowObject->SetNumberField(TEXT("clientY"), ClientRect.Top);
      WindowObject->SetNumberField(TEXT("clientWidth"), ClientSize.X);
      WindowObject->SetNumberField(TEXT("clientHeight"), ClientSize.Y);
      WindowValues.Add(MakeShared<FJsonValueObject>(WindowObject));
    }

    return WindowValues;
  }

} // namespace
#endif

// =============================================================================
// Handler Implementation
// =============================================================================

/**
 * @brief Handles UI widget operations and system control actions.
 *
 * Processes both "system_control" and "manage_ui" actions with various subActions
 * for widget creation, manipulation, screenshots, and PIE control.
 *
 * @param RequestId Identifier for the incoming request.
 * @param Action Action name ("system_control" or "manage_ui").
 * @param Payload JSON object containing "subAction" and action-specific parameters.
 * @param RequestingSocket WebSocket for response delivery.
 * @return true if the action was handled, false otherwise.
 */
/** @brief Contract: handles manage_ui requests while preserving distinct not-found, editor-availability, slate-availability, and resolved-target execution failures. */
bool UMcpAutomationBridgeSubsystem::HandleUiAction(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
  const FString LowerAction = Action.ToLower();
  bool bIsSystemControl =
      LowerAction.Equals(TEXT("system_control"), ESearchCase::IgnoreCase);
  bool bIsManageUi =
      LowerAction.Equals(TEXT("manage_ui"), ESearchCase::IgnoreCase);

  if (!bIsSystemControl && !bIsManageUi)
  {
    return false;
  }

  if (!Payload.IsValid())
  {
    SendAutomationError(RequestingSocket, RequestId, TEXT("Payload missing."),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  // -------------------------------------------------------------------------
  // Extract SubAction
  // -------------------------------------------------------------------------
  FString SubAction;
  if (Payload->HasField(TEXT("subAction")))
  {
    SubAction = GetJsonStringField(Payload, TEXT("subAction"));
  }
  else
  {
    Payload->TryGetStringField(TEXT("action"), SubAction);
  }
  const FString LowerSub = SubAction.ToLower();

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetStringField(TEXT("action"), LowerSub);

  bool bSuccess = false;
  FString Message;
  FString ErrorCode;

#if WITH_EDITOR
  // ===========================================================================
  // SubAction: list_ui_targets
  // ===========================================================================
  if (LowerSub == TEXT("list_ui_targets"))
  {
    TArray<FString> ResolvedMenuNames;
    TArray<FString> MissingMenuNames;
    const TArray<FMcpDiscoveredUiTargetDefinition> Targets =
        DiscoverUiTargets(Payload, ResolvedMenuNames, MissingMenuNames);

    TArray<TSharedPtr<FJsonValue>> TargetValues;
    for (const FMcpDiscoveredUiTargetDefinition &Target : Targets)
    {
      const TSharedPtr<FJsonObject> TargetObject = BuildUiTargetObject(Target);
      TargetValues.Add(MakeShared<FJsonValueObject>(TargetObject));
    }

    TArray<TSharedPtr<FJsonValue>> JsonRootValues;
    for (const FString &JsonRoot : GetUiDiscoverySettings().JsonDefinitionRoots)
    {
      JsonRootValues.Add(MakeShared<FJsonValueString>(JsonRoot));
    }

    TArray<TSharedPtr<FJsonValue>> KnownMenuValues;
    for (const FString &MenuName : ResolvedMenuNames)
    {
      KnownMenuValues.Add(MakeShared<FJsonValueString>(MenuName));
    }

    TArray<TSharedPtr<FJsonValue>> MissingMenuValues;
    for (const FString &MenuName : MissingMenuNames)
    {
      MissingMenuValues.Add(MakeShared<FJsonValueString>(MenuName));
    }

    Resp->SetArrayField(TEXT("targets"), TargetValues);
    Resp->SetArrayField(TEXT("jsonDefinitionRoots"), JsonRootValues);
    Resp->SetArrayField(TEXT("knownMenuNames"), KnownMenuValues);
    Resp->SetArrayField(TEXT("missingMenuNames"), MissingMenuValues);
    Resp->SetNumberField(TEXT("count"), TargetValues.Num());
    bSuccess = true;
    Message = FString::Printf(TEXT("Listed %d UI targets"), TargetValues.Num());
  }
  // ===========================================================================
  // SubAction: list_visible_windows
  // ===========================================================================
  else if (LowerSub == TEXT("list_visible_windows"))
  {
    if (!FSlateApplication::IsInitialized())
    {
      Message = TEXT("Slate application is not initialized");
      ErrorCode = TEXT("SLATE_NOT_AVAILABLE");
      Resp->SetStringField(TEXT("error"), Message);
    }
    else
    {
      const TArray<TSharedPtr<FJsonValue>> WindowValues =
          BuildVisibleWindowObjects();
      Resp->SetArrayField(TEXT("windows"), WindowValues);
      Resp->SetNumberField(TEXT("count"), WindowValues.Num());
      bSuccess = true;
      Message = FString::Printf(TEXT("Listed %d visible Slate windows"),
                                WindowValues.Num());
    }
  }
  // ===========================================================================
  // SubAction: resolve_ui_target
  // ===========================================================================
  else if (LowerSub == TEXT("resolve_ui_target"))
  {
    FString RequestedIdentifier;
    FString RequestedTabId;
    FString RequestedWindowTitle;
    Payload->TryGetStringField(TEXT("identifier"), RequestedIdentifier);
    Payload->TryGetStringField(TEXT("tabId"), RequestedTabId);
    Payload->TryGetStringField(TEXT("windowTitle"), RequestedWindowTitle);

    RequestedIdentifier = RequestedIdentifier.TrimStartAndEnd();
    RequestedTabId = RequestedTabId.TrimStartAndEnd();
    RequestedWindowTitle = RequestedWindowTitle.TrimStartAndEnd();
    ApplyRequestedUiTargetFields(RequestedIdentifier, RequestedTabId,
                                 RequestedWindowTitle, Resp);

    if (RequestedIdentifier.IsEmpty() && RequestedTabId.IsEmpty() &&
        RequestedWindowTitle.IsEmpty())
    {
      Message = TEXT("identifier, tabId, or windowTitle is required");
      ErrorCode = TEXT("INVALID_ARGUMENT");
      Resp->SetStringField(TEXT("error"), Message);
    }
    else if (!FSlateApplication::IsInitialized())
    {
      Message = TEXT("Slate application is not initialized");
      ErrorCode = TEXT("SLATE_NOT_AVAILABLE");
      Resp->SetStringField(TEXT("error"), Message);
    }
    else
    {
      TArray<FString> ResolvedMenuNames;
      TArray<FString> MissingMenuNames;
      const TArray<FMcpDiscoveredUiTargetDefinition> Targets =
          DiscoverUiTargets(Payload, ResolvedMenuNames, MissingMenuNames);

      const TArray<const FMcpDiscoveredUiTargetDefinition *> IdentifierMatches =
          RequestedIdentifier.IsEmpty()
              ? TArray<const FMcpDiscoveredUiTargetDefinition *>()
              : FindDiscoveredUiTargetsByIdentifier(Targets, RequestedIdentifier);

      if (IdentifierMatches.Num() > 1)
      {
        Resp->SetStringField(TEXT("targetStatus"), TEXT("ambiguous"));
        Resp->SetStringField(TEXT("staleReason"), TEXT("ambiguous_identifier"));
        Resp->SetStringField(TEXT("recoveryHint"), TEXT("Provide an exact tabId or windowTitle, or narrow the identifier before retrying."));
        Message = FString::Printf(TEXT("UI target identifier '%s' matched multiple targets"),
                                  *RequestedIdentifier);
        bSuccess = true;
      }
      else
      {
        const FMcpDiscoveredUiTargetDefinition *ResolvedTarget =
            IdentifierMatches.Num() == 1 ? IdentifierMatches[0] : nullptr;
        const TSharedPtr<SDockTab> RequestedLiveTab =
            FindLiveDockTabById(RequestedTabId);
        TSharedPtr<SWindow> RequestedLiveWindow;

        if (!RequestedWindowTitle.IsEmpty())
        {
          for (const TSharedRef<SWindow> &Window : GetVisibleSlateWindows())
          {
            if (Window->GetTitle().ToString().Equals(RequestedWindowTitle,
                                                     ESearchCase::IgnoreCase))
            {
              RequestedLiveWindow = Window;
              break;
            }
          }
        }

        TSharedPtr<SDockTab> ResolvedLiveTab = RequestedLiveTab;
        if (!ResolvedLiveTab.IsValid() && ResolvedTarget != nullptr &&
            !ResolvedTarget->TabId.IsEmpty())
        {
          ResolvedLiveTab = FindLiveDockTabById(ResolvedTarget->TabId);
        }

        TSharedPtr<SWindow> ResolvedLiveWindow = RequestedLiveWindow;
        if (!ResolvedLiveWindow.IsValid() && ResolvedLiveTab.IsValid())
        {
          ResolvedLiveWindow = ResolvedLiveTab->GetParentWindow();
        }

        if (!ResolvedLiveTab.IsValid() && ResolvedTarget != nullptr &&
            !ResolvedTarget->TabId.IsEmpty())
        {
          const TSharedPtr<SDockTab> TargetLiveTab =
              FindLiveDockTabById(ResolvedTarget->TabId);
          if (TargetLiveTab.IsValid())
          {
            ResolvedLiveTab = TargetLiveTab;
            if (!ResolvedLiveWindow.IsValid())
            {
              ResolvedLiveWindow = TargetLiveTab->GetParentWindow();
            }
          }
        }

        if (!ResolvedLiveWindow.IsValid() && ResolvedTarget != nullptr &&
            !RequestedWindowTitle.IsEmpty() && ResolvedLiveTab.IsValid())
        {
          ResolvedLiveWindow = ResolvedLiveTab->GetParentWindow();
        }

        const bool bHadRequestedTab = !RequestedTabId.IsEmpty();
        const bool bHadRequestedWindow = !RequestedWindowTitle.IsEmpty();
        const bool bHadRequestedIdentifier = !RequestedIdentifier.IsEmpty();
        const bool bRecoveredViaIdentifier = ResolvedTarget != nullptr &&
                                             ((bHadRequestedTab && !RequestedLiveTab.IsValid() && !RequestedLiveWindow.IsValid()) ||
                                              (bHadRequestedWindow && !RequestedLiveWindow.IsValid()));

        FString ResolvedIdentifier =
            ResolvedTarget != nullptr ? ResolvedTarget->Identifier : RequestedIdentifier;
        FString ResolvedTabId = ResolvedLiveTab.IsValid()
                                    ? ResolvedLiveTab->GetLayoutIdentifier().ToString()
                                    : (ResolvedTarget != nullptr ? ResolvedTarget->TabId : RequestedTabId);
        FString ResolvedWindowTitle = ResolvedLiveWindow.IsValid()
                                          ? ResolvedLiveWindow->GetTitle().ToString()
                                          : RequestedWindowTitle;
        FString ResolvedTargetSource;
        if (RequestedLiveTab.IsValid())
        {
          ResolvedTargetSource = TEXT("tab_id");
        }
        else if (RequestedLiveWindow.IsValid())
        {
          ResolvedTargetSource = TEXT("window_title");
        }
        else if (ResolvedLiveTab.IsValid())
        {
          ResolvedTargetSource = RequestedTabId.IsEmpty() ? TEXT("tab_id_hint") : TEXT("tab_id");
        }
        else if (ResolvedTarget != nullptr)
        {
          ResolvedTargetSource = TEXT("tab_id_hint");
        }

        if (!ResolvedIdentifier.IsEmpty())
        {
          Resp->SetStringField(TEXT("resolvedIdentifier"), ResolvedIdentifier);
          Resp->SetStringField(TEXT("identifier"), ResolvedIdentifier);
        }
        if (!ResolvedTabId.IsEmpty())
        {
          Resp->SetStringField(TEXT("resolvedTabId"), ResolvedTabId);
          Resp->SetStringField(TEXT("tabId"), ResolvedTabId);
        }
        if (!ResolvedWindowTitle.IsEmpty())
        {
          Resp->SetStringField(TEXT("resolvedWindowTitle"), ResolvedWindowTitle);
          Resp->SetStringField(TEXT("windowTitle"), ResolvedWindowTitle);
        }
        if (ResolvedTarget != nullptr)
        {
          Resp->SetStringField(TEXT("targetType"), ResolvedTarget->SourceType);
        }
        if (!ResolvedTargetSource.IsEmpty())
        {
          Resp->SetStringField(TEXT("resolvedTargetSource"), ResolvedTargetSource);
        }

        bool bResolvedWithLiveSurface = false;
        if (ResolvedLiveTab.IsValid())
        {
          if (ResolvedLiveWindow.IsValid())
          {
            ApplyWindowBoundsToResponse(ResolvedLiveWindow, Resp);
            bResolvedWithLiveSurface = true;
          }
          else
          {
            Resp->SetStringField(TEXT("targetStatus"), TEXT("stale"));
            Resp->SetBoolField(TEXT("reResolved"), bRecoveredViaIdentifier);
            Resp->SetStringField(TEXT("staleReason"), TEXT("tab_without_parent_window"));
            Resp->SetStringField(TEXT("recoveryHint"), TEXT("Reopen the tab through manage_ui.open_ui_target, then resolve it again once the parent window is live."));
            Resp->SetStringField(TEXT("recoveryAction"), TEXT("open_ui_target"));
            Message = FString::Printf(TEXT("Resolved tab %s does not have a live parent window"),
                                      *ResolvedTabId);
            bSuccess = true;
          }
        }
        else if (ResolvedLiveWindow.IsValid())
        {
          ApplyWindowBoundsToResponse(ResolvedLiveWindow, Resp);
          bResolvedWithLiveSurface = true;
        }

        if (!bSuccess && bResolvedWithLiveSurface)
        {
          const bool bRecoveredToDifferentTabId = bHadRequestedTab &&
                                                  !RequestedLiveTab.IsValid() &&
                                                  !ResolvedTabId.IsEmpty() &&
                                                  !ResolvedTabId.Equals(RequestedTabId,
                                                                        ESearchCase::CaseSensitive);
          const bool bWindowHintDrifted = bHadRequestedWindow &&
                                          !RequestedLiveWindow.IsValid() &&
                                          ResolvedLiveWindow.IsValid();
          const bool bTabHintDrifted = bHadRequestedTab &&
                                       !RequestedLiveTab.IsValid() &&
                                       (ResolvedLiveTab.IsValid() ||
                                        bRecoveredToDifferentTabId);
          const bool bNeedsReresolve = bRecoveredViaIdentifier || bWindowHintDrifted || bTabHintDrifted;
          Resp->SetStringField(TEXT("targetStatus"), bNeedsReresolve ? TEXT("stale") : TEXT("resolved"));
          Resp->SetBoolField(TEXT("reResolved"), bNeedsReresolve);
          if (bWindowHintDrifted)
          {
            Resp->SetStringField(TEXT("staleReason"), TEXT("missing_visible_window"));
          }
          else if (bTabHintDrifted)
          {
            Resp->SetStringField(TEXT("staleReason"), TEXT("missing_live_tab"));
          }
          Message = bNeedsReresolve
                        ? FString::Printf(TEXT("Re-resolved UI target %s to a live surface"),
                                          *ResolvedIdentifier)
                        : FString::Printf(TEXT("Resolved UI target %s"),
                                          *ResolvedIdentifier);
          bSuccess = true;
        }

        if (!bSuccess && ResolvedTarget != nullptr &&
            IsDiscoveredUiTargetOpenable(*ResolvedTarget))
        {
          Resp->SetStringField(TEXT("targetStatus"), TEXT("needs_open"));
          Resp->SetBoolField(TEXT("reResolved"), bRecoveredViaIdentifier);
          if ((bHadRequestedTab || !ResolvedTarget->TabId.IsEmpty()) && !ResolvedLiveTab.IsValid())
          {
            Resp->SetStringField(TEXT("staleReason"), TEXT("missing_live_tab"));
          }
          else if (bHadRequestedWindow && !RequestedLiveWindow.IsValid())
          {
            Resp->SetStringField(TEXT("staleReason"), TEXT("missing_visible_window"));
          }
          Resp->SetStringField(TEXT("recoveryHint"), TEXT("Open the resolved target through manage_ui.open_ui_target and retry resolution to capture live bounds."));
          Resp->SetStringField(TEXT("recoveryAction"), TEXT("open_ui_target"));
          Message = FString::Printf(TEXT("Resolved UI target %s, but the live surface is not open"),
                                    *ResolvedIdentifier);
          bSuccess = true;
        }

        if (!bSuccess && bHadRequestedWindow && !RequestedLiveWindow.IsValid())
        {
          Resp->SetStringField(TEXT("targetStatus"), TEXT("not_found"));
          Resp->SetStringField(TEXT("staleReason"), TEXT("missing_visible_window"));
          Resp->SetStringField(TEXT("recoveryHint"), TEXT("Call manage_ui.list_visible_windows or manage_ui.resolve_ui_target with a stronger identifier before retrying."));
          Message = FString::Printf(TEXT("Visible window '%s' was not found"),
                                    *RequestedWindowTitle);
          bSuccess = true;
        }

        if (!bSuccess && bHadRequestedTab && !RequestedLiveTab.IsValid())
        {
          Resp->SetStringField(TEXT("targetStatus"), TEXT("not_found"));
          Resp->SetStringField(TEXT("staleReason"), TEXT("missing_live_tab"));
          Resp->SetStringField(TEXT("recoveryHint"), TEXT("Retry with manage_ui.resolve_ui_target using an identifier, or open the target explicitly if it is known."));
          Message = FString::Printf(TEXT("Live tab %s was not found"), *RequestedTabId);
          bSuccess = true;
        }

        if (!bSuccess)
        {
          Resp->SetStringField(TEXT("targetStatus"), TEXT("not_found"));
          if (bHadRequestedIdentifier)
          {
            Message = FString::Printf(TEXT("UI target not found: %s"),
                                      *RequestedIdentifier);
          }
          else
          {
            Message = TEXT("UI target not found");
          }
          bSuccess = true;
        }
      }
    }
  }
  // ===========================================================================
  // SubAction: open_ui_target
  // ===========================================================================
  else if (LowerSub == TEXT("open_ui_target"))
  {
    FString ExactTabIdString;
    FString Identifier;
    bool bResolvedTarget = false;
    Payload->TryGetStringField(TEXT("tabId"), ExactTabIdString);
    Payload->TryGetStringField(TEXT("identifier"), Identifier);

    if (ExactTabIdString.TrimStartAndEnd().IsEmpty() &&
        Identifier.TrimStartAndEnd().IsEmpty())
    {
      Message = TEXT("tabId or identifier is required");
      ErrorCode = TEXT("INVALID_ARGUMENT");
      Resp->SetStringField(TEXT("error"), Message);
    }
    else
    {
      FString OpenedTabId;
      FString OpenedTarget;
      FString OpenedTargetType;

      if (!ExactTabIdString.TrimStartAndEnd().IsEmpty())
      {
        const FName ExactTabId(*ExactTabIdString.TrimStartAndEnd());
        if (!FSlateApplication::IsInitialized())
        {
          Message = TEXT("Slate application is not initialized");
          ErrorCode = TEXT("SLATE_NOT_AVAILABLE");
        }
        else
        {
          const bool bKnownTabId =
              FGlobalTabmanager::Get()->HasTabSpawner(ExactTabId) ||
              FGlobalTabmanager::Get()->FindExistingLiveTab(ExactTabId).IsValid();

          if (bKnownTabId)
          {
            bResolvedTarget = true;
            const TSharedPtr<SDockTab> OpenedTab =
                FGlobalTabmanager::Get()->TryInvokeTab(ExactTabId);
            if (OpenedTab.IsValid())
            {
              bSuccess = true;
              OpenedTabId = ExactTabId.ToString();
              OpenedTarget = ExactTabIdString.TrimStartAndEnd();
              OpenedTargetType = TEXT("slate_tab");
              Message = FString::Printf(TEXT("Opened tab %s"), *OpenedTabId);
            }
            else
            {
              Message = FString::Printf(TEXT("Failed to open tab %s"),
                                        *ExactTabIdString.TrimStartAndEnd());
              ErrorCode = TEXT("EXECUTION_FAILED");
            }
          }
          else
          {
            Message = FString::Printf(TEXT("UI tab not found: %s"),
                                      *ExactTabIdString.TrimStartAndEnd());
            ErrorCode = TEXT("NOT_FOUND");
          }
        }
      }

      if (!bSuccess && !Identifier.TrimStartAndEnd().IsEmpty())
      {
        TArray<FString> ResolvedMenuNames;
        TArray<FString> MissingMenuNames;
        const TArray<FMcpDiscoveredUiTargetDefinition> Targets =
            DiscoverUiTargets(Payload, ResolvedMenuNames, MissingMenuNames);
        if (const FMcpDiscoveredUiTargetDefinition *Target =
                FindDiscoveredUiTargetByIdentifier(Targets, Identifier))
        {
          bResolvedTarget = true;
          if (Target->SourceType == TEXT("registered_command"))
          {
            const TSharedPtr<FMcpEditorCommandDefinition> Definition =
                FindEditorCommandDefinition(FName(*Target->Identifier));
            FString ExecutedTabId;
            if (Definition.IsValid() &&
                ExecuteEditorCommandDefinition(*Definition.Get(), Message,
                                               ExecutedTabId, ErrorCode))
            {
              bSuccess = true;
              OpenedTabId = ExecutedTabId;
              OpenedTarget = Target->Identifier;
              OpenedTargetType = Target->SourceType;
            }
            else if (!Definition.IsValid())
            {
              Message = FString::Printf(TEXT("Registered command definition not found for %s"),
                                        *Target->Identifier);
              ErrorCode = TEXT("EXECUTION_FAILED");
            }
            else if (ErrorCode.IsEmpty())
            {
              ErrorCode = TEXT("EXECUTION_FAILED");
            }
          }
          else if (Target->SourceType == TEXT("tool_menu_entry"))
          {
            if (ExecuteToolMenuEntryTarget(*Target, Message, ErrorCode))
            {
              bSuccess = true;
              OpenedTarget = Target->Identifier;
              OpenedTargetType = Target->SourceType;
            }
            else if (ErrorCode.IsEmpty())
            {
              ErrorCode = TEXT("EXECUTION_FAILED");
            }
          }
          else if (!Target->TabId.IsEmpty())
          {
            const TSharedPtr<SDockTab> OpenedTab =
                FGlobalTabmanager::Get()->TryInvokeTab(FName(*Target->TabId));
            if (OpenedTab.IsValid())
            {
              bSuccess = true;
              OpenedTabId = Target->TabId;
              OpenedTarget = Target->Identifier;
              OpenedTargetType = Target->SourceType;
              Message = FString::Printf(TEXT("Opened UI target %s"),
                                        *Target->DisplayName);
            }
            else
            {
              Message = FString::Printf(TEXT("Failed to open UI target %s"),
                                        *Target->Identifier);
              ErrorCode = TEXT("EXECUTION_FAILED");
            }
          }
          else
          {
            Message = FString::Printf(TEXT("UI target %s cannot be opened directly"),
                                      *Target->Identifier);
            ErrorCode = TEXT("INVALID_ARGUMENT");
          }
        }
        else
        {
          Message = FString::Printf(TEXT("UI target not found: %s"),
                                    *Identifier.TrimStartAndEnd());
          ErrorCode = TEXT("NOT_FOUND");
        }
      }

      if (bSuccess)
      {
        Resp->SetStringField(TEXT("target"), OpenedTarget);
        Resp->SetStringField(TEXT("targetType"), OpenedTargetType);
        if (!OpenedTabId.IsEmpty())
        {
          Resp->SetStringField(TEXT("tabId"), OpenedTabId);
        }
      }
      else if (bResolvedTarget)
      {
        if (ErrorCode.IsEmpty())
        {
          ErrorCode = TEXT("EXECUTION_FAILED");
        }
        if (Message.IsEmpty())
        {
          Message = TEXT("Resolved UI target could not be opened");
        }
        Resp->SetStringField(TEXT("error"), Message);
      }
      else if (ErrorCode.IsEmpty())
      {
        Message = TEXT("UI target not found or could not be opened");
        ErrorCode = TEXT("NOT_FOUND");
        Resp->SetStringField(TEXT("error"), Message);
      }
    }
  }
  // ===========================================================================
  // SubAction: close_tab
  // ===========================================================================
  else if (LowerSub == TEXT("close_tab"))
  {
    FString ExactTabIdString;
    FString Identifier;
    FString WidgetPath;
    Payload->TryGetStringField(TEXT("tabId"), ExactTabIdString);
    Payload->TryGetStringField(TEXT("identifier"), Identifier);
    Payload->TryGetStringField(TEXT("widgetPath"), WidgetPath);
    if (WidgetPath.IsEmpty())
    {
      Payload->TryGetStringField(TEXT("utilityPath"), WidgetPath);
    }
    if (WidgetPath.IsEmpty())
    {
      Payload->TryGetStringField(TEXT("editorUtilityPath"), WidgetPath);
    }

    if (ExactTabIdString.TrimStartAndEnd().IsEmpty() &&
        Identifier.TrimStartAndEnd().IsEmpty() &&
        WidgetPath.TrimStartAndEnd().IsEmpty())
    {
      Message = TEXT("tabId, identifier, or widgetPath is required");
      ErrorCode = TEXT("INVALID_ARGUMENT");
      Resp->SetStringField(TEXT("error"), Message);
    }
    else
    {
      bool bClosed = false;
      FString ClosedTabId;
      FString ClosedTarget;
      FString ClosedType;
      UEditorUtilitySubsystem *UtilitySubsystem = nullptr;
      if (!GEditor)
      {
        Message = TEXT("Editor is not available");
        ErrorCode = TEXT("EDITOR_NOT_AVAILABLE");
        Resp->SetStringField(TEXT("error"), Message);
      }
      else
      {
        UtilitySubsystem = GEditor->GetEditorSubsystem<UEditorUtilitySubsystem>();
      }

      if (ErrorCode.IsEmpty() && !WidgetPath.TrimStartAndEnd().IsEmpty())
      {
        UObject *Asset = UEditorAssetLibrary::LoadAsset(WidgetPath);
        UEditorUtilityWidgetBlueprint *WidgetBlueprint =
            Cast<UEditorUtilityWidgetBlueprint>(Asset);
        if (!WidgetBlueprint)
        {
          Message = FString::Printf(
              TEXT("Editor Utility Widget asset not found: %s"), *WidgetPath);
          ErrorCode = TEXT("NOT_FOUND");
          Resp->SetStringField(TEXT("error"), Message);
        }
        else if (!UtilitySubsystem)
        {
          Message = TEXT("EditorUtilitySubsystem not available");
          ErrorCode = TEXT("NOT_AVAILABLE");
          Resp->SetStringField(TEXT("error"), Message);
        }
        else
        {
          const FName RegistrationId =
              MakeEditorUtilityRegistrationId(WidgetBlueprint, ExactTabIdString);
          bClosed = UtilitySubsystem->CloseTabByID(RegistrationId);
          ClosedTabId = RegistrationId.ToString();
          ClosedTarget = WidgetPath;
          ClosedType = TEXT("editor_utility_widget");
        }
      }

      if (ErrorCode.IsEmpty() && !bClosed && !ExactTabIdString.TrimStartAndEnd().IsEmpty())
      {
        const FName ExactTabId(*ExactTabIdString.TrimStartAndEnd());
        bClosed = UtilitySubsystem && UtilitySubsystem->CloseTabByID(ExactTabId);
        if (!bClosed)
        {
          bClosed = CloseSlateTabById(ExactTabId);
          if (bClosed)
          {
            ClosedType = TEXT("slate_tab");
          }
        }
        else
        {
          ClosedType = TEXT("editor_utility_widget");
        }

        if (bClosed)
        {
          ClosedTabId = ExactTabId.ToString();
          ClosedTarget = ExactTabIdString.TrimStartAndEnd();
        }
      }

      if (ErrorCode.IsEmpty() && !bClosed && !Identifier.TrimStartAndEnd().IsEmpty())
      {
        TArray<FString> ResolvedMenuNames;
        TArray<FString> MissingMenuNames;
        const TArray<FMcpDiscoveredUiTargetDefinition> Targets =
            DiscoverUiTargets(Payload, ResolvedMenuNames, MissingMenuNames);
        if (const FMcpDiscoveredUiTargetDefinition *Target =
                FindDiscoveredUiTargetByIdentifier(Targets, Identifier))
        {
          bClosed = CloseSlateTabById(FName(*Target->TabId));
          if (bClosed)
          {
            ClosedTabId = Target->TabId;
            ClosedTarget = Identifier.TrimStartAndEnd();
            ClosedType = Target->SourceType;
          }
        }
      }

      if (bClosed)
      {
        bSuccess = true;
        Message = FString::Printf(TEXT("Closed tab %s"), *ClosedTabId);
        Resp->SetStringField(TEXT("tabId"), ClosedTabId);
        Resp->SetStringField(TEXT("target"), ClosedTarget);
        Resp->SetStringField(TEXT("targetType"), ClosedType);
      }
      else if (ErrorCode.IsEmpty())
      {
        Message = TEXT("Tab not found or not currently open");
        ErrorCode = TEXT("NOT_FOUND");
        Resp->SetStringField(TEXT("error"), Message);
      }
    }
  }
  // ===========================================================================
  // SubAction: list_editor_commands
  // ===========================================================================
  else if (LowerSub == TEXT("list_editor_commands"))
  {
    TArray<FName> CommandNames;
    GMcpEditorCommands.GetKeys(CommandNames);
    CommandNames.Sort(FNameLexicalLess());

    TArray<TSharedPtr<FJsonValue>> CommandValues;
    for (const FName &CommandName : CommandNames)
    {
      const TSharedPtr<FMcpEditorCommandDefinition> Definition =
          FindEditorCommandDefinition(CommandName);
      if (!Definition.IsValid())
      {
        continue;
      }

      TSharedPtr<FJsonObject> CommandObject =
          McpHandlerUtils::CreateResultObject();
      CommandObject->SetStringField(TEXT("name"), CommandName.ToString());
      CommandObject->SetStringField(TEXT("label"), Definition->Label);
      CommandObject->SetStringField(TEXT("tooltip"), Definition->Tooltip);
      CommandObject->SetStringField(TEXT("icon"), Definition->IconName);
      CommandObject->SetStringField(TEXT("commandType"),
                                    ToCommandKindString(Definition->Kind));
      if (!Definition->Command.IsEmpty())
      {
        CommandObject->SetStringField(TEXT("command"), Definition->Command);
      }
      if (!Definition->AssetPath.IsEmpty())
      {
        CommandObject->SetStringField(TEXT("assetPath"),
                                      Definition->AssetPath);
      }
      if (!Definition->TabId.IsEmpty())
      {
        CommandObject->SetStringField(TEXT("tabId"), Definition->TabId);
      }
      CommandValues.Add(MakeShared<FJsonValueObject>(CommandObject));
    }

    Resp->SetArrayField(TEXT("commands"), CommandValues);
    Resp->SetStringField(TEXT("scope"), TEXT("session"));
    Resp->SetNumberField(TEXT("count"), CommandValues.Num());
    bSuccess = true;
    Message = FString::Printf(TEXT("Listed %d registered editor commands"),
                              CommandValues.Num());
  }

  // ===========================================================================
  // SubAction: create_editor_utility_widget
  // ===========================================================================
  else if (LowerSub == TEXT("create_editor_utility_widget"))
  {
    FString WidgetName;
    if (!Payload->TryGetStringField(TEXT("name"), WidgetName) ||
        WidgetName.IsEmpty())
    {
      Message = TEXT("name field required for create_editor_utility_widget");
      ErrorCode = TEXT("INVALID_ARGUMENT");
      Resp->SetStringField(TEXT("error"), Message);
    }
    else
    {
      FString SavePath;
      Payload->TryGetStringField(TEXT("savePath"), SavePath);
      if (SavePath.IsEmpty())
      {
        SavePath = TEXT("/Game/EditorUtilities");
      }

      const FString NormalizedPath = SavePath.TrimStartAndEnd();
      const FString TargetPath =
          FString::Printf(TEXT("%s/%s"), *NormalizedPath, *WidgetName);

      if (UEditorAssetLibrary::DoesAssetExist(TargetPath))
      {
        UObject *ExistingAsset = UEditorAssetLibrary::LoadAsset(TargetPath);
        UEditorUtilityWidgetBlueprint *ExistingWidget =
            Cast<UEditorUtilityWidgetBlueprint>(ExistingAsset);

        if (!ExistingAsset)
        {
          Message = FString::Printf(TEXT("Existing asset could not be loaded: %s"),
                                    *TargetPath);
          ErrorCode = TEXT("ASSET_LOAD_FAILED");
          Resp->SetStringField(TEXT("error"), Message);
        }
        else if (!ExistingWidget)
        {
          Message = FString::Printf(
              TEXT("Existing asset at %s is not an EditorUtilityWidgetBlueprint"),
              *TargetPath);
          ErrorCode = TEXT("TYPE_MISMATCH");
          Resp->SetStringField(TEXT("error"), Message);
        }
        else
        {
          bSuccess = true;
          Message = FString::Printf(
              TEXT("Editor utility widget already exists at %s"), *TargetPath);
          Resp->SetStringField(TEXT("widgetPath"), TargetPath);
          Resp->SetBoolField(TEXT("exists"), true);
          Resp->SetStringField(TEXT("widgetName"), WidgetName);
        }
      }
      else
      {
        UEditorUtilityWidgetBlueprintFactory *Factory =
            NewObject<UEditorUtilityWidgetBlueprintFactory>();
        if (!Factory)
        {
          Message = TEXT("Failed to create editor utility widget factory");
          ErrorCode = TEXT("FACTORY_CREATION_FAILED");
          Resp->SetStringField(TEXT("error"), Message);
        }
        else
        {
          Factory->BlueprintType = BPTYPE_Normal;
          Factory->ParentClass = UEditorUtilityWidget::StaticClass();

          FString ParentClassPath;
          if (Payload->TryGetStringField(TEXT("parentClass"), ParentClassPath) &&
              !ParentClassPath.IsEmpty())
          {
            UClass *ParentClass =
                LoadClass<UUserWidget>(nullptr, *ParentClassPath);
            if (!ParentClass)
            {
              ParentClass = FindObject<UClass>(nullptr, *ParentClassPath);
            }

            if (!ParentClass ||
                !ParentClass->IsChildOf(UEditorUtilityWidget::StaticClass()))
            {
              Message = FString::Printf(
                  TEXT("parentClass must derive from EditorUtilityWidget: %s"),
                  *ParentClassPath);
              ErrorCode = TEXT("INVALID_ARGUMENT");
              Resp->SetStringField(TEXT("error"), Message);
            }
            else
            {
              Factory->ParentClass = ParentClass;
            }
          }

          if (ErrorCode.IsEmpty())
          {
            IAssetTools &AssetTools = FAssetToolsModule::GetModule().Get();
            UObject *NewAsset = AssetTools.CreateAsset(
                WidgetName, NormalizedPath,
                UEditorUtilityWidgetBlueprint::StaticClass(), Factory);
            UEditorUtilityWidgetBlueprint *WidgetBlueprint =
                Cast<UEditorUtilityWidgetBlueprint>(NewAsset);

            if (!WidgetBlueprint)
            {
              Message = TEXT("Failed to create editor utility widget asset");
              ErrorCode = TEXT("ASSET_CREATION_FAILED");
              Resp->SetStringField(TEXT("error"), Message);
            }
            else
            {
              SaveLoadedAssetThrottled(WidgetBlueprint, -1.0, true);
              ScanPathSynchronous(WidgetBlueprint->GetOutermost()->GetName());

              bSuccess = true;
              Message = FString::Printf(
                  TEXT("Editor utility widget created at %s"),
                  *WidgetBlueprint->GetPathName());
              Resp->SetStringField(TEXT("widgetPath"),
                                   WidgetBlueprint->GetPathName());
              Resp->SetStringField(TEXT("widgetName"), WidgetName);
              Resp->SetBoolField(TEXT("exists"), false);
            }
          }
        }
      }
    }
  }
  // ===========================================================================
  // SubAction: run_editor_utility
  // ===========================================================================
  else if (LowerSub == TEXT("run_editor_utility"))
  {
    FString AssetPath;
    Payload->TryGetStringField(TEXT("utilityPath"), AssetPath);
    if (AssetPath.IsEmpty())
    {
      Payload->TryGetStringField(TEXT("editorUtilityPath"), AssetPath);
    }
    if (AssetPath.IsEmpty())
    {
      Payload->TryGetStringField(TEXT("widgetPath"), AssetPath);
    }

    if (AssetPath.IsEmpty())
    {
      Message = TEXT("utilityPath, editorUtilityPath, or widgetPath is required");
      ErrorCode = TEXT("INVALID_ARGUMENT");
      Resp->SetStringField(TEXT("error"), Message);
    }
    else
    {
      FString TabId;
      Payload->TryGetStringField(TEXT("tabId"), TabId);

      FString ExecutedTabId;
      bSuccess = RunEditorUtilityAsset(AssetPath, TabId, false, Message,
                                       ExecutedTabId, ErrorCode);
      if (bSuccess)
      {
        Resp->SetStringField(TEXT("assetPath"), AssetPath);
        if (!ExecutedTabId.IsEmpty())
        {
          Resp->SetStringField(TEXT("tabId"), ExecutedTabId);
        }
      }
      else
      {
        Resp->SetStringField(TEXT("error"), Message);
      }
    }
  }
  // ===========================================================================
  // SubAction: register_editor_command
  // ===========================================================================
  else if (LowerSub == TEXT("register_editor_command"))
  {
    FString CommandNameString;
    if (!Payload->TryGetStringField(TEXT("name"), CommandNameString) ||
        CommandNameString.IsEmpty())
    {
      Message = TEXT("name is required for register_editor_command");
      ErrorCode = TEXT("INVALID_ARGUMENT");
      Resp->SetStringField(TEXT("error"), Message);
    }
    else
    {
      const FName CommandName(*CommandNameString);
      FString DefinitionError;
      TSharedPtr<FMcpEditorCommandDefinition> Definition =
          BuildEditorCommandDefinition(Payload, CommandName, DefinitionError);
      if (!Definition.IsValid())
      {
        Message = DefinitionError;
        ErrorCode = TEXT("INVALID_ARGUMENT");
        Resp->SetStringField(TEXT("error"), Message);
      }
      else
      {
        GMcpEditorCommands.Add(CommandName, Definition);
        bSuccess = true;
        Message = FString::Printf(TEXT("Registered editor command %s"),
                                  *CommandName.ToString());
        Resp->SetStringField(TEXT("name"), CommandName.ToString());
        Resp->SetStringField(TEXT("label"), Definition->Label);
        Resp->SetStringField(TEXT("commandType"),
                             ToCommandKindString(Definition->Kind));
        if (!Definition->Command.IsEmpty())
        {
          Resp->SetStringField(TEXT("command"), Definition->Command);
        }
        if (!Definition->AssetPath.IsEmpty())
        {
          Resp->SetStringField(TEXT("assetPath"), Definition->AssetPath);
        }
        if (!Definition->TabId.IsEmpty())
        {
          Resp->SetStringField(TEXT("tabId"), Definition->TabId);
        }
      }
    }
  }
  // ===========================================================================
  // SubAction: add_menu_entry / add_toolbar_button
  // ===========================================================================
  else if (LowerSub == TEXT("add_menu_entry") ||
           LowerSub == TEXT("add_toolbar_button"))
  {
    FString MenuNameString;
    FString CommandNameString;
    if (!Payload->TryGetStringField(TEXT("menuName"), MenuNameString) ||
        MenuNameString.IsEmpty())
    {
      Message = TEXT("menuName is required");
      ErrorCode = TEXT("INVALID_ARGUMENT");
      Resp->SetStringField(TEXT("error"), Message);
    }
    else if (!Payload->TryGetStringField(TEXT("commandName"),
                                         CommandNameString) ||
             CommandNameString.IsEmpty())
    {
      Message = TEXT("commandName is required");
      ErrorCode = TEXT("INVALID_ARGUMENT");
      Resp->SetStringField(TEXT("error"), Message);
    }
    else
    {
      FString SectionNameString;
      Payload->TryGetStringField(TEXT("section"), SectionNameString);
      if (SectionNameString.IsEmpty())
      {
        SectionNameString = TEXT("McpAutomationBridge");
      }

      FString EntryNameString;
      Payload->TryGetStringField(TEXT("entryName"), EntryNameString);
      if (EntryNameString.IsEmpty())
      {
        EntryNameString = CommandNameString;
      }

      const bool bToolbarButton = LowerSub == TEXT("add_toolbar_button");
      const FString RegisteredEntryName =
          MakeOwnedToolMenuEntryName(FName(*EntryNameString)).ToString();
      FString RegistrationError;
      bSuccess = RegisterCommandMenuEntry(
          FName(*MenuNameString), FName(*SectionNameString),
          FName(*EntryNameString), FName(*CommandNameString), bToolbarButton,
          RegistrationError);

      if (bSuccess)
      {
        Message = FString::Printf(
            TEXT("Added %s '%s' to %s"),
            bToolbarButton ? TEXT("toolbar button") : TEXT("menu entry"),
            *RegisteredEntryName, *MenuNameString);
        Resp->SetStringField(TEXT("menuName"), MenuNameString);
        Resp->SetStringField(TEXT("section"), SectionNameString);
        Resp->SetStringField(TEXT("entryName"), RegisteredEntryName);
        Resp->SetStringField(TEXT("requestedEntryName"), EntryNameString);
        Resp->SetStringField(TEXT("commandName"), CommandNameString);
      }
      else
      {
        Message = RegistrationError;
        ErrorCode = TEXT("REGISTRATION_FAILED");
        Resp->SetStringField(TEXT("error"), Message);
      }
    }
  }
  // ===========================================================================
  // SubAction: create_widget
  // ===========================================================================
  else if (LowerSub == TEXT("create_widget"))
  {
#if WITH_EDITOR && MCP_HAS_WIDGET_FACTORY
    FString WidgetName;
    if (!Payload->TryGetStringField(TEXT("name"), WidgetName) ||
        WidgetName.IsEmpty())
    {
      Message = TEXT("name field required for create_widget");
      ErrorCode = TEXT("INVALID_ARGUMENT");
      Resp->SetStringField(TEXT("error"), Message);
    }
    else
    {
      FString SavePath;
      Payload->TryGetStringField(TEXT("savePath"), SavePath);
      if (SavePath.IsEmpty())
      {
        SavePath = TEXT("/Game/UI/Widgets");
      }

      FString WidgetType;
      Payload->TryGetStringField(TEXT("widgetType"), WidgetType);

      const FString NormalizedPath = SavePath.TrimStartAndEnd();
      const FString TargetPath =
          FString::Printf(TEXT("%s/%s"), *NormalizedPath, *WidgetName);
      if (UEditorAssetLibrary::DoesAssetExist(TargetPath))
      {
        UObject *ExistingAsset = UEditorAssetLibrary::LoadAsset(TargetPath);
        UWidgetBlueprint *ExistingWidget = Cast<UWidgetBlueprint>(ExistingAsset);

        if (!ExistingAsset)
        {
          Message = FString::Printf(TEXT("Existing asset could not be loaded: %s"),
                                    *TargetPath);
          ErrorCode = TEXT("ASSET_LOAD_FAILED");
          Resp->SetStringField(TEXT("error"), Message);
        }
        else if (!ExistingWidget)
        {
          Message = FString::Printf(
              TEXT("Existing asset at %s is not a WidgetBlueprint"),
              *TargetPath);
          ErrorCode = TEXT("TYPE_MISMATCH");
          Resp->SetStringField(TEXT("error"), Message);
        }
        else
        {
          bSuccess = true;
          Message = FString::Printf(TEXT("Widget blueprint already exists at %s"),
                                    *TargetPath);
          Resp->SetStringField(TEXT("widgetPath"), TargetPath);
          Resp->SetBoolField(TEXT("exists"), true);
          if (!WidgetType.IsEmpty())
          {
            Resp->SetStringField(TEXT("widgetType"), WidgetType);
          }
          Resp->SetStringField(TEXT("widgetName"), WidgetName);
        }
      }
      else
      {
        UWidgetBlueprintFactory *Factory = NewObject<UWidgetBlueprintFactory>();
        if (!Factory)
        {
          Message = TEXT("Failed to create widget blueprint factory");
          ErrorCode = TEXT("FACTORY_CREATION_FAILED");
          Resp->SetStringField(TEXT("error"), Message);
        }
        else
        {
          UObject *NewAsset = Factory->FactoryCreateNew(
              UWidgetBlueprint::StaticClass(),
              UEditorAssetLibrary::DoesAssetExist(NormalizedPath)
                  ? UEditorAssetLibrary::LoadAsset(NormalizedPath)
                  : nullptr,
              FName(*WidgetName), RF_Standalone, nullptr, GWarn);

          UWidgetBlueprint *WidgetBlueprint = Cast<UWidgetBlueprint>(NewAsset);

          if (!WidgetBlueprint)
          {
            Message = TEXT("Failed to create widget blueprint asset");
            ErrorCode = TEXT("ASSET_CREATION_FAILED");
            Resp->SetStringField(TEXT("error"), Message);
          }
          else
          {
            // Force immediate save and registry scan
            SaveLoadedAssetThrottled(WidgetBlueprint, -1.0, true);
            ScanPathSynchronous(WidgetBlueprint->GetOutermost()->GetName());

            bSuccess = true;
            Message = FString::Printf(TEXT("Widget blueprint created at %s"),
                                      *WidgetBlueprint->GetPathName());
            Resp->SetStringField(TEXT("widgetPath"),
                                 WidgetBlueprint->GetPathName());
            Resp->SetStringField(TEXT("widgetName"), WidgetName);
            if (!WidgetType.IsEmpty())
            {
              Resp->SetStringField(TEXT("widgetType"), WidgetType);
            }
          }
        }
      }
    }
#else
    Message =
        TEXT("create_widget requires editor build with widget factory support");
    ErrorCode = TEXT("NOT_AVAILABLE");
    Resp->SetStringField(TEXT("error"), Message);
#endif
  }
  // ===========================================================================
  // SubAction: add_widget_child
  // ===========================================================================
  else if (LowerSub == TEXT("add_widget_child"))
  {
#if WITH_EDITOR && MCP_HAS_WIDGET_FACTORY
    FString WidgetPath;
    if (!Payload->TryGetStringField(TEXT("widgetPath"), WidgetPath) ||
        WidgetPath.IsEmpty())
    {
      Message = TEXT("widgetPath required for add_widget_child");
      ErrorCode = TEXT("INVALID_ARGUMENT");
      Resp->SetStringField(TEXT("error"), Message);
    }
    else
    {
      UWidgetBlueprint *WidgetBP =
          LoadObject<UWidgetBlueprint>(nullptr, *WidgetPath);
      if (!WidgetBP)
      {
        Message = FString::Printf(TEXT("Could not find Widget Blueprint at %s"),
                                  *WidgetPath);
        ErrorCode = TEXT("ASSET_NOT_FOUND");
        Resp->SetStringField(TEXT("error"), Message);
      }
      else
      {
        FString ChildClassPath;
        if (!Payload->TryGetStringField(TEXT("childClass"), ChildClassPath) ||
            ChildClassPath.IsEmpty())
        {
          Message = TEXT("childClass required (e.g. /Script/UMG.Button)");
          ErrorCode = TEXT("INVALID_ARGUMENT");
          Resp->SetStringField(TEXT("error"), Message);
        }
        else
        {
          UClass *WidgetClass =
              UEditorAssetLibrary::FindAssetData(ChildClassPath)
                      .GetAsset()
                      .IsValid()
                  ? LoadClass<UObject>(nullptr, *ChildClassPath)
                  : FindObject<UClass>(nullptr, *ChildClassPath);

          // Try partial search for common UMG widgets
          if (!WidgetClass)
          {
            if (ChildClassPath.Contains(TEXT(".")))
              WidgetClass = FindObject<UClass>(nullptr, *ChildClassPath);
            else
              WidgetClass = FindObject<UClass>(
                  nullptr,
                  *FString::Printf(TEXT("/Script/UMG.%s"), *ChildClassPath));
          }

          if (!WidgetClass || !WidgetClass->IsChildOf(UWidget::StaticClass()))
          {
            Message = FString::Printf(
                TEXT("Could not resolve valid UWidget class from '%s'"),
                *ChildClassPath);
            ErrorCode = TEXT("CLASS_NOT_FOUND");
            Resp->SetStringField(TEXT("error"), Message);
          }
          else
          {
            FString ParentName;
            Payload->TryGetStringField(TEXT("parentName"), ParentName);

            WidgetBP->Modify();

            UWidget *NewWidget =
                WidgetBP->WidgetTree->ConstructWidget<UWidget>(WidgetClass);

            bool bAdded = false;
            bool bIsRoot = false;

            if (ParentName.IsEmpty())
            {
              // Try to set as RootWidget if empty
              if (WidgetBP->WidgetTree->RootWidget == nullptr)
              {
                WidgetBP->WidgetTree->RootWidget = NewWidget;
                bAdded = true;
                bIsRoot = true;
              }
              else
              {
                // Try to add to existing root if it's a panel
                UPanelWidget *RootPanel =
                    Cast<UPanelWidget>(WidgetBP->WidgetTree->RootWidget);
                if (RootPanel)
                {
                  RootPanel->AddChild(NewWidget);
                  bAdded = true;
                }
                else
                {
                  Message = TEXT("Root widget is not a panel and already "
                                 "exists. Specify parentName.");
                  ErrorCode = TEXT("ROOT_Full");
                }
              }
            }
            else
            {
              // Find parent
              UWidget *ParentWidget =
                  WidgetBP->WidgetTree->FindWidget(FName(*ParentName));
              UPanelWidget *ParentPanel = Cast<UPanelWidget>(ParentWidget);
              if (ParentPanel)
              {
                ParentPanel->AddChild(NewWidget);
                bAdded = true;
              }
              else
              {
                Message = FString::Printf(
                    TEXT("Parent '%s' not found or is not a PanelWidget"),
                    *ParentName);
                ErrorCode = TEXT("PARENT_NOT_FOUND");
              }
            }

            if (bAdded)
            {
              bSuccess = true;
              Message = FString::Printf(TEXT("Added %s to %s"),
                                        *WidgetClass->GetName(),
                                        *WidgetBP->GetName());
              Resp->SetStringField(TEXT("widgetName"), NewWidget->GetName());
              Resp->SetStringField(TEXT("childClass"), WidgetClass->GetName());
            }
            else
            {
              if (Message.IsEmpty())
                Message = TEXT("Failed to add widget child.");
              Resp->SetStringField(TEXT("error"), Message);
            }
          }
        }
      }
    }
#else
    Message = TEXT("add_widget_child requires editor build");
    ErrorCode = TEXT("NOT_AVAILABLE");
    Resp->SetStringField(TEXT("error"), Message);
#endif
  }
  // ===========================================================================
  // SubAction: screenshot
  // ===========================================================================
  else if (LowerSub == TEXT("screenshot"))
  {
    // Take a screenshot of the viewport and return as base64
    FString RawScreenshotPath;
    Payload->TryGetStringField(TEXT("path"), RawScreenshotPath);

    FString ScreenshotPath;
    if (RawScreenshotPath.IsEmpty())
    {
      ScreenshotPath =
          FPaths::ProjectSavedDir() / TEXT("Screenshots/WindowsEditor");
    }
    else
    {
      FString SafePath = SanitizeProjectFilePath(RawScreenshotPath);
      if (SafePath.IsEmpty())
      {
        Message = FString::Printf(TEXT("Invalid or unsafe screenshot path: %s. Path must be relative to project."),
                                  *RawScreenshotPath);
        ErrorCode = TEXT("SECURITY_VIOLATION");
        Resp->SetStringField(TEXT("error"), Message);
        SendAutomationResponse(RequestingSocket, RequestId, false, Message,
                               Resp, ErrorCode);
        return true;
      }

      ScreenshotPath = FPaths::ProjectDir() / SafePath;
      ScreenshotPath = FPaths::ConvertRelativePathToFull(ScreenshotPath);
      FPaths::NormalizeFilename(ScreenshotPath);

      FString NormalizedProjectDir =
          FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
      FPaths::NormalizeDirectoryName(NormalizedProjectDir);
      if (!NormalizedProjectDir.EndsWith(TEXT("/")))
      {
        NormalizedProjectDir += TEXT("/");
      }

      if (!ScreenshotPath.StartsWith(NormalizedProjectDir,
                                     ESearchCase::IgnoreCase))
      {
        Message = FString::Printf(TEXT("Invalid or unsafe screenshot path: %s. Path escapes project directory."),
                                  *RawScreenshotPath);
        ErrorCode = TEXT("SECURITY_VIOLATION");
        Resp->SetStringField(TEXT("error"), Message);
        SendAutomationResponse(RequestingSocket, RequestId, false, Message,
                               Resp, ErrorCode);
        return true;
      }
    }

    FString Filename;
    Payload->TryGetStringField(TEXT("filename"), Filename);
    Filename = FPaths::GetCleanFilename(Filename);
    if (Filename.Contains(TEXT("..")) || Filename.Contains(TEXT("/")) ||
        Filename.Contains(TEXT("\\")))
    {
      Filename = FString::Printf(TEXT("Screenshot_%lld"),
                                 FDateTime::Now().ToUnixTimestamp());
    }
    if (Filename.IsEmpty())
    {
      Filename = FString::Printf(TEXT("Screenshot_%lld"),
                                 FDateTime::Now().ToUnixTimestamp());
    }

    bool bReturnBase64 = true;
    Payload->TryGetBoolField(TEXT("returnBase64"), bReturnBase64);

    // Get viewport
    if (!GEngine || !GEngine->GameViewport)
    {
      Message = TEXT("No game viewport available");
      ErrorCode = TEXT("NO_VIEWPORT");
      Resp->SetStringField(TEXT("error"), Message);
    }
    else
    {
      UGameViewportClient *ViewportClient = GEngine->GameViewport;
      FViewport *Viewport = ViewportClient->Viewport;

      if (!Viewport)
      {
        Message = TEXT("No viewport available");
        ErrorCode = TEXT("NO_VIEWPORT");
        Resp->SetStringField(TEXT("error"), Message);
      }
      else
      {
        // Capture viewport pixels
        TArray<FColor> Bitmap;
        FIntVector Size(Viewport->GetSizeXY().X, Viewport->GetSizeXY().Y, 0);

        bool bReadSuccess = Viewport->ReadPixels(Bitmap);

        if (!bReadSuccess || Bitmap.Num() == 0)
        {
          Message = TEXT("Failed to read viewport pixels");
          ErrorCode = TEXT("CAPTURE_FAILED");
          Resp->SetStringField(TEXT("error"), Message);
        }
        else
        {
          // Ensure we have the right size
          const int32 Width = Size.X;
          const int32 Height = Size.Y;

          // Compress to PNG
          // Note: ThumbnailCompressImageArray was introduced in UE 5.1
          TArray<uint8> PngData;
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
          FImageUtils::ThumbnailCompressImageArray(Width, Height, Bitmap,
                                                   PngData);
#else
          // UE 5.0 fallback - use CompressImageArray
          FImageUtils::CompressImageArray(Width, Height, Bitmap, PngData);
#endif

          if (PngData.Num() == 0)
          {
            // Alternative: compress as PNG using IImageWrapper
            IImageWrapperModule &ImageWrapperModule =
                FModuleManager::LoadModuleChecked<IImageWrapperModule>(
                    FName("ImageWrapper"));
            TSharedPtr<IImageWrapper> ImageWrapper =
                ImageWrapperModule.CreateImageWrapper(EImageFormat::PNG);

            if (ImageWrapper.IsValid())
            {
              TArray<uint8> RawData;
              RawData.SetNum(Width * Height * 4);
              for (int32 i = 0; i < Bitmap.Num(); ++i)
              {
                RawData[i * 4 + 0] = Bitmap[i].R;
                RawData[i * 4 + 1] = Bitmap[i].G;
                RawData[i * 4 + 2] = Bitmap[i].B;
                RawData[i * 4 + 3] = Bitmap[i].A;
              }

              if (ImageWrapper->SetRaw(RawData.GetData(), RawData.Num(), Width,
                                       Height, ERGBFormat::RGBA, 8))
              {
                PngData = ImageWrapper->GetCompressed(100);
              }
            }
          }

          FString FullPath =
              FPaths::Combine(ScreenshotPath, Filename + TEXT(".png"));
          FPaths::MakeStandardFilename(FullPath);

          // Always save to disk
          IFileManager::Get().MakeDirectory(*ScreenshotPath, true);
          const bool bSaved = FFileHelper::SaveArrayToFile(PngData, *FullPath);
          if (!bSaved || !IFileManager::Get().FileExists(*FullPath))
          {
            Message = FString::Printf(TEXT("Failed to save screenshot to %s"),
                                      *FullPath);
            ErrorCode = TEXT("SAVE_FAILED");
            Resp->SetStringField(TEXT("error"), Message);
          }
          else
          {
            bSuccess = true;
            Message = FString::Printf(TEXT("Screenshot captured (%dx%d)"), Width,
                                      Height);
            Resp->SetStringField(TEXT("screenshotPath"), FullPath);
            Resp->SetStringField(TEXT("filename"), Filename);
            Resp->SetNumberField(TEXT("width"), Width);
            Resp->SetNumberField(TEXT("height"), Height);
            Resp->SetNumberField(TEXT("sizeBytes"), PngData.Num());

            // Return base64 encoded image if requested
            if (bReturnBase64 && PngData.Num() > 0)
            {
              FString Base64Data = FBase64::Encode(PngData);
              Resp->SetStringField(TEXT("imageBase64"), Base64Data);
              Resp->SetStringField(TEXT("mimeType"), TEXT("image/png"));
            }
          }
        }
      }
    }
  }
  // ===========================================================================
  // SubAction: play_in_editor
  // ===========================================================================
  else if (LowerSub == TEXT("play_in_editor"))
  {
    if (!GEditor)
    {
      Message = TEXT("Editor is not available");
      ErrorCode = TEXT("EDITOR_NOT_AVAILABLE");
      Resp->SetStringField(TEXT("error"), Message);
    }
    // Start play in editor
    else if (GEditor->PlayWorld)
    {
      Message = TEXT("Already playing in editor");
      ErrorCode = TEXT("ALREADY_PLAYING");
      Resp->SetStringField(TEXT("error"), Message);
    }
    else
    {
      // Execute play command
      bool bCommandSuccess = GEditor->Exec(nullptr, TEXT("Play In Editor"));
      if (bCommandSuccess)
      {
        bSuccess = true;
        Message = TEXT("Started play in editor");
        Resp->SetStringField(TEXT("status"), TEXT("playing"));
      }
      else
      {
        Message = TEXT("Failed to start play in editor");
        ErrorCode = TEXT("PLAY_FAILED");
        Resp->SetStringField(TEXT("error"), Message);
      }
    }
  }
  // ===========================================================================
  // SubAction: stop_play
  // ===========================================================================
  else if (LowerSub == TEXT("stop_play"))
  {
    if (!GEditor)
    {
      Message = TEXT("Editor is not available");
      ErrorCode = TEXT("EDITOR_NOT_AVAILABLE");
      Resp->SetStringField(TEXT("error"), Message);
    }
    // Stop play in editor
    else if (GEditor->PlayWorld)
    {
      // Execute stop command
      bool bCommandSuccess =
          GEditor->Exec(nullptr, TEXT("Stop Play In Editor"));
      if (bCommandSuccess)
      {
        bSuccess = true;
        Message = TEXT("Stopped play in editor");
        Resp->SetStringField(TEXT("status"), TEXT("stopped"));
      }
      else
      {
        Message = TEXT("Failed to stop play in editor");
        ErrorCode = TEXT("STOP_FAILED");
        Resp->SetStringField(TEXT("error"), Message);
      }
    }
    else
    {
      Message = TEXT("Not currently playing in editor");
      ErrorCode = TEXT("NOT_PLAYING");
      Resp->SetStringField(TEXT("error"), Message);
    }
  }
  // ===========================================================================
  // SubAction: save_all
  // ===========================================================================
  else if (LowerSub == TEXT("save_all"))
  {
    if (!GEditor)
    {
      Message = TEXT("Editor is not available");
      ErrorCode = TEXT("EDITOR_NOT_AVAILABLE");
      Resp->SetStringField(TEXT("error"), Message);
    }
    else
    {
      // Save all assets and levels
      bool bCommandSuccess = GEditor->Exec(nullptr, TEXT("Asset Save All"));
      if (bCommandSuccess)
      {
        bSuccess = true;
        Message = TEXT("Saved all assets");
        Resp->SetStringField(TEXT("status"), TEXT("saved"));
      }
      else
      {
        Message = TEXT("Failed to save all assets");
        ErrorCode = TEXT("SAVE_FAILED");
        Resp->SetStringField(TEXT("error"), Message);
      }
    }
  }
  // ===========================================================================
  // SubAction: simulate_input
  // ===========================================================================
  else if (LowerSub == TEXT("simulate_input"))
  {
    FString KeyName;
    Payload->TryGetStringField(TEXT("keyName"),
                               KeyName); // Changed to keyName to match schema
    if (KeyName.IsEmpty())
      Payload->TryGetStringField(TEXT("key"), KeyName); // Fallback

    FString EventType;
    Payload->TryGetStringField(TEXT("eventType"), EventType);

    FKey Key = FKey(FName(*KeyName));
    if (Key.IsValid())
    {
      const uint32 CharacterCode = 0;
      const uint32 KeyCode = 0;
      const bool bIsRepeat = false;
      FModifierKeysState ModifierState;

      if (EventType == TEXT("KeyDown"))
      {
        FKeyEvent KeyEvent(Key, ModifierState,
                           FSlateApplication::Get().GetUserIndexForKeyboard(),
                           bIsRepeat, CharacterCode, KeyCode);
        FSlateApplication::Get().ProcessKeyDownEvent(KeyEvent);
      }
      else if (EventType == TEXT("KeyUp"))
      {
        FKeyEvent KeyEvent(Key, ModifierState,
                           FSlateApplication::Get().GetUserIndexForKeyboard(),
                           bIsRepeat, CharacterCode, KeyCode);
        FSlateApplication::Get().ProcessKeyUpEvent(KeyEvent);
      }
      else
      {
        // Press and Release
        FKeyEvent KeyDownEvent(
            Key, ModifierState,
            FSlateApplication::Get().GetUserIndexForKeyboard(), bIsRepeat,
            CharacterCode, KeyCode);
        FSlateApplication::Get().ProcessKeyDownEvent(KeyDownEvent);

        FKeyEvent KeyUpEvent(Key, ModifierState,
                             FSlateApplication::Get().GetUserIndexForKeyboard(),
                             bIsRepeat, CharacterCode, KeyCode);
        FSlateApplication::Get().ProcessKeyUpEvent(KeyUpEvent);
      }

      bSuccess = true;
      Message = FString::Printf(TEXT("Simulated input for key: %s"), *KeyName);
    }
    else
    {
      Message = FString::Printf(TEXT("Invalid key name: %s"), *KeyName);
      ErrorCode = TEXT("INVALID_KEY");
      Resp->SetStringField(TEXT("error"), Message);
    }
  }
  // ===========================================================================
  // SubAction: create_hud
  // ===========================================================================
  else if (LowerSub == TEXT("create_hud"))
  {
    FString WidgetPath;
    Payload->TryGetStringField(TEXT("widgetPath"), WidgetPath);
    UClass *WidgetClass = LoadClass<UUserWidget>(nullptr, *WidgetPath);
    if (WidgetClass && GEngine && GEngine->GameViewport)
    {
      UWorld *World = GEngine->GameViewport->GetWorld();
      if (World)
      {
        UUserWidget *Widget = CreateWidget<UUserWidget>(World, WidgetClass);
        if (Widget)
        {
          Widget->AddToViewport();
          bSuccess = true;
          Message = TEXT("HUD created and added to viewport");
          Resp->SetStringField(TEXT("widgetName"), Widget->GetName());
        }
        else
        {
          Message = TEXT("Failed to create widget");
          ErrorCode = TEXT("CREATE_FAILED");
        }
      }
      else
      {
        Message = TEXT("No world context found (is PIE running?)");
        ErrorCode = TEXT("NO_WORLD");
      }
    }
    else
    {
      Message =
          FString::Printf(TEXT("Failed to load widget class: %s"), *WidgetPath);
      ErrorCode = TEXT("CLASS_NOT_FOUND");
    }
  }
  // ===========================================================================
  // SubAction: set_widget_text
  // ===========================================================================
  else if (LowerSub == TEXT("set_widget_text"))
  {
    FString Key, Value;
    Payload->TryGetStringField(TEXT("key"), Key);
    Payload->TryGetStringField(TEXT("value"), Value);

    bool bFound = false;
    // Iterate all widgets to find one matching Key (Name)
    TArray<UUserWidget *> Widgets;
    UWidgetBlueprintLibrary::GetAllWidgetsOfClass(
        GEditor->GetEditorWorldContext().World(), Widgets,
        UUserWidget::StaticClass(), false);
    // Also try Game Viewport world if Editor World is not right context (PIE)
    if (GEngine && GEngine->GameViewport && GEngine->GameViewport->GetWorld())
    {
      UWidgetBlueprintLibrary::GetAllWidgetsOfClass(
          GEngine->GameViewport->GetWorld(), Widgets,
          UUserWidget::StaticClass(), false);
    }

    for (UUserWidget *Widget : Widgets)
    {
      // Search inside this widget for a TextBlock named Key
      UWidget *Child = Widget->GetWidgetFromName(FName(*Key));
      if (UTextBlock *TextBlock = Cast<UTextBlock>(Child))
      {
        TextBlock->SetText(FText::FromString(Value));
        bFound = true;
        bSuccess = true;
        Message =
            FString::Printf(TEXT("Set text on '%s' to '%s'"), *Key, *Value);
        break;
      }
      // Also check if the widget ITSELF is the one (though UserWidget !=
      // TextBlock usually)
      if (Widget->GetName() == Key)
      {
        // Can't set text on UserWidget directly unless it implements interface?
        // Assuming Key refers to child widget name usually
      }
    }

    if (!bFound)
    {
      // Fallback: Use TObjectIterator to find ANY UTextBlock with that name,
      // risky but covers cases
      for (TObjectIterator<UTextBlock> It; It; ++It)
      {
        if (It->GetName() == Key && It->GetWorld())
        {
          It->SetText(FText::FromString(Value));
          bFound = true;
          bSuccess = true;
          Message = FString::Printf(TEXT("Set text on global '%s'"), *Key);
          break;
        }
      }
    }

    if (!bFound)
    {
      Message = FString::Printf(TEXT("Widget/TextBlock '%s' not found"), *Key);
      ErrorCode = TEXT("WIDGET_NOT_FOUND");
    }
  }
  // ===========================================================================
  // SubAction: set_widget_image
  // ===========================================================================
  else if (LowerSub == TEXT("set_widget_image"))
  {
    FString Key, TexturePath;
    Payload->TryGetStringField(TEXT("key"), Key);
    Payload->TryGetStringField(TEXT("texturePath"), TexturePath);
    UTexture2D *Texture = LoadObject<UTexture2D>(nullptr, *TexturePath);
    if (Texture)
    {
      bool bFound = false;
      for (TObjectIterator<UImage> It; It; ++It)
      {
        if (It->GetName() == Key && It->GetWorld())
        {
          It->SetBrushFromTexture(Texture);
          bFound = true;
          bSuccess = true;
          Message = FString::Printf(TEXT("Set image on '%s'"), *Key);
          break;
        }
      }
      if (!bFound)
      {
        Message = FString::Printf(TEXT("Image widget '%s' not found"), *Key);
        ErrorCode = TEXT("WIDGET_NOT_FOUND");
      }
    }
    else
    {
      Message = TEXT("Failed to load texture");
      ErrorCode = TEXT("ASSET_NOT_FOUND");
    }
  }
  // ===========================================================================
  // SubAction: set_widget_visibility
  // ===========================================================================
  else if (LowerSub == TEXT("set_widget_visibility"))
  {
    FString Key;
    bool bVisible = true;
    Payload->TryGetStringField(TEXT("key"), Key);
    Payload->TryGetBoolField(TEXT("visible"), bVisible);

    bool bFound = false;
    // Try UserWidgets
    for (TObjectIterator<UUserWidget> It; It; ++It)
    {
      if (It->GetName() == Key && It->GetWorld())
      {
        It->SetVisibility(bVisible ? ESlateVisibility::Visible
                                   : ESlateVisibility::Collapsed);
        bFound = true;
        bSuccess = true;
        break;
      }
    }
    // If not found, try generic UWidget
    if (!bFound)
    {
      for (TObjectIterator<UWidget> It; It; ++It)
      {
        if (It->GetName() == Key && It->GetWorld())
        {
          It->SetVisibility(bVisible ? ESlateVisibility::Visible
                                     : ESlateVisibility::Collapsed);
          bFound = true;
          bSuccess = true;
          break;
        }
      }
    }

    if (bFound)
    {
      Message = FString::Printf(TEXT("Set visibility on '%s' to %s"), *Key,
                                bVisible ? TEXT("Visible") : TEXT("Collapsed"));
    }
    else
    {
      Message = FString::Printf(TEXT("Widget '%s' not found"), *Key);
      ErrorCode = TEXT("WIDGET_NOT_FOUND");
    }
  }
  // ===========================================================================
  // SubAction: get_project_settings
  // ===========================================================================
  else if (LowerSub == TEXT("get_project_settings"))
  {
    FString Section;
    Payload->TryGetStringField(TEXT("section"), Section);
    Payload->TryGetStringField(TEXT("category"), Section);

    TSharedPtr<FJsonObject> SettingsObj = MakeShared<FJsonObject>();

    // Get common project settings
    if (GEngine)
    {
      // Engine settings
      SettingsObj->SetStringField(TEXT("engineVersion"), FString::Printf(TEXT("%d.%d"), ENGINE_MAJOR_VERSION, ENGINE_MINOR_VERSION));

      // Project name
      FString ProjectName = FApp::GetProjectName();
      SettingsObj->SetStringField(TEXT("projectName"), ProjectName);

      // Project directory
      FString ProjectDir = FPaths::ProjectDir();
      SettingsObj->SetStringField(TEXT("projectDir"), ProjectDir);

      // Game engine settings via config
      FString ResolutionX, ResolutionY;
      GConfig->GetString(TEXT("/Script/Engine.GameUserSettings"), TEXT("ResolutionSizeX"), ResolutionX, GGameUserSettingsIni);
      GConfig->GetString(TEXT("/Script/Engine.GameUserSettings"), TEXT("ResolutionSizeY"), ResolutionY, GGameUserSettingsIni);
      if (!ResolutionX.IsEmpty() && !ResolutionY.IsEmpty())
      {
        TSharedPtr<FJsonObject> ResObj = MakeShared<FJsonObject>();
        ResObj->SetStringField(TEXT("width"), ResolutionX);
        ResObj->SetStringField(TEXT("height"), ResolutionY);
        SettingsObj->SetObjectField(TEXT("resolution"), ResObj);
      }

      // Fullscreen mode
      FString FullscreenMode;
      GConfig->GetString(TEXT("/Script/Engine.GameUserSettings"), TEXT("LastConfirmedFullscreenMode"), FullscreenMode, GGameUserSettingsIni);
      if (!FullscreenMode.IsEmpty())
      {
        SettingsObj->SetStringField(TEXT("fullscreenMode"), FullscreenMode);
      }
    }

    Resp->SetObjectField(TEXT("settings"), SettingsObj);
    bSuccess = true;
    Message = TEXT("Project settings retrieved");
  }
  // ===========================================================================
  // SubAction: set_project_setting
  // ===========================================================================
  else if (LowerSub == TEXT("set_project_setting"))
  {
    FString Section, Key, Value;
    Payload->TryGetStringField(TEXT("section"), Section);
    Payload->TryGetStringField(TEXT("key"), Key);
    Payload->TryGetStringField(TEXT("value"), Value);

    if (Section.IsEmpty() || Key.IsEmpty())
    {
      Message = TEXT("section and key are required for set_project_setting");
      ErrorCode = TEXT("INVALID_ARGUMENT");
      Resp->SetStringField(TEXT("error"), Message);
    }
    else
    {
      // Try to set the config value
      // First, normalize section format (ensure it starts with /Script/ if it looks like a UE section)
      FString NormalizedSection = Section;
      if (!NormalizedSection.StartsWith(TEXT("/")) && !NormalizedSection.StartsWith(TEXT("[")))
      {
        NormalizedSection = FString::Printf(TEXT("/Script/%s"), *Section);
      }

      // Set the value in the appropriate config file
      // For project settings, use DefaultEngine.ini
      FString ConfigFile = FPaths::ProjectConfigDir() / TEXT("DefaultEngine.ini");

      // Use GConfig to set the value
      GConfig->SetString(*NormalizedSection, *Key, *Value, ConfigFile);
      GConfig->Flush(false, ConfigFile);

      Resp->SetStringField(TEXT("section"), NormalizedSection);
      Resp->SetStringField(TEXT("key"), Key);
      Resp->SetStringField(TEXT("value"), Value);
      bSuccess = true;
      Message = FString::Printf(TEXT("Set %s.%s = %s"), *NormalizedSection, *Key, *Value);
    }
  }
  // ===========================================================================
  // SubAction: remove_widget_from_viewport
  // ===========================================================================
  else if (LowerSub == TEXT("remove_widget_from_viewport"))
  {
    FString Key;
    Payload->TryGetStringField(TEXT("key"),
                               Key); // If empty, remove all? OR specific

    if (Key.IsEmpty())
    {
      // Remove all user widgets?
      TArray<UUserWidget *> TempWidgets;
      UWidgetBlueprintLibrary::GetAllWidgetsOfClass(
          GEditor->GetEditorWorldContext().World(), TempWidgets,
          UUserWidget::StaticClass(), true);
      // Implementation:
      if (GEngine && GEngine->GameViewport &&
          GEngine->GameViewport->GetWorld())
      {
        TArray<UUserWidget *> Widgets;
        UWidgetBlueprintLibrary::GetAllWidgetsOfClass(
            GEngine->GameViewport->GetWorld(), Widgets,
            UUserWidget::StaticClass(), true);
        for (UUserWidget *W : Widgets)
        {
          W->RemoveFromParent();
        }
        bSuccess = true;
        Message = TEXT("Removed all widgets");
      }
    }
    else
    {
      bool bFound = false;
      for (TObjectIterator<UUserWidget> It; It; ++It)
      {
        if (It->GetName() == Key && It->GetWorld())
        {
          It->RemoveFromParent();
          bFound = true;
          bSuccess = true;
          break;
        }
      }
      if (bFound)
      {
        Message = FString::Printf(TEXT("Removed widget '%s'"), *Key);
      }
      else
      {
        Message = FString::Printf(TEXT("Widget '%s' not found"), *Key);
        ErrorCode = TEXT("WIDGET_NOT_FOUND");
      }
    }
  }
  // ===========================================================================
  // Unknown SubAction
  // ===========================================================================
  else
  {
    Message = FString::Printf(
        TEXT("System control action '%s' not implemented"), *LowerSub);
    ErrorCode = TEXT("NOT_IMPLEMENTED");
    Resp->SetStringField(TEXT("error"), Message);
  }

#else
  Message = TEXT("System control actions require editor build.");
  ErrorCode = TEXT("NOT_IMPLEMENTED");
  Resp->SetStringField(TEXT("error"), Message);
#endif

  Resp->SetBoolField(TEXT("success"), bSuccess);
  if (Message.IsEmpty())
  {
    Message = bSuccess ? TEXT("System control action completed")
                       : TEXT("System control action failed");
  }

  SendAutomationResponse(RequestingSocket, RequestId, bSuccess, Message, Resp,
                         ErrorCode);
  return true;
}
