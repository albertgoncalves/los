with import <nixpkgs> {};
mkShell.override { stdenv = llvmPackages_19.stdenv; } {
    buildInputs = [
        aseprite
        glfw3
        libGL
        mold
    ];
    APPEND_LIBRARY_PATH = lib.makeLibraryPath [
        glfw3
        libGL
    ];
    shellHook = ''
        export LD_LIBRARY_PATH="$APPEND_LIBRARY_PATH:$LD_LIBRARY_PATH"
        . .shellhook
    '';
    hardeningDisable = [ "all" ];
}
