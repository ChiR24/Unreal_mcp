#!/usr/bin/env node
import { readFile } from 'fs/promises';

async function validateServerJson() {
  try {
    // Read the server.json file
    const serverJson = JSON.parse(await readFile('server.json', 'utf8'));

    console.log('✅ server.json is valid JSON!');
    console.log('📋 Server Information:');
    console.log(`   - Name: ${serverJson.name}`);
    console.log(`   - Version: ${serverJson.version}`);
    console.log(`   - Description: ${serverJson.description}`);
    console.log(`   - Package Count: ${serverJson.packages?.length || 0}`);
    console.log(`   - Keywords: ${serverJson.keywords?.length || 0} tags`);
    console.log(`   - Environment Variables: ${serverJson.packages?.[0]?.environmentVariables?.length || 0}`);

    // Basic structure validation
    const requiredFields = ['name', 'description', 'version', 'packages'];
    const missingFields = requiredFields.filter(field => !serverJson[field]);

    if (missingFields.length > 0) {
      console.log(`⚠️  Missing required fields: ${missingFields.join(', ')}`);
      return false;
    }

    // Validate package structure
    if (serverJson.packages && serverJson.packages.length > 0) {
      const pkg = serverJson.packages[0];
      if (!pkg.identifier || !pkg.registryType || !pkg.transport) {
        console.log('⚠️  Package missing required fields (identifier, registryType, transport)');
        return false;
      }
    }

    console.log('✅ server.json structure validation passed!');
    console.log('✅ Ready for MCP Registry submission!');
    return true;

  } catch (error) {
    console.error('❌ Validation error:', error.message);
    return false;
  }
}

validateServerJson();