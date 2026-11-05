# Ocarina of Time PC Recomp

Research project: static recompilation of *The Legend of Zelda: Ocarina of Time* (title id `04004860224DEC00`) into a
native Windows program, by the method of [totk-pc-recomp](https://github.com/valentderah/totk-pc-recomp) as carried
to further games by [fe3h-pc-recomp](https://github.com/jaigner-hub/fe3h-pc-recomp) and
[metroid-prime-4-recomp](https://github.com/jaigner-hub/metroid-prime-4-recomp). The game's code is lifted to C,
compiled for x86-64 and linked with a host runtime that stands in for the console's OS and graphics API on Vulkan.

This is the fourth game on that code and the first Switch 2 title, so the project also tests how much of the shared
core survives a new console generation.

## Status

Phase 0, the survey: nothing runs yet. The tree is metroid-prime-4-recomp's, renamed; the game-bound tables in it are
still Metroid Prime 4's (see [docs/plan.md](docs/plan.md)).

This repository contains no game content. You need your own dump of the game and the keys from your own console. The
content check (`tools/check_content.py`, run by the pre-commit hook and CI) keeps it that way.

## License

MIT, see [LICENSE](LICENSE). Code from other projects is listed in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
