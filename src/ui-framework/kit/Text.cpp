#include "ui-framework/kit/Text.h"

namespace ui {

Catalog &Catalog::get() {
    static Catalog instance;
    return instance;
}

void Catalog::set(const std::string &key, const std::string &text) {
    entries_[key] = text;
    pseudoCache_.erase(key);
}

const std::string &Catalog::tr(const std::string &key) {
    const auto it = entries_.find(key);
    if (it == entries_.end()) {
        // Never show a key: fall back to a visible placeholder in development builds only.
        missing_ = "[" + key + "]";
        return missing_;
    }
    if (!pseudo_) return it->second;
    auto cached = pseudoCache_.find(key);
    if (cached == pseudoCache_.end()) cached = pseudoCache_.emplace(key, pseudoize(it->second)).first;
    return cached->second;
}

void Catalog::setPseudo(bool enabled) {
    pseudo_ = enabled;
}

std::string Catalog::pseudoize(const std::string &text) const {
    static const char *const accents[26] = {"á", "ß", "ç", "ð", "é", "ƒ", "ĝ", "ĥ", "í", "ĵ", "ķ", "ļ", "m",
                                            "ñ", "ö", "þ", "q", "ŕ", "š", "ţ", "ú", "v", "ŵ", "x", "ý", "ž"};
    std::string out = "[!! ";
    int letters = 0;
    bool inPlaceholder = false;   // {1} and the button placeholders {A} {START} ... must stay readable to the formatters
    for (char ch : text) {
        if (ch == '{') inPlaceholder = true;
        if (inPlaceholder) { out += ch; if (ch == '}') inPlaceholder = false; continue; }
        if (ch >= 'a' && ch <= 'z') { out += accents[ch - 'a']; ++letters; }
        else if (ch >= 'A' && ch <= 'Z') { out += accents[ch - 'A']; ++letters; }
        else out += ch;
    }
    for (int i = 0; i < letters * 2 / 5; i += 3) out += " ~~~";   // about 40 % longer, wrappable
    out += " !!]";
    return out;
}

namespace {
std::string fillArgs(const std::string &tmpl, const std::string *args, size_t count) {
    std::string out;
    out.reserve(tmpl.size() + 16);
    for (size_t i = 0; i < tmpl.size(); ++i) {
        if (tmpl[i] == '{' && i + 2 < tmpl.size() && tmpl[i + 1] >= '1' && tmpl[i + 1] <= '9' && tmpl[i + 2] == '}' &&
            size_t(tmpl[i + 1] - '1') < count) {
            out += args[tmpl[i + 1] - '1'];
            i += 2;
        } else {
            out += tmpl[i];
        }
    }
    return out;
}
}  // namespace

std::string fillTemplate(const std::string &tmpl, std::initializer_list<std::string> args) {
    return fillArgs(tmpl, args.begin(), args.size());
}

std::string format(const char *key, std::initializer_list<std::string> args) {
    return fillTemplate(Catalog::get().tr(key), args);
}

std::string plural(const char *key, long count, std::initializer_list<std::string> extra) {
    // English: exactly one is singular. A language file may later bring its own rule; the keys stay `.one` / `.other`.
    const std::string full = std::string(key) + (count == 1 ? ".one" : ".other");
    std::string args[9];
    args[0] = std::to_string(count);
    size_t n = 1;
    for (const std::string &e : extra) if (n < 9) args[n++] = e;
    return fillArgs(Catalog::get().tr(full), args, n);
}

std::string position(long at, long total, bool capped) {
    return format("format.position", {std::to_string(at), std::to_string(total) + (capped ? "+" : "")});
}

std::string joinParts(std::initializer_list<std::string> parts) {
    std::string out;
    for (const std::string &part : parts) {
        if (part.empty()) continue;
        if (!out.empty()) out += Catalog::get().tr("format.parts");
        out += part;
    }
    return out;
}

void registerFrameworkStrings() {
    Catalog &c = Catalog::get();
    // Hint vocabulary: one word per concept (see docs/components/Hints.md).
    c.set("hint.hold", "Hold");
    c.set("hint.move", "Move");
    c.set("hint.open", "Open");
    c.set("hint.choose", "Choose");
    c.set("hint.select", "Select");
    c.set("hint.back", "Back");
    c.set("hint.close", "Close");
    c.set("hint.cancel", "Cancel");
    c.set("hint.changeTab", "Change tab");
    c.set("hint.leave", "Leave");
    c.set("hint.change", "Change");
    c.set("hint.done", "Done");
    c.set("hint.confirm", "Confirm");
    c.set("modal.details", "Details");
    c.set("hint.browse", "Browse");
    c.set("hint.tryAgain", "Try again");
    c.set("hint.chooseKey", "Choose key");
    c.set("hint.type", "Type");
    c.set("hint.enter", "Enter");
    c.set("hint.search", "Search");
    c.set("hint.continue", "Continue");
    c.set("hint.start", "Start");
    c.set("hint.skip", "Skip");
    c.set("hint.later", "Later");
    c.set("hint.save", "Save");
    c.set("hint.scroll", "Scroll");
    // Wizard: result status words, the long-wait line and the entry page's status word.
    c.set("result.status.done", "Done");
    c.set("result.status.warning", "Warning");
    c.set("result.status.error", "Error");
    c.set("result.status.info", "Info");
    c.set("wizard.longWait", "Still working. This can take a few minutes. Nothing is stuck.");
    c.set("wizard.busy.elapsed", "Running for {1}");
    c.set("wizard.entry.check", "Check");
    // On-screen keyboard key labels.
    c.set("keyboard.shift", "Shift");
    c.set("keyboard.symbols", "Symbols");
    c.set("keyboard.letters", "ABC");
    c.set("keyboard.space", "Space");
    c.set("keyboard.delete", "Delete");
    c.set("keyboard.clear", "Clear");
    c.set("keyboard.show", "Show");
    c.set("keyboard.hide", "Hide");
    c.set("keyboard.done", "Done");
    c.set("hint.delete", "Delete");
    c.set("hint.space", "Space");
    c.set("format.position", "{1} of {2}");
    c.set("format.parts", " \xC2\xB7 ");
    c.set("switch.on", "On");
    c.set("switch.off", "Off");
}

}  // namespace ui
