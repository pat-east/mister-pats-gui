#!/usr/bin/env python3
import sys
import zlib
from pathlib import Path


def main():
    source = Path(sys.argv[1]).read_bytes()
    compressed = zlib.compress(source, 9)
    output = Path(sys.argv[2])
    with output.open("w", encoding="ascii", newline="\n") as target:
        target.write('#include "DiscIndexData.h"\n#include <zlib.h>\n\n')
        target.write("namespace {\nconst unsigned char kDiscIndexCompressed[] = {\n")
        for offset in range(0, len(compressed), 16):
            values = ", ".join(f"0x{value:02x}" for value in compressed[offset:offset + 16])
            target.write("    " + values + ",\n")
        target.write("};\n")
        target.write(f"const unsigned long kDiscIndexSize = {len(source)}UL;\n")
        target.write("}\n\n")
        target.write("bool loadEmbeddedDiscIndex(std::string &data) {\n")
        target.write("    data.resize(kDiscIndexSize);\n")
        target.write("    uLongf size = kDiscIndexSize;\n")
        target.write("    const int status = uncompress(reinterpret_cast<Bytef *>(&data[0]), &size,\n")
        target.write("        kDiscIndexCompressed, sizeof(kDiscIndexCompressed));\n")
        target.write("    if (status != Z_OK || size != kDiscIndexSize) { data.clear(); return false; }\n")
        target.write("    return true;\n}\n")


if __name__ == "__main__":
    main()
