# runtime-patches

This game's own changes to the submodules, applied on top of N64Recomp-browser's patches, in order of name, by
`apply.cmake` here when `CMakeLists.txt` configures and when `tools/build_n64recomp.sh` runs. A new change is a new file per submodule, `<submodule>-<n>-<what>.patch`, numbered after
that submodule's highest, with a line here.

```
RecompFrontend-0001  RECOMPINPUT_PADS_IN_ORDER (CMakeLists.txt turns it on): until players are assigned, each
                     controller plays a port of its own, in the order they were connected, and the keyboard port 1,
                     where single-player mode gave every controller to every port. Rumble goes to each port's own
                     controller too. get_detected_controller_count says how many ports have one (main.cpp).
```
