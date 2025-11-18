// SPDX-License-Identifier: CC-BY-NC-SA-4.0
import { describe, it, expect } from '@jest/globals';
import {
  EthervoxError,
  AudioError,
  ModelError,
  PlatformError,
  NetworkError,
  PluginError,
  Ok,
  Err,
  type Result
} from '../../src/common/errors';

describe('EthervoxError', () => {
  it('should create base error with context', () => {
    const error = new EthervoxError('Test error', 'TEST_ERROR', {
      detail: 'some detail'
    });
    
    expect(error.message).toBe('Test error');
    expect(error.code).toBe('TEST_ERROR');
    expect(error.context).toEqual({ detail: 'some detail' });
    expect(error.timestamp).toBeInstanceOf(Date);
    expect(error.name).toBe('EthervoxError');
  });
  
  it('should serialize to JSON', () => {
    const error = new EthervoxError('Test error', 'TEST_ERROR', { key: 'value' });
    const json = error.toJSON();
    
    expect(json.name).toBe('EthervoxError');
    expect(json.message).toBe('Test error');
    expect(json.code).toBe('TEST_ERROR');
    expect(json.context).toEqual({ key: 'value' });
    expect(json.timestamp).toBeInstanceOf(Date);
    expect(json.stack).toBeDefined();
  });
});

describe('Specific Error Types', () => {
  it('should create AudioError', () => {
    const error = new AudioError('Device not found');
    expect(error).toBeInstanceOf(AudioError);
    expect(error).toBeInstanceOf(EthervoxError);
    expect(error.code).toBe('AUDIO_ERROR');
    expect(error.name).toBe('AudioError');
  });
  
  it('should create ModelError', () => {
    const error = new ModelError('Model load failed');
    expect(error.code).toBe('MODEL_ERROR');
    expect(error.name).toBe('ModelError');
  });
  
  it('should create PlatformError', () => {
    const error = new PlatformError('HAL not found');
    expect(error.code).toBe('PLATFORM_ERROR');
    expect(error.name).toBe('PlatformError');
  });
  
  it('should create NetworkError', () => {
    const error = new NetworkError('Connection failed', {
      url: 'https://example.com',
      status: 500
    });
    expect(error.code).toBe('NETWORK_ERROR');
    expect(error.context?.url).toBe('https://example.com');
  });
  
  it('should create PluginError', () => {
    const error = new PluginError('Plugin execution failed');
    expect(error.code).toBe('PLUGIN_ERROR');
  });
});

describe('Result Type', () => {
  it('should create success result', () => {
    const result = Ok('test value');
    expect(result.success).toBe(true);
    if (result.success) {
      expect(result.value).toBe('test value');
    }
  });
  
  it('should create error result', () => {
    const error = new AudioError('Test error');
    const result = Err(error);
    expect(result.success).toBe(false);
    if (!result.success) {
      expect(result.error).toBe(error);
      expect(result.error.code).toBe('AUDIO_ERROR');
    }
  });
  
  it('should handle Result type in function', () => {
    function divide(a: number, b: number): Result<number> {
      if (b === 0) {
        return Err(new EthervoxError('Division by zero', 'MATH_ERROR'));
      }
      return Ok(a / b);
    }
    
    const success = divide(10, 2);
    expect(success.success).toBe(true);
    if (success.success) {
      expect(success.value).toBe(5);
    }
    
    const failure = divide(10, 0);
    expect(failure.success).toBe(false);
    if (!failure.success) {
      expect(failure.error.code).toBe('MATH_ERROR');
    }
  });
});