import { describe, expect, it } from 'vitest';
import { withHandlerContext, createUnknownActionResponse } from './handler-error-context.js';
import { ResponseFactory } from '../../../../utils/responses/response-factory.js';

describe('withHandlerContext', () => {
  it('resolves with the handler return value on success', async () => {
    const handler = async () => ({ success: true, data: 'test' });
    const result = await withHandlerContext(handler);
    expect(result).toEqual({ success: true, data: 'test' });
  });

  it('thrown Error → resolves with exactly what ResponseFactory.error returns', async () => {
    const error = new Error('something went wrong');
    const handler = async () => { throw error; };
    const result = await withHandlerContext(handler);
    expect(result).toEqual(ResponseFactory.error(error));
  });

  it('thrown non-Error value (string) → same delegation equality', async () => {
    const handler = async () => { throw 'oops'; };
    const result = await withHandlerContext(handler);
    expect(result).toEqual(ResponseFactory.error('oops'));
  });

  it('thrown non-Error value (object) → same delegation equality', async () => {
    const nonError = { reason: 'bad input' };
    const handler = async () => { throw nonError; };
    const result = await withHandlerContext(handler);
    expect(result).toEqual(ResponseFactory.error(nonError));
  });

  it('rejected promise → same delegation equality', async () => {
    const reason = new Error('promise rejected');
    const handler = async () => Promise.reject(reason);
    const result = await withHandlerContext(handler);
    expect(result).toEqual(ResponseFactory.error(reason));
  });

  it('rejected promise with non-Error reason → same delegation equality', async () => {
    const handler = async () => Promise.reject('fail');
    const result = await withHandlerContext(handler);
    expect(result).toEqual(ResponseFactory.error('fail'));
  });
});

describe('createUnknownActionResponse', () => {
  it('base shape { success:false, error:"UNKNOWN_ACTION", message } for message+extra', () => {
    const result = createUnknownActionResponse('Action not found');
    expect(result).toEqual({
      success: false,
      error: 'UNKNOWN_ACTION',
      message: 'Action not found'
    });
  });

  it('extra fields are spread in', () => {
    const result = createUnknownActionResponse('Action not found', {
      action: 'do_something',
      assetPath: '/Game/Foo'
    });
    expect(result).toEqual({
      success: false,
      error: 'UNKNOWN_ACTION',
      message: 'Action not found',
      action: 'do_something',
      assetPath: '/Game/Foo'
    });
  });

  it('extra omitted → no undefined keys', () => {
    const result = createUnknownActionResponse('Action not found');
    expect(result).not.toHaveProperty('extra');
    expect(Object.values(result)).not.toContain(undefined);
  });

  it('cleanObject applied (undefined-valued extra field is stripped)', () => {
    const result = createUnknownActionResponse('Action not found', {
      action: 'do_something',
      maybeUndefined: undefined
    });
    expect(result).toEqual({
      success: false,
      error: 'UNKNOWN_ACTION',
      message: 'Action not found',
      action: 'do_something'
    });
    expect(result).not.toHaveProperty('maybeUndefined');
  });
});
