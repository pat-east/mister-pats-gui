#pragma once

#include <initializer_list>
#include <string>
#include <unordered_map>

namespace ui {

// User-visible text is a catalog entry with a stable key (docs/I18N.md). English is the source
// language and is registered in the program; other languages replace entries. A key that is missing
// falls back to English; the UI never shows a key. Data (a title, a file name) is drawn as given.
class Catalog {
public:
    static Catalog &get();

    // Registers or replaces an entry (English source at startup, a language file later).
    void set(const std::string &key, const std::string &text);

    // The text for a key in the active language. Allocation-free after the first lookup.
    const std::string &tr(const std::string &key);

    // The pseudo-locale (en-XA): every letter accented and the text about 40 % longer, wrapped in
    // markers. It exposes literals, clipping and fixed widths at a glance.
    void setPseudo(bool enabled);
    bool pseudo() const { return pseudo_; }

private:
    std::string pseudoize(const std::string &text) const;

    std::unordered_map<std::string, std::string> entries_;
    std::unordered_map<std::string, std::string> pseudoCache_;
    std::string missing_;
    bool pseudo_ = false;
};

// What components accept for user-visible text: a catalog key, or data drawn as given.
class Text {
public:
    Text() = default;
    static Text key(const char *k) { Text t; t.key_ = k; return t; }
    static Text data(const std::string &s) { Text t; t.data_ = s; t.isData_ = true; return t; }

    const std::string &str() const { return isData_ ? data_ : Catalog::get().tr(key_); }
    bool empty() const { return isData_ ? data_.empty() : key_.empty(); }

private:
    std::string key_;
    std::string data_;
    bool isData_ = false;
};

// ---- Format: the one placeholder syntax of every catalog sentence (docs/I18N.md).
// A sentence marks where data goes with {1}, {2}, ... (1-based, up to {9}); a translation may reorder them.
// `{` that is not followed by a digit and `}` is left as written; a placeholder without an argument stays visible.
std::string fillTemplate(const std::string &tmpl, std::initializer_list<std::string> args);
// The catalog sentence for `key` with its placeholders filled: format("grid.info", {view, "3", "42"}).
std::string format(const char *key, std::initializer_list<std::string> args);
// A sentence whose wording depends on a number (English: one or other). The catalog holds `key.one` and
// `key.other`; the count itself is {1}, the extra arguments follow as {2}, {3}, ...
std::string plural(const char *key, long count, std::initializer_list<std::string> extra = {});
// "3 of 42"; with `capped` the total is shown as "42+" (the real total is not known). Catalog key `format.position`.
std::string position(long at, long total, bool capped = false);
// The non-empty parts joined with the separator ("Mega Drive · 1993 · 2 players"; catalog key `format.parts`, the
// middle dot with a space on each side). One separator for every line that lists facts.
std::string joinParts(std::initializer_list<std::string> parts);

// Registers the framework's own vocabulary (hints, status words, units) in English.
void registerFrameworkStrings();

}  // namespace ui
