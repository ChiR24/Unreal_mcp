// Blueprint behaviour recipes are data files the plugin reads at run time
// (McpBlueprintBehaviour::LoadRecipe, Resources/Recipes/<Domain>/<Name>.json), so
// no compiler ever sees them: a recipe bug first shows up as an INVALID_RECIPE
// reply in a live editor. This reads every recipe file and applies the checks the
// plugin makes when it builds a plan, plus two it cannot make: a placeholder no
// handler knows to fill, and a function call that resolves only on some Blueprints.

import { readFileSync, readdirSync } from 'node:fs';
import { join, relative, resolve } from 'node:path';

import { describe, expect, it } from 'vitest';

type Json = string | number | boolean | null | Json[] | { [key: string]: Json };
type JsonObject = { [key: string]: Json };

const pluginRoot = resolve(process.cwd(), 'plugins/McpAutomationBridge');
const recipeRoot = join(pluginRoot, 'Resources', 'Recipes');

// The fields the plugin accepts: BuildPlan (Recipe.cpp), PlanMembers (Members.cpp),
// CheckHooks (Hooks.cpp) and PlanInput (Input.cpp). Keep these in step with them.
const RECIPE_FIELDS = ['description', 'tag', 'replace', 'placeholders', 'variables', 'dispatchers', 'customEvents',
  'functions', 'hooks', 'input', 'eventGraph'];
const SECTION_FIELDS: Readonly<Record<string, readonly string[]>> = {
  variables: ['variableName', 'variableType', 'defaultValue', 'category', 'isReplicated', 'instanceEditable',
    'exposeOnSpawn', 'keepExisting'],
  dispatchers: ['name', 'parameters'],
  customEvents: ['id', 'eventName', 'shared'],
  functions: ['id', 'functionName', 'inputs', 'outputs', 'pure', 'isPublic', 'operations'],
  hooks: ['id', 'event', 'customEvent', 'component'],
};
const INPUT_FIELDS = ['id', 'inputActionPath', 'mappingContextPath', 'key', 'registerContext'];

// A CallFunction step without memberClass resolves on the Blueprint's own class or on
// KismetSystemLibrary, GameplayStatics, KismetMathLibrary, KismetStringLibrary and
// KismetTextLibrary (ResolveGraphCallFunction). A name listed here must be a function
// of one of those libraries, so it resolves on every Blueprint; a function of the
// Blueprint's parent class (Jump, LaunchCharacter) needs memberClass in the recipe.
const DEFAULT_CLASS_FUNCTIONS = new Set(['PrintString', 'Delay', 'Not_PreBool', 'SelectFloat']);
// The spellings the build_graph pre-check treats as a function call (IsCallFunctionType).
const CALL_FUNCTION_TYPES = new Set(['CallFunction', 'K2Node_CallFunction', 'FunctionCall']);
// IsRecipeToken: a placeholder, domain or recipe name.
const TOKEN = /^[A-Za-z0-9_]+$/u;

const isObject = (value: Json | undefined): value is JsonObject =>
  typeof value === 'object' && value !== null && !Array.isArray(value);
const objectsIn = (value: Json | undefined): JsonObject[] => (Array.isArray(value) ? value.filter(isObject) : []);
const text = (value: Json | undefined): string => (typeof value === 'string' ? value : '');

/** Everything the plugin would refuse in this recipe, or could not see at all. */
function recipeProblems(recipe: Json): string[] {
  if (!isObject(recipe)) {
    return ['the recipe is not a JSON object'];
  }
  const problems: string[] = [];
  const unknownFields = (where: string, object: JsonObject, allowed: readonly string[]): void => {
    for (const key of Object.keys(object)) {
      if (!allowed.includes(key)) problems.push(`${where} has an unknown field '${key}'`);
    }
  };
  unknownFields('the recipe', recipe, RECIPE_FIELDS);
  if (text(recipe.tag).trim() === '') problems.push('the recipe has no tag');
  if (text(recipe.description) === '') problems.push('the recipe has no description');
  for (const [section, allowed] of Object.entries(SECTION_FIELDS)) {
    objectsIn(recipe[section]).forEach((entry, index) => unknownFields(`${section}[${index}]`, entry, allowed));
  }
  if (isObject(recipe.input)) unknownFields('input', recipe.input, INPUT_FIELDS);

  // ParseRecipe: a string value is exactly "{{Name}}" or holds no braces at all.
  const used = new Set<string>();
  const walk = (value: Json, where: string): void => {
    if (typeof value === 'string') {
      if (!value.includes('{{') && !value.includes('}}')) return;
      const name = value.slice(2, -2);
      if (value.startsWith('{{') && value.endsWith('}}') && TOKEN.test(name)) used.add(name);
      else problems.push(`${where}: '${value}' is not a placeholder, which is a whole string "{{Name}}"`);
    } else if (Array.isArray(value)) {
      value.forEach((item, index) => walk(item, `${where}[${index}]`));
    } else if (isObject(value)) {
      for (const [key, item] of Object.entries(value)) walk(item, `${where}.${key}`);
    }
  };
  walk(recipe, 'recipe');
  // The handler fills what `placeholders` lists; a placeholder it does not know stays unfilled.
  const declared = isObject(recipe.placeholders) ? Object.keys(recipe.placeholders) : [];
  for (const name of used) {
    if (!declared.includes(name)) problems.push(`placeholder {{${name}}} is not listed in placeholders, so no handler fills it`);
  }
  for (const name of declared) {
    if (!used.has(name)) problems.push(`placeholder ${name} is listed in placeholders but never used`);
  }

  const own = new Set([...objectsIn(recipe.functions).map((fn) => text(fn.functionName)),
    ...objectsIn(recipe.customEvents).map((event) => text(event.eventName))]);
  const steps = [
    ...objectsIn(recipe.eventGraph).map((step, index) => ({ step, where: `eventGraph[${index}]` })),
    ...objectsIn(recipe.functions).flatMap((fn, owner) =>
      objectsIn(fn.operations).map((step, index) => ({ step, where: `functions[${owner}].operations[${index}]` }))),
  ];
  for (const { step, where } of steps) {
    if (step.edit !== 'create_node' || !CALL_FUNCTION_TYPES.has(text(step.nodeType))) continue;
    // The pre-check reads memberClass, else targetClass.
    if ((typeof step.memberClass === 'string' ? step.memberClass : text(step.targetClass)) !== '') continue;
    const name = text(step.memberName) || text(step.functionName);
    if (!own.has(name) && !DEFAULT_CLASS_FUNCTIONS.has(name)) {
      problems.push(`${where} calls '${name}' without memberClass; it is not a function or custom event of the recipe `
        + 'nor a stock library function listed in DEFAULT_CLASS_FUNCTIONS');
    }
  }
  return problems;
}

const recipeFiles = (directory: string): string[] =>
  readdirSync(directory, { withFileTypes: true }).flatMap((entry) => {
    const path = join(directory, entry.name);
    if (entry.isDirectory()) return recipeFiles(path);
    return entry.name.endsWith('.json') ? [path] : [];
  });

const recipes = recipeFiles(recipeRoot).map((file) => ({ name: relative(recipeRoot, file).replace(/\\/gu, '/'), file }));

describe('Blueprint behaviour recipe files', () => {
  it('finds the recipe files it checks', () => {
    // An empty list would make every check below pass vacuously.
    expect(recipes.length).toBeGreaterThan(0);
  });

  it.each(recipes)('$name is a recipe the plugin accepts', ({ file }) => {
    expect(recipeProblems(JSON.parse(readFileSync(file, 'utf8')) as Json)).toEqual([]);
  });

  it('keeps each file where LoadRecipe looks: <Domain>/<Name>.json with token names', () => {
    for (const { name } of recipes) {
      const parts = name.replace(/\.json$/u, '').split('/');
      expect(parts, name).toHaveLength(2);
      for (const part of parts) expect(part, name).toMatch(TOKEN);
    }
  });

  it('ships the recipes with the packaged plugin', () => {
    // UAT BuildPlugin packages Resources/... by default; FilterPlugin.ini must not take it out.
    const filter = readFileSync(join(pluginRoot, 'Config', 'FilterPlugin.ini'), 'utf8');
    expect(filter).not.toMatch(/^\s*-\s*\/Resources/imu);
  });

  it('catches each recipe mistake it is there for', () => {
    const problems = recipeProblems({
      description: 'd', tag: 't', colour: 'red', placeholders: { Speed: 'walk speed' },
      variables: [{ variableName: 'A', variableType: 'float', defaultValue: '{{Walk}}', flavour: 1 }],
      eventGraph: [
        { edit: 'create_node', id: 'a', nodeType: 'CallFunction', memberName: 'LaunchCharacter' },
        { edit: 'set_pin_default_value', nodeId: '$a', pinName: 'LaunchVelocity', propertyValue: 'x {{Speed}}' },
      ],
    });
    expect(problems).toEqual(expect.arrayContaining([
      expect.stringContaining("unknown field 'colour'"),
      expect.stringContaining("unknown field 'flavour'"),
      expect.stringContaining('{{Walk}} is not listed'),
      expect.stringContaining("'x {{Speed}}' is not a placeholder"),
      expect.stringContaining('Speed is listed in placeholders but never used'),
      expect.stringContaining("'LaunchCharacter' without memberClass"),
    ]));
    expect(problems).toHaveLength(6);
  });

  it('accepts a recipe that calls only its own members, stock functions and named classes', () => {
    expect(recipeProblems({
      description: 'd', tag: 't', placeholders: { Message: 'text to print' },
      customEvents: [{ id: 'ping', eventName: 'Ping' }],
      functions: [{ id: 'go', functionName: 'Go', operations: [{ edit: 'create_node', nodeType: 'CallFunction', memberName: 'Ping' }] }],
      eventGraph: [
        { edit: 'create_node', nodeType: 'CallFunction', memberName: 'Go' },
        { edit: 'create_node', nodeType: 'CallFunction', memberName: 'PrintString', pinDefaults: { InString: '{{Message}}' } },
        { edit: 'create_node', nodeType: 'CallFunction', memberName: 'LaunchCharacter', memberClass: 'Character' },
      ],
    })).toEqual([]);
  });
});
