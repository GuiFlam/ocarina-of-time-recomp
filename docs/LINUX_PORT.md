# Linux port status

**50% of the stages**

| Stage | Done | Detail |
|---|---:|---|
| tools run on Linux | 100% | build_game.py finds clang, ninja, ar on PATH |
| runtime compiles | 98% | 89 of 91 runtime files |
| game C compiles | 100% | 551 of 551 generated files |
| links | 0% | build/v131072/oot not built |
| selftest passes | 0% | oot --selftest |
| title cinematic shows | 0% | a Linux run reaches the title |

Platform layer: linux.c defines 117 of the 119 functions platform.h declares (98%).
