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

Generate the [SMPTE color bars](https://en.wikipedia.org/wiki/SMPTE_color_bars):

`colorist generate "640x320, #c0c0c0, #c0c000, #00c0c0, #00c000, #c000c0, #c00000, #0000c0 / 640x40, #0000c0, #000000, #c000c0, #000000, #00c0c0, #000000, #c0c0c0 / 640x120, #00214C, x3, #ffffff, x3, #32006A, x3, #131313, x3, #000000, #090909, #1D1D1D, #131313, x3" smpte.png`

Generate the [Macbeth chart](https://en.wikipedia.org/wiki/ColorChecker):

`colorist generate "1920x270,xyY(0.400, 0.350, 0.101), xyY(0.377, 0.345, 0.358), xyY(0.247, 0.251, 0.193), xyY(0.337, 0.422, 0.133), xyY(0.265, 0.240, 0.243), xyY(0.261, 0.343, 0.431) / xyY(0.506, 0.407, 0.301), xyY(0.211, 0.175, 0.120), xyY(0.453, 0.306, 0.198), xyY(0.285, 0.202, 0.066), xyY(0.380, 0.489, 0.443), xyY(0.473, 0.438, 0.431) / xyY(0.187, 0.129, 0.061), xyY(0.305, 0.478, 0.234), xyY(0.539, 0.313, 0.120), xyY(0.448, 0.470, 0.591), xyY(0.364, 0.233, 0.198), xyY(0.196, 0.252, 0.198) / xyY(0.310, 0.316, 0.900), xyY(0.310, 0.316, 0.591), xyY(0.310, 0.316, 0.362), xyY(0.310, 0.316, 0.198), xyY(0.310, 0.316, 0.090), xyY(0.310, 0.316, 0.031)" macbeth.png`

Many more image string examples are in the [Usage](./Usage.md) section.
