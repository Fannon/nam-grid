# Fonts

`Roboto-Regular.ttf` is Roboto Regular version 1.100141 (2013), as distributed
with the iPlug2 examples (SHA-256
`b2efabca5ea4bc56eea829713706b5cd0788b82aca153bd4adde9b1573933b4f`), with its
TrueType hinting and glyph names removed to save space (SHA-256
`927cc6b76c5accbde22f74ae28d936b6e58254ca4648b27acca9e7d317bbc7aa`).

The editor renders text with NanoVG's stb_truetype, which ignores hinting, so the
trim changes no pixels. The trim deleted the `fpgm`, `prep`, `cvt ` and `LTSH` tables,
emptied each glyph's instructions, zeroed the `maxp` hinting limits and saved
`post` as format 3, using fontTools with bounding-box and timestamp recalculation
off. Every other table, outline and metric is unchanged.
Font data copyright Google 2012; Roboto is a trademark of Google. It is licensed
under the Apache License, Version 2.0 ([LICENSE-Roboto.txt](LICENSE-Roboto.txt)),
which is also reproduced in the shipped [third-party notices](../../../ThirdPartyNotices.txt).
No upstream Roboto distribution includes a NOTICE file.
