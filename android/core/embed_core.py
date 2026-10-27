"""Generate embedded RGSS data sources from the pinned Lunetile copy."""
import json
import sys
from pathlib import Path


def bundle(root):
    source = root / "lunetile"
    schema = json.loads((source / "rpg_schema.json").read_text(encoding="utf-8"))
    definitions = "\n".join(
        f"RGSSXP.define_data_class({json.dumps(name)}, {json.dumps(fields)})"
        for name, fields in sorted(schema.items())
    )
    texts = [(source / name).read_text(encoding="utf-8") for name in ("types.rb", "data.rb", "data_constructors.rb", "text.rb")]
    return texts[0], texts[1] + "\n" + definitions + "\n" + texts[2], texts[3], (root / "saves.rb").read_text(encoding="utf-8"), (root / "scripts.rb").read_text(encoding="utf-8"), (root / "project.rb").read_text(encoding="utf-8"), (source / "integer_map.rb").read_text(encoding="utf-8")


def main():
    root = Path(__file__).resolve().parent
    blocks = bundle(root)
    out = []
    for name, text in zip(("value_types", "data_types", "text_support", "save_support", "script_support", "project_support", "integer_map_support"), blocks):
        if ')RGSSANDROID"' in text:
            raise ValueError("Ruby source conflicts with C++ raw-string delimiter")
        out.append(f'static const char {name}[] = R"RGSSANDROID(# encoding: UTF-8\n{text})RGSSANDROID";\n')
    Path(sys.argv[1]).write_text("\n".join(out), encoding="utf-8", newline="\n")


if __name__ == "__main__":
    main()
