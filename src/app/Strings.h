#pragma once

#include "ui-framework/kit/Text.h"

namespace patsgui {

// The words more than one tab uses, registered once (src/app/Strings.cpp): the hint vocabulary, the view names, the
// "n of total" and search-result texts, the search placeholders, the removal questions and the picture
// placeholders. One word per concept: a tab never registers its own copy of one of these; it uses the key.
// A text that belongs to one tab only stays in that tab's strings function. Called first by registerStrings().
void registerSharedStrings(ui::Catalog &c);

}  // namespace patsgui
