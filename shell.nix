{
  pkgs ? import <nixpkgs> { },
}:
pkgs.mkShell {
  buildInputs = with pkgs.buildPackages; [
    fuse3
    lzo
    xz
    zlib
    bzip2
    zstd
  ];
  nativeBuildInputs = with pkgs; [
    pkg-config
    scons
  ];

  shellHook = ''
    export LDFLAGS="-g $(echo $NIX_LDFLAGS | sed 's/-rpath/-Wl,-rpath/g')"
    export CPPFLAGS="-g -O0 $NIX_CFLAGS_COMPILE"
  '';
}
