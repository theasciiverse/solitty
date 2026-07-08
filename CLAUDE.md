# solitty

Ncurses-based Klondike solitaire, part of the Asciiverse catalog. Upstream project by Murilo Pereira,
packaged/maintained here at github.com/katalystnord/solitty.

## Building

```bash
make            # build the binary
sudo make install
make test       # unit tests
```

## Debian packaging

`dpkg-buildpackage` writes its output (`.deb`, `.changes`, `.buildinfo`, `.ddeb`) into the *parent*
directory of the source tree by default, which used to mean they landed loose in `code/` before this
repo moved to `Asciiverse/games/solitty/`. Use `./build-deb.sh` instead of calling
`dpkg-buildpackage` directly — it runs the build and moves the resulting packages into `dist/`
(gitignored) so they stay inside this game's own directory.

```bash
./build-deb.sh
```
