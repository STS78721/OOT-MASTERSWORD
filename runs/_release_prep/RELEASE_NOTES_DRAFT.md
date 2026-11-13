# Hookshot (personal build) — first release draft

**Based on** NotRexed/Hookshot v2.0.5 + local graphics/OoT fixes  
**Requires** your own Switch dump (no game files in this release)  
**Status** experimental Windows build

## Highlights
- Insert menu: Graphics, Upscaling, Frame Generation, Colour
- SMAA anti-aliasing and colour grading in the present path
- Native Hookshot frame generation; Nodix/vendor FG rows listed (stand-ins where runtimes are not wired)
- Foliage: stem marker fix (#759), sword-swing vanish fix (#957), mesh/SSBO neon hardening
- Hyrule Field distant ground lines (#956 BC3 one-layer)
- Safer compute shader seeding on RADV (#894)

## Known limits
- Not an official NotRexed release
- Some upscaler/FG menu options are placeholders
- Output-resolution menu row (#849) not included in this tree yet
- Neon foliage may still show residual artefacts on some GPUs

## Credits
Upstream Hookshot / Eden shader stack; SMAA (iryoku); ports from Hookshot PRs #759 #894 #956 #957 and related work.
