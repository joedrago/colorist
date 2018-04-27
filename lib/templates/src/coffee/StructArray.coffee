pako = require 'pako'

utils = require './utils'

formatToSize = (format) ->
  switch format
    when 'i8', 'u8'
      1
    when 'i16', 'u16'
      2
    when 'i32', 'u32', 'f32'
      4

class StructArray
  constructor: (@payload) ->
    @buffer = Buffer.from(pako.inflate(Buffer.from(@payload.data, 'base64')))
    @elementSize = 0
    for s in @payload.schema
      @elementSize += formatToSize(s.format)
    console.log "@elementSize #{@elementSize}"

  get: (x = 0, y = 0) ->
    e = {}
    x = utils.clamp(x, 0, @payload.width)
    y = utils.clamp(y, 0, @payload.height)
    elementOffset = @elementSize * (x + (y * @payload.width))
    for s in @payload.schema
      switch s.format
        when 'i8'
          e[s.name] = @buffer.readInt8(elementOffset)
        when 'u8'
          e[s.name] = @buffer.readUInt8(elementOffset)
        when 'i16'
          e[s.name] = @buffer.readInt16LE(elementOffset)
        when 'u16'
          e[s.name] = @buffer.readUInt16LE(elementOffset)
        when 'i32'
          e[s.name] = @buffer.readInt32LE(elementOffset)
        when 'u32'
          e[s.name] = @buffer.readUInt32LE(elementOffset)
        when 'f32'
          e[s.name] = @buffer.readFloatLE(elementOffset)
      elementOffset += formatToSize(s.format)
    return e

module.exports = StructArray
