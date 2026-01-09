import js from '@eslint/js';
import tseslintPlugin from '@typescript-eslint/eslint-plugin';
import tsParser from '@typescript-eslint/parser';
import { dirname } from 'node:path';
import { fileURLToPath } from 'node:url';

const __dirname = dirname(fileURLToPath(import.meta.url));

const tsRecommendedConfigs = tseslintPlugin.configs['flat/recommended'].map((config) => ({
  ...config,
  languageOptions: {
    ...config.languageOptions,
    parser: tsParser,
    parserOptions: {
      ...(config.languageOptions?.parserOptions ?? {}),
      ecmaVersion: 2022,
      sourceType: 'module',
      tsconfigRootDir: __dirname,
    },
  },
}));

export default [
  {
    ignores: [
      'dist/**',
      'node_modules/**',
      '**/*.js',
      '**/*.d.ts',
      '**/*.cjs',
      '**/*.mjs',
      'eslint.config.*',
      'vitest.config.ts',
      'test-*.js',
      'tests/**/*.mjs',
    ],
  },
  js.configs.recommended,
  ...tsRecommendedConfigs,
  {
    files: ['**/*.ts'],
    rules: {
      '@typescript-eslint/no-unused-vars': [
        'warn',
        {
          args: 'all',
          argsIgnorePattern: '^_',
          caughtErrors: 'all',
          caughtErrorsIgnorePattern: '^_',
          destructuredArrayIgnorePattern: '^_',
          varsIgnorePattern: '^_',
          ignoreRestSiblings: true,
        },
      ],
      '@typescript-eslint/no-explicit-any': 'error',
      '@typescript-eslint/explicit-module-boundary-types': 'off',
      '@typescript-eslint/no-non-null-assertion': 'warn',
      'no-console': 'warn',
      semi: ['error', 'always'],
      quotes: ['error', 'single', { avoidEscape: true }],
      'no-empty': ['error', { allowEmptyCatch: false }],
      'no-useless-escape': 'warn',
      'no-case-declarations': 'off',
      'prefer-const': 'error',
      'no-unused-vars': 'off',
    },
  },
];
