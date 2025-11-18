// SPDX-License-Identifier: CC-BY-NC-SA-4.0

/**
 * Base error class for EthervoxAI
 */
export class EthervoxError extends Error {
  public readonly code: string;
  public readonly timestamp: Date;
  public readonly context?: Record<string, unknown>;

  constructor(message: string, code: string, context?: Record<string, unknown>) {
    super(message);
    this.name = this.constructor.name;
    this.code = code;
    this.timestamp = new Date();
    this.context = context;
    Error.captureStackTrace(this, this.constructor);
  }

  toJSON() {
    return {
      name: this.name,
      message: this.message,
      code: this.code,
      timestamp: this.timestamp,
      context: this.context,
      stack: this.stack,
    };
  }
}

/**
 * Audio subsystem errors
 */
export class AudioError extends EthervoxError {
  constructor(message: string, context?: Record<string, unknown>) {
    super(message, 'AUDIO_ERROR', context);
  }
}

/**
 * Model loading and processing errors
 */
export class ModelError extends EthervoxError {
  constructor(message: string, context?: Record<string, unknown>) {
    super(message, 'MODEL_ERROR', context);
  }
}

/**
 * Platform and HAL errors
 */
export class PlatformError extends EthervoxError {
  constructor(message: string, context?: Record<string, unknown>) {
    super(message, 'PLATFORM_ERROR', context);
  }
}

/**
 * Network and API errors
 */
export class NetworkError extends EthervoxError {
  constructor(message: string, context?: Record<string, unknown>) {
    super(message, 'NETWORK_ERROR', context);
  }
}

/**
 * Plugin system errors
 */
export class PluginError extends EthervoxError {
  constructor(message: string, context?: Record<string, unknown>) {
    super(message, 'PLUGIN_ERROR', context);
  }
}

/**
 * Result type for operations that may fail
 */
export type Result<T, E = EthervoxError> = 
  | { success: true; value: T }
  | { success: false; error: E };

/**
 * Helper to create success result
 */
export function Ok<T>(value: T): Result<T, never> {
  return { success: true, value };
}

/**
 * Helper to create error result
 */
export function Err<E extends EthervoxError>(error: E): Result<never, E> {
  return { success: false, error };
}