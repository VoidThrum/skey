/**
 * viet_restore.cpp — Auto-restore logic for VietnameseEngine.
 *
 * Two restore paths share a common predicate:
 *
 *   R5  maybeAutoRestoreRealTime  – runs after every recompose() and
 *                                   backspace(); requires all-ASCII result
 *                                   before restoring (preserves intentional
 *                                   Telex transforms like oo→ô mid-word).
 *   --  autoRestore (public)      – commit-time one-shot restore without
 *                                   the all-ASCII check (currently unused
 *                                   in the production fcitx5 path).
 *
 * Both consult bamboo-core's is_valid() and the autoRestore_ flag.
 */

#include "vietnamese.h"

#include "bamboo_ffi.h"
#include "viet_util.h"

namespace skey {

// ---------------------------------------------------------------------------
// Shared predicate
// ---------------------------------------------------------------------------

bool VietnameseEngine::shouldRestoreToRaw(bool requireAllAscii) const {
    // Trigger: autoRestore_ is on, composed_ differs from rawInput_,
    // rawInput_ is non-empty, bamboo says the result is invalid.
    // When requireAllAscii is true (real-time path), the composed_ must
    // also be all-ASCII — this prevents restoring mid-word Vietnamese
    // transforms like oo→ô, aa→â, dd→đ which the user intentionally typed.
    // Reads: autoRestore_, rawInput_, composed_, handle_.
    if (!autoRestore_) return false;
    if (rawInput_.empty()) return false;
    if (composed_ == rawInput_) return false;
    if (skey_engine_is_valid(handle_) != 0) return false;
    if (requireAllAscii && !detail::isAllAscii(composed_)) return false;
    return true;
}

// ---------------------------------------------------------------------------
// R5 — Real-time auto-restore
// ---------------------------------------------------------------------------

void VietnameseEngine::maybeAutoRestoreRealTime() {
    // Edge case: Telex modifier keys destructively rewrite English words
    // (e.g. "address" → bamboo yields "addres" when ss = undo tone).
    //
    // Deliberately does NOT restore composed_ containing Vietnamese
    // characters (ô, â, ê, đ, ...) mid-word — those are valid Telex
    // transforms that the user intentionally typed.
    //
    // Reads: autoRestore_, rawInput_, composed_, handle_.
    // Writes: composed_ (restored to rawInput_ when condition met).
    // Bamboo limitation: bamboo-core has no "auto-restore non-Vietnamese"
    // mode — its restore_last_word() is manual.  Partial upstream candidate.
    if (shouldRestoreToRaw(/*requireAllAscii=*/true)) {
        composed_ = rawInput_;
    }
}

// ---------------------------------------------------------------------------
// Commit-time auto-restore (public API, currently unused in production)
// ---------------------------------------------------------------------------

void VietnameseEngine::autoRestore() {
    // Commit-time auto-restore without the all-ASCII check.
    // Trigger: autoRestore_ is on, composed_ differs from rawInput_,
    // and bamboo says the result is invalid.
    // Reads/writes: autoRestore_, rawInput_, composed_.
    if (shouldRestoreToRaw(/*requireAllAscii=*/false)) {
        composed_ = rawInput_;
    }
}

} // namespace skey
