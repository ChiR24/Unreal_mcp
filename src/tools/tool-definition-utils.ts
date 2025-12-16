
export const commonSchemas = {
  location: { 
    type: 'object', 
    properties: { 
      x: { type: 'number' }, 
      y: { type: 'number' }, 
      z: { type: 'number' } 
    } 
  },
  rotation: { 
    type: 'object', 
    properties: { 
      pitch: { type: 'number' }, 
      yaw: { type: 'number' }, 
      roll: { type: 'number' } 
    } 
  },
  scale: { 
    type: 'object', 
    properties: { 
      x: { type: 'number' }, 
      y: { type: 'number' }, 
      z: { type: 'number' } 
    } 
  },
  vector3: {
     type: 'object',
     properties: {
         x: { type: 'number' },
         y: { type: 'number' },
         z: { type: 'number' }
     }
  }
};
