# Installation

### macOS

`brew install joedrago/repo/colorist`

Note: If it doesn't find a pre-compiled bottle, building from source takes a
while (10+ minutes). I'll try to keep bottles available for the most recent
macOS releases.

### Windows

Grab the latest executable from [Releases](https://github.com/joedrago/colorist/releases) and put it somewhere in your PATH.

### Build from source

Building from source requires [CMake](https://cmake.org/download/), version 3.5
or higher, and [NASM](https://nasm.us/).

Clone or download a zip of the repo, then run CMake on the root directory and
run the generated build.

To use codecs other than libaom (used with AVIF), you must enable the
associated `AVIF_CODEC_*` CMake variable, and to build locally, you must run
the appropriate .cmd file in `ext/avif/ext` and then additionally enable the
associated `AVIF_LOCAL_*` CMake variable.

For a near-automated Linux or macOS build, simply run `scripts/build.sh`. It
requires Rust, CMake, Ninja, NASM, Meson, and Git, but should build `dav1d`
and `rav1e` (for `libavif`), link them into colorist. Read the contents of
build.sh and the .cmd files inside of `ext/avif/ext` to see exactly what
commands will be run.

---

# Usage

Please see the [Usage](./docs/Usage.md) documentation and the
[Cookbook](./docs/Cookbook.md).

---

# Build Status

[![AppVeyor Build Status](https://ci.appveyor.com/api/projects/status/github/joedrago/colorist?branch=master&svg=true)](https://ci.appveyor.com/project/joedrago/colorist) [![Travis Build Status](https://travis-ci.com/joedrago/colorist.svg?branch=master)](https://travis-ci.com/joedrago/colorist)

---

# Overview & Explanation

Colorist is an image file and ICC profile converter, generator, and identifier.
Why make such a tool when the venerable
[ImageMagick](https://www.imagemagick.org/) already exists and seems to offer
every possible image processing tool you can imagine? The answer is __absolute
luminance__.

(Also, making tools is great fun.)

Since the dawn of computer rendering, luminance (brightness) has always been
*relative*.\*\* Values of 0 in a pixel have always meant "emit no light / as
little light as possible", and max values in a pixel (255 in 8-bit, etc) meant
"as bright as possible". We've gotten by just fine for a while with this
strategy, but times are changing. For example, the HDR10 standard
([BT.2100](https://en.wikipedia.org/wiki/Rec._2100)) and [Dolby
Vision](https://en.wikipedia.org/wiki/Dolby_Laboratories#Video_processing)
have defined a luminance range of 0-10,000 nits. We no longer can assume that
the author of an image containing max-channel white pixels intended to burn
your retinas out of your head. We need more information!

<sup><sub>\*\* *Hasn't it?*</sub></sup>

This means somewhere in the image file we must store our intended max luminance
such that renderers know how much to scale it when rendering (depending on the
output's max luminance). But where to store it? It turns out there is already a
place available in any image file format that can embed an ICC profile: an ICC
profile's **lumi** tag. The explanation in the ICC spec for the lumi tag is:

> This tag contains the absolute luminance of emissive devices in candelas per
> square metre as described by the Y channel.

Sounds perfect, no? Unfortunately, while ICC profile viewers and editors will
happily manipulate this tag and standard ICC profiles occasionally include the
tag for completeness, no image manipulation tool to date actually honors the
value during conversion or rendering. Until now!

**The goal of this tool** is to be a one-stop shop for manipulating/abusing ICC
profiles and image file formats (with respect to absolute luminance). By
leveraging the fantastic [LittleCMS](http://www.littlecms.com/) library,
choosing interesting tone curves and max luminance, and injecting my own scaling
and tonemapping steps into the pipeline, I hope to maintain as much of the
original image's fidelity when converting to other color profiles or file
formats that can't handle larger bit depth or are excessively lossy.

Any files created/generated via this tool will still be fully standards
compliant, it will simply have a slightly more *interesting* color profile
embedded that you can choose to parse in your own engines and scale that
luminance down accordingly. If the output of this tool isn't to your
satisfaction, ImageMagick is better in pretty much every other way. I highly
recommend it!

---

# License

Released under the Boost Software License (Version 1.0).
