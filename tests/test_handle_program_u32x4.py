"""BuildHandleProgram must extract words from LD.128 (CompositeExtractU32x4).

OoT lighting loads each light's texture/shadow handle with LD.128 from the light list
at c0[0x520], then CompositeExtractU32x4 picks one word. Without that case the builder
falls back to treating c0[0x520] itself as a handle (Lichii #94/#101, Crono on #696).
"""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "gpu" / "vk" / "eden" / "shader_recompiler" / "ir_opt" / "texture_pass.cpp"
text = SRC.read_text(encoding="utf-8", errors="replace")
if "case IR::Opcode::CompositeExtractU32x2:" not in text:
    raise SystemExit("CompositeExtractU32x2 case missing")
if "case IR::Opcode::CompositeExtractU32x4" not in text:
    raise SystemExit("no case IR::Opcode::CompositeExtractU32x4")
# Same load_word path as U32x2 (element index * 4 bytes)
needle = "return load_word(inst->Arg(0).InstRecursive(), element.U32() * 4);"
# Must appear after the U32x4 case label
idx4 = text.find("case IR::Opcode::CompositeExtractU32x4")
idx_ret = text.find(needle, idx4)
if idx_ret < 0 or idx_ret - idx4 > 800:
    raise SystemExit("CompositeExtractU32x4 does not route through load_word(... * 4)")
print("ok: BuildHandleProgram handles CompositeExtractU32x4")
