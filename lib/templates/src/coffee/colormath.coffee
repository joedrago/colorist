# ---------------------------------------------------------------------------
#                         Copyright Joe Drago 2018.
#         Distributed under the Boost Software License, Version 1.0.
#            (See accompanying file LICENSE_1_0.txt or copy at
#                  http://www.boost.org/LICENSE_1_0.txt)
# ---------------------------------------------------------------------------

# ---------------------------------------------------------------------------
# Generic vector math

vecDiv = (v, s) ->
  return [
    v[0] / s
    v[1] / s
    v[2] / s
  ]

vecScaleK = (v, k) ->
  return [
    v[0] * k
    v[1] * k
    v[2] * k
  ]

vecMul = (v1, v2) ->
  return [
    v1[0] * v2[0]
    v1[1] * v2[1]
    v1[2] * v2[2]
  ]

vecPow = (v, e) ->
  return [
    Math.pow(v[0], e)
    Math.pow(v[1], e)
    Math.pow(v[2], e)
  ]

vecSum = (v) ->
  return v[0] + v[1] + v[2]

# ---------------------------------------------------------------------------
# Generic matrix math

matTranspose = (m) ->
  return [
    [ m[0][0], m[1][0], m[2][0] ]
    [ m[0][1], m[1][1], m[2][1] ]
    [ m[0][2], m[1][2], m[2][2] ]
  ]

matDet = (m) ->
  det = m[0][0] * (m[2][2] * m[1][1] - m[2][1] * m[1][2]) -
        m[1][0] * (m[2][2] * m[0][1] - m[2][1] * m[0][2]) +
        m[2][0] * (m[1][2] * m[0][1] - m[1][1] * m[0][2])
  return det

matInvert = (m) ->
  scale = 1.0 / matDet(m)

  i = [
    [0,0,0]
    [0,0,0]
    [0,0,0]
  ]

  i[0][0] =  scale * (m[2][2] * m[1][1] - m[2][1] * m[1][2])
  i[0][1] = -scale * (m[2][2] * m[0][1] - m[2][1] * m[0][2])
  i[0][2] =  scale * (m[1][2] * m[0][1] - m[1][1] * m[0][2])

  i[1][0] = -scale * (m[2][2] * m[1][0] - m[2][0] * m[1][2])
  i[1][1] =  scale * (m[2][2] * m[0][0] - m[2][0] * m[0][2])
  i[1][2] = -scale * (m[1][2] * m[0][0] - m[1][0] * m[0][2])

  i[2][0] =  scale * (m[2][1] * m[1][0] - m[2][0] * m[1][1])
  i[2][1] = -scale * (m[2][1] * m[0][0] - m[2][0] * m[0][1])
  i[2][2] =  scale * (m[1][1] * m[0][0] - m[1][0] * m[0][1])

  return i

matMul = (m1, m2) ->
  m = [
    [ 0, 0, 0 ]
    [ 0, 0, 0 ]
    [ 0, 0, 0 ]
  ]

  for i in [0..2]
    for j in [0..2]
      for k in [0..2]
        m[i][j] += m1[i][k] * m2[k][j]

  return m

matEval = (m, v) ->
  return [
    (m[0][0] * v[0]) + (m[0][1] * v[1]) + (m[0][2] * v[2])
    (m[1][0] * v[0]) + (m[1][1] * v[1]) + (m[1][2] * v[2])
    (m[2][0] * v[0]) + (m[2][1] * v[1]) + (m[2][2] * v[2])
  ]

# ---------------------------------------------------------------------------
# Color specific math

rawToFloat = (rgba, depth) ->
  maxChannel = (1 << depth) - 1
  return [
    rgba[0] / maxChannel
    rgba[1] / maxChannel
    rgba[2] / maxChannel
    rgba[3] / maxChannel
  ]

convertXYYtoXYZ = (xyy) ->
  if !xyy[2]
    return [0, 0, 0]
  return [
    (xyy[0] * xyy[2]) / xyy[1],
    xyy[2],
    ((1 - xyy[0] - xyy[1]) * xyy[2]) / xyy[1]
  ]

convertXYZtoXYY = (xyz, whitePoint) ->
  sum = vecSum(xyz)
  if !sum
    return [whitePoint[0], whitePoint[1], 0]
  return [
    xyz[0] / sum,
    xyz[1] / sum,
    xyz[1]
  ]

whitePointToXYZ = (xy) ->
  return convertXYYtoXYZ([ xy[0], xy[1], 1 ])

matDeriveRGBToXYZ = (primaries) -> # [rx,ry,gx,gy,bx,by,wx,wy]
  xr = primaries[0]
  yr = primaries[1]
  zr = 1 - xr - yr
  xg = primaries[2]
  yg = primaries[3]
  zg = 1 - xg - yg
  xb = primaries[4]
  yb = primaries[5]
  zb = 1 - xb - yb
  xw = primaries[6]
  yw = primaries[7]
  zw = 1 - xw - yw

  m = [
    [ xr / yr, xg / yg, xb / yb ]
    [ 1, 1, 1 ]
    [ zr / yr, zg / yg, zb / yb ]
  ]

  mi = matInvert(m)

  # derive xyz white point
  wxyz = vecDiv([xw, yw, zw], yw)

  s = [
    vecSum(vecMul(mi[0], wxyz))
    vecSum(vecMul(mi[1], wxyz))
    vecSum(vecMul(mi[2], wxyz))
  ]

  mxyz = [
    [ s[0] * m[0][0], s[1] * m[0][1], s[2] * m[0][2] ]
    [ s[0] * m[1][0], s[1] * m[1][1], s[2] * m[1][2] ]
    [ s[0] * m[2][0], s[1] * m[2][1], s[2] * m[2][2] ]
  ]
  return mxyz

matDeriveRGBToRGB = (srcPrimaries, dstPrimaries) ->
  srcToXYZ = matDeriveRGBToXYZ(srcPrimaries)
  dstToXYZ = matDeriveRGBToXYZ(dstPrimaries)
  xyzToDst = matInvert(dstToXYZ)
  return matMul(xyzToDst, srcToXYZ)

# ---------------------------------------------------------------------------

module.exports =
  vecDiv: vecDiv
  vecScaleK: vecScaleK
  vecMul: vecMul
  vecPow: vecPow
  vecSum: vecSum

  matTranspose: matTranspose
  matDet: matDet
  matInvert: matInvert
  matMul: matMul
  matEval: matEval

  rawToFloat: rawToFloat
  convertXYYtoXYZ: convertXYYtoXYZ
  convertXYZtoXYY: convertXYZtoXYY
  whitePointToXYZ: whitePointToXYZ
  matDeriveRGBToXYZ: matDeriveRGBToXYZ
  matDeriveRGBToRGB: matDeriveRGBToRGB
