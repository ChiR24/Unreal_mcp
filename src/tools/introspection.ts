import { UnrealBridge } from '../unreal-bridge.js';

export class IntrospectionTools {
  constructor(private bridge: UnrealBridge) {}

  async inspectObject(params: { objectPath: string }) {
    const py = `\nimport unreal, json, inspect\npath = r"${params.objectPath}"\ntry:\n    obj = unreal.load_object(None, path)\n    if not obj:\n        print('RESULT:' + json.dumps({'success': False, 'error': 'Object not found'}))\n    else:\n        info = {'class': obj.get_class().get_name() if hasattr(obj, 'get_class') else str(type(obj)), 'name': obj.get_name() if hasattr(obj, 'get_name') else '', 'path': path, 'properties': []}\n        # Best-effort property discovery\n        try:\n            cls = obj.get_class()\n            props = []\n            try:\n                for p in cls.properties():\n                    props.append(str(p.get_name()))\n            except Exception:\n                pass\n            if not props:\n                # Fallback: try common editor properties\n                props = ['location','rotation','scale','tags']\n            info['properties'] = props\n        except Exception:\n            pass\n        print('RESULT:' + json.dumps({'success': True, 'info': info}))\nexcept Exception as e:\n    print('RESULT:' + json.dumps({'success': False, 'error': str(e)}))\n`.trim();
    const resp = await this.bridge.executePython(py);
    const out = typeof resp === 'string' ? resp : JSON.stringify(resp);
    const m = out.match(/RESULT:({.*})/);
    if (m) { try { return JSON.parse(m[1]); } catch {} }
    return { success: false, error: 'Failed to inspect object' };
  }

  async setProperty(params: { objectPath: string; propertyName: string; value: any }) {
    try {
      const res = await this.bridge.httpCall('/remote/object/property', 'PUT', {
        objectPath: params.objectPath,
        propertyName: params.propertyName,
        propertyValue: params.value
      });
      return { success: true, result: res };
    } catch (err: any) {
      return { success: false, error: String(err?.message || err) };
    }
  }
}
