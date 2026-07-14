import re

file_path = r"src\tools\handlers\widget-authoring-handlers.ts"

with open(file_path, "r", encoding="utf-8") as f:
    content = f.read()

# Add import if missing
if "sanitizePath" not in content:
    content = content.replace("import { ITools }", "import { sanitizePath } from '../../utils/path-security.js';\nimport { ITools }")

# Add a helper function for JSON validation
json_helper = """
function requireValidJson(jsonString: string | undefined, paramName: string): void {
  requireNonEmptyString(jsonString, paramName, `Missing required parameter: ${paramName}`);
  try {
    JSON.parse(jsonString as string);
  } catch (e) {
    throw new Error(`Invalid JSON provided for ${paramName}: ${(e as Error).message}`);
  }
}
"""

# Insert json_helper after finiteNumber function
if "requireValidJson" not in content:
    content = content.replace("function finiteNumber", json_helper + "\nfunction finiteNumber")

# Replace instances of requireNonEmptyString for JSON fields
content = content.replace(
    "requireNonEmptyString(argsRecord.widgetTreeJson, 'widgetTreeJson', 'Missing required parameter: widgetTreeJson');",
    "requireValidJson(argsRecord.widgetTreeJson as string, 'widgetTreeJson');"
)
content = content.replace(
    "requireNonEmptyString(argsRecord.propertiesJson, 'propertiesJson', 'Missing required parameter: propertiesJson');",
    "requireValidJson(argsRecord.propertiesJson as string, 'propertiesJson');"
)

# Sanitize widgetPath for all the new actions before calling sendRequest
actions_to_patch = [
    'export_widget_tree',
    'apply_widget_tree',
    'query_widget_properties',
    'set_widget_properties',
    'get_layout_data',
    'reparent_widget',
    'delete_widget'
]

for action in actions_to_patch:
    # Find the case block
    pattern = rf"(case '{action}': {{\s*requireNonEmptyString\(argsRecord\.widgetPath, 'widgetPath', 'Missing required parameter: widgetPath'\);(.*?)\s*return sendRequest\('{action}'\);\s*}})"
    
    def repl(m):
        inner = m.group(2)
        new_inner = inner + "\n      argsRecord.widgetPath = sanitizePath(argsRecord.widgetPath as string);\n"
        return f"case '{action}': {{\n      requireNonEmptyString(argsRecord.widgetPath, 'widgetPath', 'Missing required parameter: widgetPath');{new_inner}      return sendRequest('{action}');\n    }}"
    
    content = re.sub(pattern, repl, content, flags=re.DOTALL)


with open(file_path, "w", encoding="utf-8") as f:
    f.write(content)

print("TS Patching complete.")
