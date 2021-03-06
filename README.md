# Etterna

Etterna is an advanced cross-platform rhythm game focused on keyboard play.

| Mac                                   | Linux-clang                           | Linux-gcc                             | Windows 7                         | Windows 10                        | Coverity                            |
| ------------------------------------- | ------------------------------------- | ------------------------------------- | --------------------------------- | --------------------------------- | ----------------------------------- |
| [![build1-trav][]][build-link-travis] | [![build2-trav][]][build-link-travis] | [![build3-trav][]][build-link-travis] | [![build1-app][]][build-link-app] | [![build2-app][]][build-link-app] | [![build1-cov][]][build-link-cover] |

[build1-trav]: https://travis-matrix-badges.herokuapp.com/repos/etternagame/etterna/branches/develop/1
[build2-trav]: https://travis-matrix-badges.herokuapp.com/repos/etternagame/etterna/branches/develop/2
[build3-trav]: https://travis-matrix-badges.herokuapp.com/repos/etternagame/etterna/branches/develop/4
[build1-app]: https://appveyor-matrix-badges.herokuapp.com/repos/Nickito12/etterna/branch/develop/1
[build2-app]: https://appveyor-matrix-badges.herokuapp.com/repos/Nickito12/etterna/branch/develop/2
[build1-cov]: https://img.shields.io/coverity/scan/12978.svg
[build-link-travis]: https://travis-ci.org/etternagame/etterna
[build-link-app]: https://ci.appveyor.com/project/Nickito12/etterna
[build-link-cover]: https://scan.coverity.com/projects/etternagame-etterna

![Discord](https://img.shields.io/discord/339597420239519755.svg)

![Github Releases (by Release)](https://img.shields.io/github/downloads/etternagame/etterna/v0.60.0/total.svg)

## Installation

### MacOS

To install on Mac we currently require a few things to be installed.

First step is to run this in your terminal, this will install Homebrew.
```bash
/usr/bin/ruby -e "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/master/install)"
```

The next step is to install the required libssl version using the following commands.
```bash
brew update && brew upgrade;
brew uninstall openssl;
brew install --force openssl@1.1;
sudo ln -s /usr/local/opt/openssl@1.1/lib/libcrypto.1.1.dylib /usr/local/lib/libcrypto.1.1.dylib;
sudo ln -s /usr/local/opt/openssl@1.1/lib/libssl.1.1.dylib /usr/local/lib/libssl.1.1.dylib;
```

The final step is to whitelist the Etterna directory, we cannot afford code signing and Apple forces it for the file system access we need to load NoteSkins and Songs.

```bash
sudo xattr -r -d com.apple.quarantine ~/your/path/to/Etterna
```

You path to Etterna is where ever you placed the folder inside of the DMG. If you copied it to your Desktop for example and renamed the folder, your path would be ``~/Desktop/Etterna``

### From Packages

For those that do not wish to compile the game on their own and use a binary right away, be aware of the following issues:

- Windows users are expected to have installed the [Microsoft Visual C++ x86 Redistributable for Visual Studio 2015](http://www.microsoft.com/en-us/download/details.aspx?id=48145) prior to running the game. For those on a 64-bit operating system, grab the x64 redistributable as well. [DirectX End-User Runtimes (June 2010)](http://www.microsoft.com/en-us/download/details.aspx?id=8109) is also required. Windows 7 is the minimum supported version.
- macOS users need to have macOS 10.6.8 or higher to run Etterna.
- Linux users should receive all they need from the package manager of their choice.

### From Source

https://etternagame.github.io/wiki/Building-Etterna.html

## Resources

- [Website](https://etternaonline.com/)
- [Discord](https://discord.gg/ZqpUjsJ)
- [Lua for Etterna](https://etternagame.github.io/Lua-For-Etterna/)
- [Lua API Reference](https://etternagame.github.io/Lua-For-Etterna/API/Lua.xml)
- [ETTP docs](https://github.com/Nickito12/NodeMultiEtt/blob/master/README.md)

## Licensing Terms

In short — you can do anything you like with the game (including sell products made with it), provided you _do not_ claim to have created the engine yourself or remove the credits.

For specific information/legalese:

- All of the our source code is under the [MIT license](http://opensource.org/licenses/MIT).
- The [MAD library](http://www.underbit.com/products/mad/) and [FFmpeg codecs](https://www.ffmpeg.org/) when built with our code use the [GPL license](http://www.gnu.org).

Etterna began as a fork of https://github.com/stepmania/stepmania

## Building

[On Building](Docs/Building.md)

## Building Documentation

[On Building Documentation](Docs/Building-Docs.md)

## Collaborating

[On Collaborating](Docs/Contributing.md)

## Bug Reporting

[On Bug Reporting](Docs/Bugreporting.md)
