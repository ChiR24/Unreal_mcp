import re

file_path = r"plugins\McpAutomationBridge\Source\McpAutomationBridge\Private\McpAutomationBridge_WidgetAuthoringHandlers.cpp"

with open(file_path, "r", encoding="utf-8") as f:
    content = f.read()

# 1. Eliminar el bloque viejo de reparent_widget
old_reparent_pattern = r'if \(SubAction\.Equals\(TEXT\("reparent_widget"\), ESearchCase::IgnoreCase\)\)\s*\{\s*FString WidgetPath = GetJsonStringField\(Payload, TEXT\("widgetPath"\)\);\s*FString SlotName = GetJsonStringField\(Payload, TEXT\("slotName"\)\);.*?return true;\s*\}'
content = re.sub(old_reparent_pattern, '', content, flags=re.DOTALL)

# 2. En el nuevo bloque "19.4 Advanced UMG JSON", agregar sanitización y validación GEditor.
# Buscamos el inicio de la sección
section_start = content.find('19.4 Advanced UMG JSON')
if section_start != -1:
    section_content = content[section_start:]
    pre_content = content[:section_start]
    
    # Sanitizar WidgetPath
    # Reemplazamos: FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
    # Por: FString WidgetPath = SanitizeProjectRelativePath(GetJsonStringField(Payload, TEXT("widgetPath")));
    section_content = section_content.replace(
        'FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));',
        'FString WidgetPath = SanitizeProjectRelativePath(GetJsonStringField(Payload, TEXT("widgetPath")));'
    )
    
    # Proteger GetEditorSubsystem<UUmgGetSubsystem>()
    get_subsystem_replacement = '''if (!GEditor)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Editor is not available"), TEXT("EDITOR_UNAVAILABLE"));
            return true;
        }
        UUmgGetSubsystem* GetSubsystem = GEditor->GetEditorSubsystem<UUmgGetSubsystem>();'''
    section_content = section_content.replace('UUmgGetSubsystem* GetSubsystem = GEditor->GetEditorSubsystem<UUmgGetSubsystem>();', get_subsystem_replacement)
    
    # Proteger GetEditorSubsystem<UUmgSetSubsystem>()
    set_subsystem_replacement = '''if (!GEditor)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Editor is not available"), TEXT("EDITOR_UNAVAILABLE"));
            return true;
        }
        UUmgSetSubsystem* SetSubsystem = GEditor->GetEditorSubsystem<UUmgSetSubsystem>();'''
    section_content = section_content.replace('UUmgSetSubsystem* SetSubsystem = GEditor->GetEditorSubsystem<UUmgSetSubsystem>();', set_subsystem_replacement)
    
    content = pre_content + section_content

with open(file_path, "w", encoding="utf-8") as f:
    f.write(content)

print("Patching complete.")
