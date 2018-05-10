# Cookbook

Please see the [Usage](./Usage.md) documentation for basic usage.

This is a living document / work in progress!

### Identify

Basic identify usage:

`colorist identify image.png`

Dumping the pixel values of a 2x2 rect at (100,1000):

`colorist identify image.png -z 100,1000,2,2`


### Convert

Basic conversion, leaving the source's ICC profile completely intact:

`colorist convert src.png dst.png`

Autograding an image's max luminance and gamma curve:

`colorist convert -a src.png dst.png`

Converting an image to DCI P3 at 300 nits:

`colorist convert -p p3 -g 2.4 -l 300 src.png dst.png`

Converting an image to BT. 2020 (HDR10's color primaries), with 10,000 nits luminance and a basic gamma curve:

`colorist convert -p bt2020 -g 2.4 -l 10000 src.png dst.png`

Compressing to JPEG2000 with a rate of 100x:

`colorist convert -r 100 src.png dst.jp2`

Extract an ICC profile from an image:

`colorist convert src.png dst.icc`

Convert an image to a made-up ICC profile (also generated below):

`colorist convert src.png dst.png -p bt709 -l 300 -g 2.4 -d "My sRGB Profile" -c "(c) Me 2018"`


### Generate

Generate an sRGB ICC profile with a max luminance of 300 nits and a custom description & copyright:

`colorist generate mysrgb.icc -p bt709 -l 300 -g 2.4 -d "My sRGB Profile" -c "(c) Me 2018"`

Generate a 4K black to red 8-bit gradient (left to right):

`colorist generate -b 8 "3840x2160,(0,0,0)..(255,0,0)" gradient.png`

Generate a 4K black to red 8-bit gradient (top to bottom):

`colorist generate -b 8 "2160x3840,(0,0,0)..(255,0,0),cw" gradient.png`

Generate a 4K black to red 16-bit gradient (top to bottom):

`colorist generate "2160x3840,(0,0,0)..(255,0,0),cw" gradient.png`

Many more image string examples are in the [Usage](./Usage.md) section.
