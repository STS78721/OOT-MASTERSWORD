"""Windows Intel's compiler is entered by one thread, and a fault creating a shader module leaves that program out.

Adapted from Longshot 57f4eaec for MasterSword: checks IntelDriverGate (Windows Intel only, RX_VK_NO_INTEL_SERIAL=1
over it), guarded_shader_module, ask try-locks (including vk_mempack's stage_bind hot-cache module path on compute
dispatches), lock order with PcacheFetch, and all renderer compute/graphics helper files.

python -I -B tests/test_intel_compiler_gate.py
"""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def block(text, marker, start_at=0):
    start = text.index(marker, start_at)
    end = text.index("{", start) + 1
    depth = 1
    while depth:
        depth += (text[end] == "{") - (text[end] == "}")
        end += 1
    return text[start:end]


def brace_block(text, open_at):
    """text[open_at] is '{'; return through its match, including the braces."""
    end = open_at + 1
    depth = 1
    while depth:
        depth += (text[end] == "{") - (text[end] == "}")
        end += 1
    return text[open_at:end]


def need(cond, why):
    if not cond:
        raise SystemExit("FAIL: " + why)


def main():
    internal = (ROOT / "gpu/vk/vk_internal.h").read_text(encoding="utf-8")
    gate = block(internal, "struct IntelDriverGate {")
    on = block(internal, "inline bool intel_gate_on() {")
    need("#ifdef _WIN32" in on, "the lock is not limited to Windows")
    need('env_on("RX_VK_NO_INTEL_SERIAL")' in on, "RX_VK_NO_INTEL_SERIAL does not turn the lock off")
    need("dev.props.vendorID == 0x8086" in on, "a vendor other than Intel takes the lock")
    need("if (!intel_gate_on()) return;" in gate, "the lock is taken where it is off")
    need("std::try_to_lock" in gate and "busy = !hold.owns_lock();" in gate, "an ask waits for the lock")
    need("std::unique_lock<std::mutex>(mu)" in gate, "the compiler is not entered by one thread")

    draw = (ROOT / "gpu/vk/vk_draw.cpp").read_text(encoding="utf-8")
    win = block(draw, "__declspec(noinline) VkResult guarded_shader_module(")
    need("__try" in win and "vkCreateShaderModule" in win and "return (VkResult)-77;" in win,
         "a fault in vkCreateShaderModule is not turned into a left-out program")
    linux_at = draw.index("#else", draw.index("__declspec(noinline) VkResult guarded_shader_module("))
    linux = block(draw, "VkResult guarded_shader_module(", linux_at)
    need("vkCreateShaderModule" in linux, "non-Windows guarded_shader_module missing")

    for fn in ("VkResult cached_graphics(", "VkResult cached_compute("):
        body = block(draw, fn)
        need(body.index("IntelDriverGate") < body.index("create_cached"), fn + " compiles before it takes the lock")
        need("IntelDriverGate gate(compiles_nothing(ci->flags));" in body and
             "if (gate.busy) return intel_gate_busy();" in body, fn + " makes an ask wait for a compile")
    graphics = block(draw, "VkResult graphics_with(")
    need("return cached_graphics" in graphics, "the play-cache path takes the lock twice")
    need("if (gate.busy) return intel_gate_busy();" in graphics, "graphics_with makes an ask wait for a compile")

    busy = block(draw, "VkResult intel_gate_busy()")
    need("++st_intel_gate_busy" in busy and "logf(" in busy and "return VK_PIPELINE_COMPILE_REQUIRED;" in busy,
         "an ask answered cold for a busy gate is not counted")

    dispatch = draw[draw.index("bool on_worker = false;"):]
    dispatch = dispatch[:dispatch.index("jobs_push_serial")]
    need("info(p, asked_code, asked_bound, &on_worker)" in dispatch and "if (on_worker) intel_gate_busy();" in dispatch,
         "on Intel a dispatch's hot-cache module create waits for the gate on the replay thread or isn't counted")

    link = block(draw, "VkPipeline gpl_link(")
    need(link.index("IntelDriverGate") < link.index("guarded_graphics"), "gpl_link's pipeline is not guarded")
    need("vkCreateGraphicsPipelines" not in link, "gpl_link still calls the driver with no guard")

    for fn, at in (("void mask_stabilize(", 0),
                    ("bool movie_upscale(", 0),
                    ("static bool typed_upscale(", draw.rindex("static bool typed_upscale(", 0, draw.index("TAAU_UPSCALE_COMP_SPV")))):
        body = block(draw, fn, at)
        need("IntelDriverGate gate;" in body and "guarded_shader_module" in body and "guarded_compute" in body,
             fn + " is unguarded")
        need("vkCreateShaderModule" not in body and "vkCreateComputePipelines" not in body,
             fn + " still calls the driver raw")

    prep = block(draw, "bool prepare_run(")
    warm_at = prep.index("guarded_compute_held(pcache")
    need("IntelDriverGate" in prep[max(0, warm_at - 400):warm_at], "prepare_run warm_compute does not take the gate")

    for fn in ("size_t play_file_write(", "void pcache_init()", "void pipeline_cache_save(bool force)"):
        body = block(draw, fn)
        need("PcacheFetch" in body, fn + " fetches a cache's data outside the fetch lock")
        need("IntelDriverGate" not in body, fn + " takes Intel's gate around a fetch (the builders take it outside)")
    for fn in ("VkResult guarded_graphics_held(", "VkResult guarded_compute_held("):
        need("IntelDriverGate" not in block(draw, fn), fn + " takes Intel's gate inside the fetch lock")

    clear = block(draw, "VkPipeline clear_pipeline(")
    gate_at = clear.index("IntelDriverGate gate;")
    held = brace_block(clear, clear.rindex("{", 0, gate_at))
    need("cached_graphics(" not in held, "a masked clear holds the lock across cached_graphics (it takes the same lock)")
    need(clear.index(held) < clear.index("cached_graphics"), "the masked clear's modules are created after the pipeline")

    shader = (ROOT / "gpu/vk/vk_shader.cpp").read_text(encoding="utf-8")
    module = block(shader, "VkShaderModule make_module(")
    need("IntelDriverGate" in module and "extern_guarded_shader_module" in module, "make_module is unguarded")
    need("vkCreateShaderModule(" not in module, "make_module still calls vkCreateShaderModule raw")

    mempack = (ROOT / "gpu/vk/vk_mempack.cpp").read_text(encoding="utf-8")
    sbind = block(mempack, "bool stage_bind(")
    need("IntelDriverGate gate(busy != nullptr);" in sbind and "*busy = true;" in sbind and
         "extern_guarded_shader_module" in sbind, "stage_bind waits for the gate or skips the guard")
    need("vkCreateShaderModule(" not in mempack and "IntelDriverGate gate;" in mempack,
         "vk_mempack hot_warm is unguarded")

    temporal = (ROOT / "gpu/vk/vk_temporal.cpp").read_text(encoding="utf-8")
    pipe = block(temporal, "bool make_pipeline(")
    actor = block(temporal, "bool make_actor_pipeline()")
    need("extern_guarded_shader_module" in pipe and "extern_guarded_compute" in pipe,
         "a temporal compute pipeline is unguarded")
    need("extern_guarded_shader_module" in actor and "extern_guarded_graphics" in actor,
         "the actor-motion pipeline is unguarded")
    need("vkCreateShaderModule" not in pipe and "vkCreateComputePipelines" not in pipe,
         "make_pipeline still calls the driver raw")
    need("vkCreateGraphicsPipelines" not in actor, "make_actor_pipeline still calls the driver raw")

    for name in ("vk_texture", "vk_memory", "vk_reactive", "vk_fsr3", "vk_fsr3_inputs", "vk_fsr3fi", "vk_framegen",
                 "vk_coopmat", "vk_color", "vk_smaa", "vk_mvec"):
        src = (ROOT / "gpu/vk" / (name + ".cpp")).read_text(encoding="utf-8")
        need("vkCreateShaderModule(" not in src and "vkCreateComputePipelines(" not in src,
             name + ".cpp still calls the compiler with no guard")
        need("IntelDriverGate gate;" in src, name + ".cpp does not take the lock")

    print("Intel's compiler on Windows is entered by one thread, and shader module creation is guarded: ok")


if __name__ == "__main__":
    main()
