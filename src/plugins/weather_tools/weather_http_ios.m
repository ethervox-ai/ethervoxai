/**
 * @file weather_http_ios.m
 * @brief iOS-specific HTTP implementation for weather tools using NSURLSession
 *
 * Copyright (c) 2024-2025 EthervoxAI Team
 * Proprietary and confidential. See LICENSE.
 */

#include "ethervox/weather_tools.h"
#include "ethervox/logging.h"
#include "ethervox/error.h"

#import <Foundation/Foundation.h>

#include <stdlib.h>
#include <string.h>

/**
 * @brief Make HTTP GET request using iOS NSURLSession
 */
ethervox_result_t ios_http_get_request(
    const char* url,
    char** response_out,
    char** error_message_out
) {
    if (!url || !response_out) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }

    *response_out = NULL;
    if (error_message_out) {
        *error_message_out = NULL;
    }

    @autoreleasepool {
        // Create URL
        NSString *urlString = [NSString stringWithUTF8String:url];
        NSURL *nsurl = [NSURL URLWithString:urlString];
        
        if (!nsurl) {
            NSLog(@"[WeatherHTTP] Invalid URL: %s", url);
            if (error_message_out) {
                *error_message_out = strdup("Invalid URL");
            }
            return ETHERVOX_ERROR_INVALID_ARGUMENT;
        }

        // Create request
        NSURLRequest *request = [NSURLRequest requestWithURL:nsurl
                                                  cachePolicy:NSURLRequestReloadIgnoringLocalCacheData
                                              timeoutInterval:30.0];

        // Synchronous request using semaphore
        dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);
        __block NSData *responseData = nil;
        __block NSError *requestError = nil;
        __block NSInteger statusCode = 0;

        NSURLSessionDataTask *task = [[NSURLSession sharedSession] dataTaskWithRequest:request
            completionHandler:^(NSData *data, NSURLResponse *response, NSError *error) {
                responseData = data;
                requestError = error;
                
                if ([response isKindOfClass:[NSHTTPURLResponse class]]) {
                    statusCode = [(NSHTTPURLResponse *)response statusCode];
                }
                
                dispatch_semaphore_signal(semaphore);
            }];

        [task resume];
        
        // Wait for completion (timeout after 30 seconds)
        long timeout = dispatch_semaphore_wait(semaphore, dispatch_time(DISPATCH_TIME_NOW, 30 * NSEC_PER_SEC));
        
        if (timeout != 0) {
            NSLog(@"[WeatherHTTP] Request timed out");
            if (error_message_out) {
                *error_message_out = strdup("Request timed out");
            }
            return ETHERVOX_ERROR_TIMEOUT;
        }

        // Check for errors
        if (requestError) {
            NSString *errorDesc = [requestError localizedDescription];
            NSLog(@"[WeatherHTTP] Request failed: %@", errorDesc);
            if (error_message_out) {
                *error_message_out = strdup([errorDesc UTF8String]);
            }
            return ETHERVOX_ERROR_NETWORK;
        }

        // Check status code
        if (statusCode < 200 || statusCode >= 300) {
            NSLog(@"[WeatherHTTP] HTTP error: %ld", (long)statusCode);
            if (error_message_out) {
                NSString *errMsg = [NSString stringWithFormat:@"HTTP %ld", (long)statusCode];
                *error_message_out = strdup([errMsg UTF8String]);
            }
            return ETHERVOX_ERROR_NETWORK;
        }

        // Convert response data to C string
        if (responseData && responseData.length > 0) {
            NSString *responseString = [[NSString alloc] initWithData:responseData encoding:NSUTF8StringEncoding];
            if (responseString) {
                *response_out = strdup([responseString UTF8String]);
                NSLog(@"[WeatherHTTP] Success: %lu bytes", (unsigned long)responseData.length);
                return ETHERVOX_SUCCESS;
            } else {
                NSLog(@"[WeatherHTTP] Failed to decode response as UTF-8");
                if (error_message_out) {
                    *error_message_out = strdup("Invalid UTF-8 response");
                }
                return ETHERVOX_ERROR_INVALID_ARGUMENT;
            }
        } else {
            NSLog(@"[WeatherHTTP] Empty response");
            if (error_message_out) {
                *error_message_out = strdup("Empty response");
            }
            return ETHERVOX_ERROR_NETWORK;
        }
    }
}
