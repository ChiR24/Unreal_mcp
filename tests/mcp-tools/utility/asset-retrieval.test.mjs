#!/usr/bin/env node
// manage_asset thumbnail_sheet, caption_assets and find_assets_like: assets found by what they show and what they are.
// Two cubes in a Rocks folder and a sphere in a Props folder, copied from the engine's basic shapes into a folder of this
// run's own, each given a thumbnail; the caption kept for the sphere is taken away again at the end.
// (tests/mcp-tools/utility/asset-retrieval.test.mjs)

import { runToolTests } from '../../test-runner.mjs';

const ts = Date.now();
const FOLDER = `/Game/MCPTest/Retrieval_${ts}`;
const ROCK_1 = `${FOLDER}/Rocks/SM_Rock_01`;
const ROCK_2 = `${FOLDER}/Rocks/SM_Rock_02`;
const BALL = `${FOLDER}/Props/SM_Ball`;

const testCases = [
  { scenario: 'Setup: a cube as the first rock', toolName: 'manage_asset', arguments: { action: 'duplicate', sourcePath: '/Engine/BasicShapes/Cube', destinationPath: ROCK_1 }, expected: 'success' },
  { scenario: 'Setup: a cube as the second rock', toolName: 'manage_asset', arguments: { action: 'duplicate', sourcePath: '/Engine/BasicShapes/Cube', destinationPath: ROCK_2 }, expected: 'success' },
  { scenario: 'Setup: a sphere as a ball', toolName: 'manage_asset', arguments: { action: 'duplicate', sourcePath: '/Engine/BasicShapes/Sphere', destinationPath: BALL }, expected: 'success' },
  { scenario: 'Setup: the first rock\'s thumbnail', toolName: 'manage_asset', arguments: { action: 'create_thumbnail', assetPath: ROCK_1, width: 256, height: 256 }, expected: 'success' },
  { scenario: 'Setup: the second rock\'s thumbnail', toolName: 'manage_asset', arguments: { action: 'create_thumbnail', assetPath: ROCK_2, width: 256, height: 256 }, expected: 'success' },
  { scenario: 'Setup: the ball\'s thumbnail', toolName: 'manage_asset', arguments: { action: 'create_thumbnail', assetPath: BALL, width: 256, height: 256 }, expected: 'success' },
  { scenario: 'READ: a page of two of the three meshes, with their thumbnails', toolName: 'manage_asset', arguments: { action: 'thumbnail_sheet', folder: FOLDER, class: 'StaticMesh', uncaptionedOnly: false, offset: 0, limit: 2 }, expected: 'success', assertions: [
    { path: 'structuredContent.result.total', equals: 3, label: 'three meshes in the folder' },
    { path: 'structuredContent.result.assets', length: 2, label: 'two on the page' },
    { path: 'structuredContent.result.assets.0.thumbnail', equals: true, label: 'drawn from its thumbnail' },
    { path: 'structuredContent.result.nextOffset', equals: 2, label: 'the next page starts at the third' },
  ] },
  { scenario: 'UPDATE: the ball is captioned', toolName: 'manage_asset', arguments: { action: 'caption_assets', captions: [{ assetPath: BALL, caption: 'a smooth round ball', tags: ['sphere', 'toy'] }] }, expected: 'success', assertions: [
    { path: 'structuredContent.result.captioned', equals: 1, label: 'one caption kept' },
  ] },
  { scenario: 'READ: the uncaptioned page leaves the ball out', toolName: 'manage_asset', arguments: { action: 'thumbnail_sheet', folder: FOLDER }, expected: 'success', assertions: [
    { path: 'structuredContent.result.total', equals: 2, label: 'only the two rocks are left to caption' },
  ] },
  { scenario: 'READ: words find the ball by its caption', toolName: 'manage_asset', arguments: { action: 'find_assets_like', query: 'round toy', folder: FOLDER }, expected: 'success', assertions: [
    { path: 'structuredContent.result.assets.0.assetPath', equals: BALL, label: 'the captioned ball first' },
    { path: 'structuredContent.result.assets.0.why.0', equals: 'words 1', label: 'ranked first by its words' },
  ] },
  { scenario: 'READ: like the first rock, the other rock first', toolName: 'manage_asset', arguments: { action: 'find_assets_like', likeAsset: ROCK_1, folder: FOLDER, limit: 5 }, expected: 'success', assertions: [
    { path: 'structuredContent.result.assets.0.assetPath', equals: ROCK_2, label: 'the other rock of its folder' },
    { path: 'structuredContent.result.assets', length: 2, label: 'never the example itself (two of the three)' },
  ] },
  { scenario: 'READ: by size and colour among the meshes', toolName: 'manage_asset', arguments: { action: 'find_assets_like', query: 'ball', sizeCm: 100, colour: 'white', class: 'StaticMesh', folder: FOLDER }, expected: 'success', assertions: [
    { path: 'structuredContent.result.considered', equals: 3, label: 'the three meshes' },
    { path: 'structuredContent.result.assets.0.assetPath', equals: BALL, label: 'the ball leads' },
  ] },
  { scenario: 'ERROR: an example that is not an asset', toolName: 'manage_asset', arguments: { action: 'find_assets_like', likeAsset: `${FOLDER}/SM_NoSuchRock` }, expected: 'error|ASSET_NOT_FOUND' },
  { scenario: 'ERROR: a colour it cannot read', toolName: 'manage_asset', arguments: { action: 'find_assets_like', colour: 'plaid' }, expected: 'error|INVALID_ARGUMENT' },
  { scenario: 'ERROR: a query with no word in it', toolName: 'manage_asset', arguments: { action: 'find_assets_like', query: '?' }, expected: 'error|INVALID_ARGUMENT' },
  { scenario: 'ERROR: a folder outside the project', toolName: 'manage_asset', arguments: { action: 'thumbnail_sheet', folder: '../Outside' }, expected: 'error' },
  { scenario: 'ERROR: a caption for an asset that does not exist keeps none', toolName: 'manage_asset', arguments: { action: 'caption_assets', captions: [{ assetPath: ROCK_1, caption: 'a cube' }, { assetPath: `${FOLDER}/SM_NoSuchRock`, caption: 'nothing' }] }, expected: 'error|ASSET_NOT_FOUND' },
  { scenario: 'Cleanup: delete the meshes', toolName: 'manage_asset', arguments: { action: 'delete', paths: [ROCK_1, ROCK_2, BALL], force: true }, expected: 'success|not found' },
  { scenario: 'Cleanup: the deleted ball\'s caption is taken away', toolName: 'manage_asset', arguments: { action: 'caption_assets', captions: [{ assetPath: BALL, caption: '' }] }, expected: 'success', assertions: [
    { path: 'structuredContent.result.removed', equals: 1, label: 'removed although the asset is gone' },
  ] },
];

runToolTests('asset-retrieval', testCases, {});
