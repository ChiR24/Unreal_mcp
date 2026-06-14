import re
import os
import sys

file_path = os.path.join("plugins", "McpAutomationBridge", "Source", "McpAutomationBridge", "Private", "Domains", "WidgetAuthoring", "McpAutomationBridge_WidgetAuthoringHandlers.cpp")

if os.path.exists(file_path):
    with open(file_path, "r", encoding="utf-8") as f:
        content = f.read()

    old_reparent_pattern = r'if \(SubAction\.Equals\(TEXT\("reparent_widget"\), ESearchCase::IgnoreCase\)\)\s*\{\s*FString WidgetPath = GetJsonStringField\(Payload, TEXT\("widgetPath"\)\);\s*FString SlotName = GetJsonStringField\(Payload, TEXT\("slotName"\)\);.*?return true;\s*\}'
    content = re.sub(old_reparent_pattern, '', content, flags=re.DOTALL)

    section_start = content.find('19.4 Advanced UMG JSON')
    if section_start != -1:
        section_content = content[section_start:]
        pre_content = content[:section_start]
        
        if 'SanitizeProjectRelativePath(' not in section_content:
            section_content = section_content.replace(
                'FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));',
                'FString WidgetPath = SanitizeProjectRelativePath(GetJsonStringField(Payload, TEXT("widgetPath")));'
            )
        
        get_subsystem_replacement = '''if (!GEditor)
            {
                SendAutomationError(RequestingSocket, RequestId, TEXT("Editor is not available"), TEXT("EDITOR_UNAVAILABLE"));
                return true;
            }
            UUmgGetSubsystem* GetSubsystem = GEditor->GetEditorSubsystem<UUmgGetSubsystem>();'''
        
        set_subsystem_replacement = '''if (!GEditor)
            {
                SendAutomationError(RequestingSocket, RequestId, TEXT("Editor is not available"), TEXT("EDITOR_UNAVAILABLE"));
                return true;
            }
            UUmgSetSubsystem* SetSubsystem = GEditor->GetEditorSubsystem<UUmgSetSubsystem>();'''
        
        # A better idempotency check is replacing only if the EXACT old string is found, AND the replacement string isn't.
        if 'UUmgSetSubsystem* SetSubsystem = GEditor->GetEditorSubsystem<UUmgSetSubsystem>();' in section_content:
            # Check if the block right before it is `if (!GEditor)`
            if get_subsystem_replacement not in section_content:
                 section_content = section_content.replace('UUmgGetSubsystem* GetSubsystem = GEditor->GetEditorSubsystem<UUmgGetSubsystem>();', get_subsystem_replacement)
            if set_subsystem_replacement not in section_content:
                 section_content = section_content.replace('UUmgSetSubsystem* SetSubsystem = GEditor->GetEditorSubsystem<UUmgSetSubsystem>();', set_subsystem_replacement)
        
        content = pre_content + section_content
    else:
        print("Error: 19.4 Advanced UMG JSON section not found! Skipping safely.")
        sys.exit(0)

    with open(file_path, "w", encoding="utf-8") as f:
        f.write(content)

print("Patching complete.")
