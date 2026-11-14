/**
 * @file ios_avaudiosession.m
 * @brief AVAudioSession configuration shim for iOS voice capture/playback
 *
 * Copyright (c) 2024-2025 EthervoxAI Team
 * Proprietary and confidential. See LICENSE.
 *
 * ARCHITECTURE CHANGE (Granite Speech / multiplatform voice integration):
 * iOS has no equivalent of Android's AAudio input-preset mechanism for
 * requesting platform echo cancellation - instead, the *entire app process*
 * shares one AVAudioSession, configured via category/mode, which is
 * Objective-C-only API. This is the iOS analog of platform_android.c's
 * AAUDIO_INPUT_PRESET_VOICE_COMMUNICATION vs VOICE_RECOGNITION selection:
 *   - enable_echo_cancellation=true  -> AVAudioSessionModeVoiceChat
 *     (enables the platform's built-in AEC/NS/AGC pipeline, same one VoIP
 *     apps use, needed so the mic doesn't pick up this device's own TTS
 *     output during full-duplex barge-in - see plan.md Open Question 1)
 *   - enable_echo_cancellation=false -> AVAudioSessionModeMeasurement
 *     (flat, unprocessed capture - matches today's plain dictation-quality
 *     capture for Mode 2 / non-barge-in sessions)
 * Exposes a plain C function (matching the weather_http_ios.m precedent)
 * so the pure-C src/audio/platform_ios.c can call it without needing the
 * whole translation unit to be Objective-C.
 */

#import <AVFoundation/AVFoundation.h>
#import <Foundation/Foundation.h>

#include "ethervox/error.h"
#include "ethervox/config.h"

/**
 * @brief Configure the shared AVAudioSession for voice capture/playback.
 *
 * Must be called before starting an AudioQueue input or output stream.
 * Safe to call repeatedly (e.g. when switching between barge-in-enabled
 * Mode 1/4 sessions and non-barge-in Mode 2 dictation sessions).
 *
 * @param enable_echo_cancellation Non-zero to request platform AEC/NS/AGC
 *        (.voiceChat mode); zero for flat capture (.measurement mode).
 * @return ETHERVOX_SUCCESS on success, ETHERVOX_ERROR_AUDIO_INIT on failure.
 */
int ios_configure_audio_session_for_voice(int enable_echo_cancellation) {
    @autoreleasepool {
        AVAudioSession* session = [AVAudioSession sharedInstance];
        NSError* error = nil;

        AVAudioSessionMode mode = enable_echo_cancellation
            ? AVAudioSessionModeVoiceChat
            : AVAudioSessionModeMeasurement;

        // .playAndRecord is required for simultaneous mic capture + TTS
        // playback (Mode 1's full-duplex barge-in); .allowBluetooth and
        // .defaultToSpeaker keep behavior consistent with a typical VoIP-style
        // app (mirrors what AAUDIO_INPUT_PRESET_VOICE_COMMUNICATION implies
        // on Android - route audio in a way that assumes a voice conversation
        // rather than media playback).
        AVAudioSessionCategoryOptions options =
            AVAudioSessionCategoryOptionAllowBluetoothHFP |
            AVAudioSessionCategoryOptionDefaultToSpeaker;

        BOOL ok = [session setCategory:AVAudioSessionCategoryPlayAndRecord
                                   mode:mode
                                options:options
                                  error:&error];
        if (!ok || error) {
            ETHERVOX_LOGE("Failed to set AVAudioSession category/mode: %s",
                           error ? [[error localizedDescription] UTF8String] : "unknown error");
            return ETHERVOX_ERROR_AUDIO_INIT;
        }

        error = nil;
        ok = [session setActive:YES error:&error];
        if (!ok || error) {
            ETHERVOX_LOGE("Failed to activate AVAudioSession: %s",
                           error ? [[error localizedDescription] UTF8String] : "unknown error");
            return ETHERVOX_ERROR_AUDIO_INIT;
        }

        ETHERVOX_LOGI("AVAudioSession configured: mode=%s",
                       enable_echo_cancellation ? "voiceChat (AEC on)" : "measurement (flat)");
        return ETHERVOX_SUCCESS;
    }
}

/**
 * @brief Deactivate the shared AVAudioSession.
 *
 * Called when tearing down the last active capture/playback stream so the
 * app releases the audio session (allowing other apps' audio to resume,
 * matching AudioQueue teardown behavior on macOS).
 */
void ios_deactivate_audio_session(void) {
    @autoreleasepool {
        AVAudioSession* session = [AVAudioSession sharedInstance];
        NSError* error = nil;
        // NotifyOthersOnDeactivation lets other apps' paused audio resume,
        // consistent with a well-behaved iOS audio citizen.
        [session setActive:NO
                withOptions:AVAudioSessionSetActiveOptionNotifyOthersOnDeactivation
                      error:&error];
        if (error) {
            ETHERVOX_LOGW("Failed to deactivate AVAudioSession: %s",
                           [[error localizedDescription] UTF8String]);
        }
    }
}
