#include "vietnamese.h"

#include <cstdlib>
#include <cstring>

// bamboo-core FFI
#include "bamboo_ffi.h"

namespace skey {

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

VietnameseEngine::VietnameseEngine() {
    handle_ = bamboo_engine_new(BAMBOO_METHOD_TELEX);
}

VietnameseEngine::~VietnameseEngine() {
    if (handle_) {
        bamboo_engine_free(handle_);
        handle_ = nullptr;
    }
}

VietnameseEngine::VietnameseEngine(VietnameseEngine &&other) noexcept
    : handle_(other.handle_),
      method_(other.method_),
      toneStyle_(other.toneStyle_),
      freeMarking_(other.freeMarking_),
      autoRestore_(other.autoRestore_),
      shortW_(other.shortW_),
      bracketUO_(other.bracketUO_),
      rawInput_(std::move(other.rawInput_)),
      composed_(std::move(other.composed_)),
      englishBypass_(other.englishBypass_),
      committed_(std::move(other.committed_)) {
    other.handle_ = nullptr;
    other.englishBypass_ = false;
}

VietnameseEngine &VietnameseEngine::operator=(VietnameseEngine &&other) noexcept {
    if (this != &other) {
        if (handle_) {
            bamboo_engine_free(handle_);
        }
        handle_ = other.handle_;
        method_ = other.method_;
        toneStyle_ = other.toneStyle_;
        freeMarking_ = other.freeMarking_;
        autoRestore_ = other.autoRestore_;
        shortW_ = other.shortW_;
        bracketUO_ = other.bracketUO_;
        rawInput_ = std::move(other.rawInput_);
        composed_ = std::move(other.composed_);
        englishBypass_ = other.englishBypass_;
        committed_ = std::move(other.committed_);
        other.handle_ = nullptr;
        other.englishBypass_ = false;
    }
    return *this;
}

// ---------------------------------------------------------------------------
// Configuration
// ---------------------------------------------------------------------------

void VietnameseEngine::setMethod(InputMethod method) {
    method_ = method;
    int32_t m;
    if (method == InputMethod::VNI) {
        m = BAMBOO_METHOD_VNI;
    } else if (shortW_) {
        // Telex + "bare w → ư" option → bamboo telex_w variant
        m = BAMBOO_METHOD_TELEXW;
    } else {
        m = BAMBOO_METHOD_TELEX;
    }
    skey_engine_set_method(handle_, m);
}

void VietnameseEngine::setToneStyle(ToneStyle style) {
    toneStyle_ = style;
    // Modern = "hòa" (std_tone_style=true), Traditional = "hoà" (std_tone_style=false)
    skey_engine_set_std_tone_style(handle_, style == ToneStyle::Modern ? 1 : 0);
}

void VietnameseEngine::setFreeMarking(bool free) {
    freeMarking_ = free;
    // bamboo-core's free_tone_marking=true means "enable smart tone relocation"
    // (the engine auto-moves tone marks to standard position).
    // User's "Đánh dấu tự do" = true means "let me place tone freely" →
    // so we INVERT: user free=true → bamboo free_tone_marking=false.
    skey_engine_set_free_marking(handle_, free ? 0 : 1);
}

void VietnameseEngine::setAutoRestore(bool restore) {
    autoRestore_ = restore;
}

void VietnameseEngine::setShortW(bool enabled) {
    if (shortW_ == enabled) return;
    shortW_ = enabled;
    // Re-apply method so bamboo switches between telex() and telex_w().
    setMethod(method_);
}

void VietnameseEngine::setBracketUO(bool enabled) {
    bracketUO_ = enabled;
}

// ---------------------------------------------------------------------------
// Input processing
// ---------------------------------------------------------------------------

ProcessResult VietnameseEngine::processKey(char ch) {
    // Check if this is a letter that can start or continue composition
    bool isLetter = (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z');
    bool isDigit = (ch >= '0' && ch <= '9');

    // Telex "][→ươ" option: let '[' and ']' through as composition keys.
    // recompose() translates them to "ow"/"uw" before feeding bamboo.
    bool bracketActive = bracketUO_ && method_ == InputMethod::Telex &&
                         (ch == '[' || ch == ']');

    if (!isLetter && !bracketActive &&
        !(method_ == InputMethod::VNI && isDigit && !rawInput_.empty())) {
        return ProcessResult::Ignored;
    }

    std::string oldComposed = composed_;
    std::string oldRawInput = rawInput_;
    rawInput_ += ch;

    // English bypass: after an undo was detected, skip Vietnamese
    // processing for the remainder of the current word.  Just append
    // the raw character so the caller sees a simple ASCII append.
    if (englishBypass_) {
        composed_ = rawInput_;
        return ProcessResult::Consumed;
    }

    recompose();

    // Detect undo: bamboo-core cancelled the transformation.
    // Before adding this key, there was an active transformation
    // (oldComposed != oldRawInput, e.g. "ư" != "w", or "đ" != "dd").
    // After adding this key, the transformation is gone
    // (composed_ is pure ASCII — no Vietnamese chars remain).
    //
    // When undo is detected, whether the undo trigger key was consumed
    // depends on the transformation:
    //   - Multi-char transforms (oo→ô, dd→đ): trigger consumed.
    //     composed_ is shorter than rawInput_. Commit all.
    //     ooo → "oo" (2<3) → commit "oo"
    //     ddd → "dd" (2<3) → commit "dd"
    //   - Single-char transforms (w→ư when shortW/telex_w): trigger NOT consumed.
    //     composed_ equals rawInput_. Strip last char (the trigger).
    //     ww  → "ww" (2=2) → commit "w"
    if (rawInput_.size() > 1 && oldComposed != oldRawInput) {
        bool newIsAllAscii = true;
        for (unsigned char c : composed_) {
            if (c > 127) { newIsAllAscii = false; break; }
        }
        if (newIsAllAscii) {
            // Only treat as undo when the new key repeats the last raw
            // input character (e.g., "dd"→"đ" undone by third 'd').
            // Otherwise a manual dd→đ abbreviation followed by a
            // different letter (e.g., "ađ" + 'r') looks like an undo
            // but isn't one — composed_ is all-ASCII only because
            // bamboo never produced the đ in the first place.
            bool isRepeatKey = !oldRawInput.empty() && ch == oldRawInput.back();
            if (isRepeatKey) {
                if (composed_.size() < rawInput_.size()) {
                    // Trigger consumed by bamboo — commit all
                    committed_ += composed_;
                } else if (composed_.size() > 0) {
                    // Trigger still present at end — strip it
                    committed_ += composed_.substr(0, composed_.size() - 1);
                }

                // Clear composition entirely — undo key consumed
                rawInput_.clear();
                composed_.clear();
                skey_engine_reset(handle_);

                // Enter English bypass mode: subsequent keys in this word
                // will be forwarded as raw ASCII without Vietnamese processing.
                englishBypass_ = true;

                return ProcessResult::Committed;
            }
            // Not a repeat key: keep composed_ as-is.
            // The all-ASCII result is because bamboo didn't transform
            // "dd"→"đ" earlier, not because an undo happened.
        }
    }

    // Bamboo-core only transforms double-letter Telex patterns
    // (dd→đ, oo→ô, aa→â, etc.) at syllable start (position 0).
    // For all other positions, bamboo leaves the pair as-is.
    // Replace every untransformed pair with the correct Vietnamese
    // character whenever bamboo produced no Vietnamese chars at all
    // (composed_ == rawInput_).  This gives Unikey-like free typing.
    //
    // Undo patterns (ddd, ooo, etc.) are NOT handled here — when
    // composed_ ≠ rawInput_ (bamboo consumed a char for undo), we
    // skip this entire block.
    if (composed_ == rawInput_ && composed_.size() >= 2) {
        // Telex two-letter transforms: pair → UTF-8 result.
        // Only applied when composed_ == rawInput_ (bamboo produced
        // no Vietnamese chars at all, so these pairs were NOT
        // transformed at non-start positions).
        struct {
            char c0;       // first char (lowercase)
            char c1;       // second char (lowercase)
            const char *lo; // UTF-8 lowercase result
            const char *up; // UTF-8 uppercase result
        } static const kPairs[] = {
            {'d','d', "\xC4\x91","\xC4\x90"}, // đ/Đ
            {'o','o', "\xC3\xB4","\xC3\x94"}, // ô/Ô
            {'a','a', "\xC3\xA2","\xC3\x82"}, // â/Â
            {'e','e', "\xC3\xAA","\xC3\x8A"}, // ê/Ê
            {'w','w', "\xC6\xB0","\xC6\xAF"}, // ư/Ư
            {'a','w', "\xC4\x83","\xC4\x82"}, // ă/Ă
            {'o','w', "\xC6\xA1","\xC6\xA0"}, // ơ/Ơ
            {'u','w', "\xC6\xB0","\xC6\xAF"}, // ư/Ư
        };
        std::string fixed;
        for (size_t i = 0; i < composed_.size(); ) {
            bool replaced = false;
            if (i + 1 < composed_.size()) {
                char a = composed_[i];
                char b = composed_[i+1];
                // ToLower: 'A'-'Z' → 'a'-'z'
                auto toLower = [](char c) -> char {
                    return (c >= 'A' && c <= 'Z') ? static_cast<char>(c + 32) : c;
                };
                char al = toLower(a);
                char bl = toLower(b);
                bool firstUpper = (a >= 'A' && a <= 'Z');

                for (auto &p : kPairs) {
                    if (al == p.c0 && bl == p.c1) {
                        fixed += firstUpper ? p.up : p.lo;
                        i += 2;
                        replaced = true;
                        break;
                    }
                }
            }
            if (!replaced) {
                fixed += composed_[i];
                i++;
            }
        }
        composed_ = fixed;
    }

    return ProcessResult::Consumed;
}

void VietnameseEngine::backspace() {
    if (rawInput_.empty()) return;

    rawInput_.pop_back();
    if (rawInput_.empty()) {
        composed_.clear();
        skey_engine_reset(handle_);
    } else {
        recompose();
    }
}

void VietnameseEngine::reset() {
    rawInput_.clear();
    composed_.clear();
    committed_.clear();
    englishBypass_ = false;
    skey_engine_reset(handle_);
}

std::string VietnameseEngine::getComposed() const {
    return composed_;
}

// ---------------------------------------------------------------------------
// Internal
// ---------------------------------------------------------------------------

void VietnameseEngine::recompose() {
    // Telex "][→ươ": translate bracket keys to their Telex equivalents
    // ('[' → "ow" → ơ, ']' → "uw" → ư) before feeding bamboo. rawInput_
    // keeps the literal brackets so backspace/undo stay 1-char-per-key.
    const char *input = rawInput_.c_str();
    std::string translated;
    if (bracketUO_ && method_ == InputMethod::Telex &&
        (rawInput_.find('[') != std::string::npos ||
         rawInput_.find(']') != std::string::npos)) {
        translated.reserve(rawInput_.size() + 4);
        for (char c : rawInput_) {
            if (c == '[') translated += "ow";
            else if (c == ']') translated += "uw";
            else translated += c;
        }
        input = translated.c_str();
    }

    char *result = skey_engine_process_string(handle_, input);
    if (result) {
        composed_ = result;
        bamboo_free_string(result);
    } else {
        composed_ = rawInput_;
    }

    // Real-time auto-restore: only when bamboo produced a shorter or
    // different all-ASCII result — this happens when Telex modifier keys
    // (like "ss") destructively rewrite an English word.
    // E.g. "address" → bamboo yields "addres" (ss = undo tone).
    //
    // We intentionally do NOT restore composed_ that contains Vietnamese
    // characters (ô, â, ê, đ, ...) mid-word — those are valid Telex
    // transforms that the user intentionally typed.  Vietnamese vowels
    // and diacritics resulting from transforms like oo→ô, aa→â, dd→đ
    // should persist during typing, not be second-guessed.
    if (autoRestore_ &&
        composed_ != rawInput_ && !rawInput_.empty() &&
        skey_engine_is_valid(handle_) == 0) {

        bool composedAllAscii = true;
        for (unsigned char c : composed_) {
            if (c > 127) { composedAllAscii = false; break; }
        }
        if (composedAllAscii) {
            composed_ = rawInput_;
        }
    }
}

void VietnameseEngine::autoRestore() {
    if (!autoRestore_) return;
    if (rawInput_.empty() || composed_ == rawInput_) return;

    if (skey_engine_is_valid(handle_) == 0) {
        composed_ = rawInput_;
    }
}

} // namespace skey
