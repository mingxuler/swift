//===--- UTF16.swift ------------------------------------------------------===//
//
// This source file is part of the Swift.org open source project
//
// Copyright (c) 2014 - 2017 Apple Inc. and the Swift project authors
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See https://swift.org/LICENSE.txt for license information
// See https://swift.org/CONTRIBUTORS.txt for the list of Swift project authors
//
//===----------------------------------------------------------------------===//
extension _Unicode {
  public enum UTF16 {
  case _swift3Buffer(_Unicode.UTF16.ForwardParser)
  }
}

extension _Unicode.UTF16 : UnicodeEncoding {
  public typealias CodeUnit = UInt16
  public typealias EncodedScalar = _UIntBuffer<UInt32, UInt16>

  public static var encodedReplacementCharacter : EncodedScalar {
    return EncodedScalar(_storage: 0xFFFD, _bitCount: 16)
  }

  public static func _isScalar(_ x: CodeUnit) -> Bool  {
    return x & 0xf800 != 0xd800
  }

  public static func decode(_ source: EncodedScalar) -> UnicodeScalar {
    let bits = source._storage
    if _fastPath(source._bitCount == 16) {
      return UnicodeScalar(_unchecked: bits & 0xffff)
    }
    _sanityCheck(source._bitCount == 32)
    let value = 0x10000 + (bits >> 16 & 0x03ff | (bits & 0x03ff) << 10)
    return UnicodeScalar(_unchecked: value)
  }

  public static func encodeIfRepresentable(
    _ source: UnicodeScalar
  ) -> EncodedScalar? {
    let x = source.value
    if _fastPath(x < (1 << 16)) {
      return EncodedScalar(_storage: x, _bitCount: 16)
    }
    let x1 = x - (1 << 16)
    var r = (0xdc00 + (x1 & 0x3ff))
    r &<<= 16
    r |= (0xd800 + (x1 &>> 10 & 0x3ff))
    return EncodedScalar(_storage: r, _bitCount: 32)
  }

  @inline(__always)
  public static func transcodeIfRepresentable<FromEncoding : UnicodeEncoding>(
    _ content: FromEncoding.EncodedScalar, from _: FromEncoding.Type
  ) -> EncodedScalar? {
    if _fastPath(FromEncoding.self == UTF8.self) {
      let c = unsafeBitCast(content, to: UTF8.EncodedScalar.self)
      var b = c._bitCount
      b = b &- 8
      if _fastPath(b == 0) {
        return EncodedScalar(
          _storage: c._storage & 0b0__111_1111, _bitCount: 16)
      }
      var s = c._storage
      var r = s
      r &<<= 6
      s &>>= 8
      r |= s & 0b0__11_1111
      b = b &- 8
      
      if _fastPath(b == 0) {
        return EncodedScalar(_storage: r & 0b0__111_1111_1111, _bitCount: 16)
      }
      r &<<= 6
      s &>>= 8
      r |= s & 0b0__11_1111
      b = b &- 8
      
      if _fastPath(b == 0) {
        return EncodedScalar(_storage: r & 0xFFFF, _bitCount: 16)
      }
      
      r &<<= 6
      s &>>= 8
      r |= s & 0b0__11_1111
      r &= (1 &<< 21) - 1
      return encodeIfRepresentable(UnicodeScalar(_unchecked: r))
    }
    else if _fastPath(FromEncoding.self == UTF16.self) {
      return unsafeBitCast(content, to: UTF16.EncodedScalar.self)
    }
    return encodeIfRepresentable(FromEncoding.decode(content))
  }
  
  public struct ForwardParser {
    public typealias _Buffer = _UIntBuffer<UInt32, UInt16>
    public init() { _buffer = _Buffer() }
    public var _buffer: _Buffer
  }
  
  public struct ReverseParser {
    public typealias _Buffer = _UIntBuffer<UInt32, UInt16>
    public init() { _buffer = _Buffer() }
    public var _buffer: _Buffer
  }
}

extension UTF16.ReverseParser : UnicodeParser, _UTFParser {
  public typealias Encoding = _Unicode.UTF16

  public func _parseMultipleCodeUnits() -> (isValid: Bool, bitCount: UInt8) {
    _sanityCheck(  // this case handled elsewhere
      !Encoding._isScalar(UInt16(extendingOrTruncating: _buffer._storage)))
    if _fastPath(_buffer._storage & 0xFC00_FC00 == 0xD800_DC00) {
      return (true, 2*16)
    }
    return (false, 1*16)
  }
  
  public func _bufferedScalar(bitCount: UInt8) -> Encoding.EncodedScalar {
    return Encoding.EncodedScalar(
      _storage:
        (_buffer._storage &<< 16 | _buffer._storage &>> 16) &>> (32 - bitCount),
      _bitCount: bitCount
    )
  }
}

extension _Unicode.UTF16.ForwardParser : UnicodeParser, _UTFParser {
  public typealias Encoding = _Unicode.UTF16
  
  public func _parseMultipleCodeUnits() -> (isValid: Bool, bitCount: UInt8) {
    _sanityCheck(  // this case handled elsewhere
      !Encoding._isScalar(UInt16(extendingOrTruncating: _buffer._storage)))
    if _fastPath(_buffer._storage & 0xFC00_FC00 == 0xDC00_D800) {
      return (true, 2*16)
    }
    return (false, 1*16)
  }
  
  public func _bufferedScalar(bitCount: UInt8) -> Encoding.EncodedScalar {
    var r = _buffer
    r._bitCount = bitCount
    return r
  }
}
