/**
 * viet_undo.cpp — Undo and English-bypass logic for VietnameseEngine.
 *
 * Three related concerns that all involve "the user wants to cancel a
 * Telex transform and get raw ASCII back":
 *
 *   P2  processEnglishBypass  — fast path: skip VN processing for the
 *                               remainder of the current word after undo.
 *   P4  tryUndoTransform       — detect ooo→oo, ddd→dd, ww→w cancel patterns.
 *   P5  tryToneKeyUndo         — detect xx→raw-form tone cycle break.
 *
 * Each is a self-contained predicate+action pair extracted from processKey().
 */

#include "vietnamese.h"

#include <string>

#include "bamboo_ffi.h"
#include "viet_util.h"

namespace skey {

// ---------------------------------------------------------------------------
// Shared helpers
// ---------------------------------------------------------------------------

void VietnameseEngine::resetCompositionState() {
    // Clear rawInput_ and composed_, reset bamboo engine.
    // Deliberately does NOT clear committed_ — undo paths (P4, P5) append
    // to it; only reset() clears it.
    rawInput_.clear();
    composed_.clear();
    skey_engine_reset(handle_);
}

void VietnameseEngine::enterEnglishBypass() {
    englishBypass_ = true;
}

// ---------------------------------------------------------------------------
// P2 — English bypass
// ---------------------------------------------------------------------------

bool VietnameseEngine::processEnglishBypass() {
    // Edge case: after an undo set englishBypass_, subsequent keys in the
    // same word must pass through as raw ASCII without Vietnamese processing.
    // Trigger: englishBypass_ == true.
    // Must run AFTER rawInput_ += ch (the appended char is part of the word
    // being bypassed).
    // Reads/writes: englishBypass_, rawInput_, composed_.
    if (!englishBypass_) return false;
    composed_ = rawInput_;
    return true;  // caller returns ProcessResult::Consumed
}

// ---------------------------------------------------------------------------
// P4 — Undo detection (transform cancelled by repeat key)
// ---------------------------------------------------------------------------

bool VietnameseEngine::tryUndoTransform(
    char ch, const std::string &oldComposed, const std::string &oldRawInput) {
    // Detect undo: bamboo-core cancelled the transformation.
    // Before this key: oldComposed ≠ oldRawInput (e.g. "ư" ≠ "w", "đ" ≠ "dd").
    // After this key: composed_ is pure ASCII — no Vietnamese chars remain.
    //
    // Sub-cases for the commit decision:
    //   Multi-char transforms (oo→ô, dd→đ): trigger consumed.
    //     composed_ is shorter than rawInput_.  Commit ALL of composed_.
    //     ooo → "oo" (2<3) → commit "oo"
    //     ddd → "dd" (2<3) → commit "dd"
    //   Single-char transforms (w→ư when shortW/telex_w): trigger NOT consumed.
    //     composed_ equals rawInput_.  Strip last char (the trigger).
    //     ww → "ww" (2=2) → commit "w"
    //
    // The isRepeatKey guard distinguishes real undo from a manual
    // abbreviation followed by a different letter (e.g. "ađ" + 'r') —
    // in that case composed_ is all-ASCII only because bamboo never
    // produced the đ in the first place.
    //
    // Reads: rawInput_, composed_, handle_.
    // Writes: committed_ (appended), rawInput_, composed_, englishBypass_.
    // Bamboo limitation: commit-on-undo + word-level English bypass are
    // skey UI semantics — must stay in the wrapper.

    if (rawInput_.size() <= 1 || oldComposed == oldRawInput) return false;

    if (!detail::isAllAscii(composed_)) return false;

    bool isRepeatKey = !oldRawInput.empty() && ch == oldRawInput.back();
    if (!isRepeatKey) return false;
    // Not a repeat key: keep composed_ as-is.  The all-ASCII result is
    // because bamboo didn't transform "dd"→"đ" earlier, not because an
    // undo happened.

    if (composed_.size() < rawInput_.size()) {
        // Trigger consumed by bamboo — commit all
        committed_ += composed_;
    } else if (composed_.size() > 0) {
        // Trigger still present at end — strip it
        committed_ += composed_.substr(0, composed_.size() - 1);
    }

    resetCompositionState();
    enterEnglishBypass();

    return true;  // caller returns ProcessResult::Committed
}

// ---------------------------------------------------------------------------
// P5 — Double same-tone undo
// ---------------------------------------------------------------------------

bool VietnameseEngine::tryToneKeyUndo(
    char ch, const std::string &oldComposed, const std::string &oldRawInput) {
    // Double same-tone key = undo to clean raw form.
    // Edge case: bamboo-core treats a repeated tone key as a no-op
    // (ngã + x stays ngã), but users expect "xx" after a tone to reveal
    // the base characters + just one tone key.
    //
    // Trigger: ch is a tone key (s/f/r/x/j/z) AND oldComposed ≠ oldRawInput
    // AND oldComposed == composed_ (key changed nothing) AND
    // lowercase(ch) == lowercase(oldRawInput.back()).
    //
    // Action: build clean base by stripping every tone key AFTER the first
    // vowel from oldRawInput (tone keys before it are letters, e.g. 'x' in
    // "xin"), commit that base, reset engine with rawInput_ = single current
    // tone key, composed_ = that key.
    //
    // Does NOT set englishBypass_ (unlike P4) — the word-level bypass in P4
    // lasts the remainder of the word; the tone key reset here produces a
    // clean state for a fresh tone key.
    //
    // Writes: committed_ (appended), rawInput_, composed_, engine.
    // Bamboo limitation: without the engine reset + rawInput_ shrink, the
    // full old rawInput_ (with prior tone keys) is re-processed and the
    // tone re-appears — creating the unbreakable xìn→xinf→xìn cycle.
    // MUST stay in the wrapper.

    char cl = detail::toLowerASCII(ch);
    if (!detail::isToneKey(cl)) return false;
    if (oldComposed == oldRawInput || oldRawInput.empty()) return false;
    if (oldComposed != composed_) return false;

    char lastCl = detail::toLowerASCII(oldRawInput.back());
    if (cl != lastCl) return false;

    // Build clean raw form: strip all tone keys from oldRawInput,
    // keep base chars + append current tone key.
    size_t firstVowel = detail::findFirstVowel(oldRawInput);
    std::string base;
    for (size_t i = 0; i < oldRawInput.size(); ++i) {
        char c = oldRawInput[i];
        char lc = detail::toLowerASCII(c);
        bool isToneK = detail::isToneKey(lc);
        // Only strip tone keys that appear after the first vowel —
        // tone keys before the vowel are regular letters.
        if (isToneK &&
            firstVowel != std::string::npos &&
            static_cast<int>(i) > static_cast<int>(firstVowel))
            continue;
        base += c;
    }
    // Commit the clean base and reset engine so subsequent keys start
    // from the current tone key as a regular letter.
    committed_ = base;
    rawInput_ = std::string(1, ch);
    composed_ = rawInput_;
    skey_engine_reset(handle_);

    return true;  // caller returns ProcessResult::Committed
}

} // namespace skey
