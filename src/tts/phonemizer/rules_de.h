/**
 * @file rules_de.h
 * @brief German grapheme-to-phoneme rules
 * 
 * Rule-based German G2P system leveraging German's regular orthography.
 * German spelling is highly predictable compared to English.
 * 
 * License: Part of EthervoxAI (CC BY-NC-SA 4.0)
 * No external dictionary dependencies - pure algorithmic approach.
 */

#ifndef RULES_DE_H
#define RULES_DE_H

#include "ethervox/error.h"
#include <stddef.h>

/**
 * Apply German grapheme-to-phoneme rules
 * 
 * @param word Input German word (UTF-8)
 * @param ipa_out Output IPA string
 * @param max_len Maximum output buffer size
 * @return ETHERVOX_SUCCESS on success, error code otherwise
 * 
 * Example:
 *   apply_german_g2p_rules("Haus", ipa_out, 256);
 *   // ipa_out: "haʊs"
 */
ethervox_result_t apply_german_g2p_rules(const char* word, char* ipa_out, size_t max_len);

#endif // RULES_DE_H
