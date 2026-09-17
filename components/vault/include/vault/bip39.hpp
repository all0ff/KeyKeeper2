#pragma once

#include <cstddef>
#include <string>
#include <vector>

// =============================================================================
// vault::bip39 -- validation against the official BIP-39 English
// wordlist, for vault::VaultEntry::seed_phrase (a crypto wallet's
// mnemonic recovery phrase -- see vault_model.hpp's own comment on
// that field for why it's a SEPARATE thing from recovery_codes).
//
// SCOPE, DELIBERATELY LIMITED: this checks that the phrase has one of
// BIP-39's defined lengths (12/15/18/21/24 words) and that every word
// is actually in the wordlist. It does NOT verify the BIP-39
// checksum (the last word's low bits are a SHA-256-derived checksum
// over the preceding entropy, per BIP-39 section "Generating the
// mnemonic") -- a phrase can pass everything here and still not be a
// checksum-valid, genuinely derivable mnemonic (e.g. words in the
// wrong order, or one word swapped for another valid word). Checking
// that would mean deriving entropy bits from the word indices and
// computing a SHA-256 over them -- a real, separate piece of work,
// not implemented here. What this DOES catch: typos, autocomplete
// mistakes, and words from a different wordlist entirely -- the
// overwhelming majority of real data-entry mistakes.
// =============================================================================

namespace vault::bip39 {

/// Case-sensitive, exact match against the wordlist (BIP-39 words are
/// themselves all lowercase -- the caller, not this function, is
/// responsible for lowercasing user input first if that's wanted).
bool is_valid_word(const std::string& word);

/// Always 2048 -- exposed mainly so callers/tests don't need to
/// hardcode that number themselves.
size_t wordlist_size();

/// One of BIP-39's five defined lengths.
bool is_valid_word_count(size_t count);

/**
 * @brief Full validation: word count is one of BIP-39's five defined
 *        lengths (12/15/18/21/24) AND every word is in the wordlist.
 *        See this file's own comment for what this does NOT check
 *        (the checksum).
 */
bool validate_seed_phrase(const std::vector<std::string>& words);

} // namespace vault::bip39
