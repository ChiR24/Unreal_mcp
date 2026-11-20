# Sample: a plugin that adds a capability to EngineWire

`EngineWireSample/` is a complete editor plugin that adds one capability, `ext_enginewiresample.echo`, to EngineWire's
`unreal` tool through `IEngineWireCapabilityProvider` (see [Extending the EngineWire plugin](../../docs/editor-plugin-extension.md#capabilities-from-another-plugin)).

To try it:

1. Copy `EngineWireSample/` into your project's `Plugins/` folder beside `EngineWire/` and build the project.
2. In Project Settings > EngineWire > Trusted Extension Providers, add `EngineWireSample`, then restart the editor.
3. `manage_tools list_extensions` shows it joined; search `echo text` finds it; executing it with `{"text": "hello"}`
   answers `Echoed 5 character(s).`

Without step 2 the provider is listed as not trusted and adds nothing.
