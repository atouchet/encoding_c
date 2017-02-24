// Copyright 2015-2016 Mozilla Foundation. See the COPYRIGHT
// file at the top-level directory of this distribution.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

#pragma once

#ifndef encoding_rs_cpp_h_
#define encoding_rs_cpp_h_

#include <string>
#include <tuple>
#include <memory>
#include <experimental/optional>
#include "gsl/gsl"

#include "encoding_rs.h"

class Encoding;

/**
 * A converter that decodes a byte stream into Unicode according to a
 * character encoding in a streaming (incremental) manner.
 *
 * The various `decode_*` methods take an input buffer (`src`) and an output
 * buffer `dst` both of which are caller-allocated. There are variants for
 * both UTF-8 and UTF-16 output buffers.
 *
 * A `decode_*` method decodes bytes from `src` into Unicode characters stored
 * into `dst` until one of the following three things happens:
 *
 * 1. A malformed byte sequence is encountered (`*_without_replacement`
 *    variants only).
 *
 * 2. The output buffer has been filled so near capacity that the decoder
 *    cannot be sure that processing an additional byte of input wouldn't
 *    cause so much output that the output buffer would overflow.
 *
 * 3. All the input bytes have been processed.
 *
 * The `decode_*` method then returns tuple of a status indicating which one
 * of the three reasons to return happened, how many input bytes were read,
 * how many output code units (`u8` when decoding into UTF-8 and `u16`
 * when decoding to UTF-16) were written (except when decoding into `String`,
 * whose length change indicates this), and in the case of the
 * variants performing replacement, a boolean indicating whether an error was
 * replaced with the REPLACEMENT CHARACTER during the call.
 *
 * The number of bytes "written" is what's logically written. Garbage may be
 * written in the output buffer beyond the point logically written to.
 * Therefore, if you wish to decode into an `&mut str`, you should use the
 * methods that take an `&mut str` argument instead of the ones that take an
 * `&mut [u8]` argument. The former take care of overwriting the trailing
 * garbage to ensure the UTF-8 validitiy of the `&mut str` as a whole, but the
 * latter don't.
 *
 * In the case of the `*_without_replacement` variants, the status is a
 * [`DecoderResult`][1] enumeration (possibilities `Malformed`, `OutputFull` and
 * `InputEmpty` corresponding to the three cases listed above).
 *
 * In the case of methods whose name does not end with
 * `*_without_replacement`, malformed sequences are automatically replaced
 * with the REPLACEMENT CHARACTER and errors do not cause the methods to
 * return early.
 *
 * When decoding to UTF-8, the output buffer must have at least 4 bytes of
 * space. When decoding to UTF-16, the output buffer must have at least two
 * UTF-16 code units (`u16`) of space.
 *
 * When decoding to UTF-8 without replacement, the methods are guaranteed
 * not to return indicating that more output space is needed if the length
 * of the output buffer is at least the length returned by
 * [`max_utf8_buffer_length_without_replacement()`][2]. When decoding to UTF-8
 * with replacement, the length of the output buffer that guarantees the
 * methods not to return indicating that more output space is needed is given
 * by [`max_utf8_buffer_length()`][3]. When decoding to UTF-16 with
 * or without replacement, the length of the output buffer that guarantees
 * the methods not to return indicating that more output space is needed is
 * given by [`max_utf16_buffer_length()`][4].
 *
 * The output written into `dst` is guaranteed to be valid UTF-8 or UTF-16,
 * and the output after each `decode_*` call is guaranteed to consist of
 * complete characters. (I.e. the code unit sequence for the last character is
 * guaranteed not to be split across output buffers.)
 *
 * The boolean argument `last` indicates that the end of the stream is reached
 * when all the bytes in `src` have been consumed.
 *
 * A `Decoder` object can be used to incrementally decode a byte stream.
 *
 * During the processing of a single stream, the caller must call `decode_*`
 * zero or more times with `last` set to `false` and then call `decode_*` at
 * least once with `last` set to `true`. If `decode_*` returns `InputEmpty`,
 * the processing of the stream has ended. Otherwise, the caller must call
 * `decode_*` again with `last` set to `true` (or treat a `Malformed` result as
 *  a fatal error).
 *
 * Once the stream has ended, the `Decoder` object must not be used anymore.
 * That is, you need to create another one to process another stream.
 *
 * When the decoder returns `OutputFull` or the decoder returns `Malformed` and
 * the caller does not wish to treat it as a fatal error, the input buffer
 * `src` may not have been completely consumed. In that case, the caller must
 * pass the unconsumed contents of `src` to `decode_*` again upon the next
 * call.
 *
 * [1]: enum.DecoderResult.html
 * [2]: #method.max_utf8_buffer_length_without_replacement
 * [3]: #method.max_utf8_buffer_length
 * [4]: #method.max_utf16_buffer_length
 *
 * # Infinite loops
 *
 * When converting with a fixed-size output buffer whose size is too small to
 * accommodate one character of output, an infinite loop ensues. When
 * converting with a fixed-size output buffer, it generally makes sense to
 * make the buffer fairly large (e.g. couple of kilobytes).
 */
class Decoder final
{
public:
  ~Decoder() {}
  static void operator delete(void* decoder) { decoder_free(reinterpret_cast<Decoder*>(decoder)); }

  /**
   * The `Encoding` this `Decoder` is for.
   *
   * BOM sniffing can change the return value of this method during the life
   * of the decoder.
   */
  inline gsl::not_null<const Encoding*> encoding() const { return decoder_encoding(this); }

  /**
   * Query the worst-case UTF-16 output size (with or without replacement).
   *
   * Returns the size of the output buffer in UTF-16 code units (`u16`)
   * that will not overflow given the current state of the decoder and
   * `byte_length` number of additional input bytes.
   *
   * Since the REPLACEMENT CHARACTER fits into one UTF-16 code unit, the
   * return value of this method applies also in the
   * `_without_replacement` case.
   */
  inline size_t max_utf16_buffer_length(size_t u16_length) const
  {
    return decoder_max_utf16_buffer_length(this, u16_length);
  }

  /**
   * Query the worst-case UTF-8 output size _without replacement_.
   *
   * Returns the size of the output buffer in UTF-8 code units (`u8`)
   * that will not overflow given the current state of the decoder and
   * `byte_length` number of additional input bytes when decoding without
   * replacement error handling.
   *
   * Note that this value may be too small for the `_with_replacement` case.
   * Use `max_utf8_buffer_length()` for that case.
   */
  inline size_t max_utf8_buffer_length_without_replacement(size_t byte_length) const
  {
    return decoder_max_utf8_buffer_length_without_replacement(this, byte_length);
  }

  /**
   * Query the worst-case UTF-8 output size _with replacement_.
   *
   * Returns the size of the output buffer in UTF-8 code units (`u8`)
   * that will not overflow given the current state of the decoder and
   * `byte_length` number of additional input bytes when decoding with
   * errors handled by outputting a REPLACEMENT CHARACTER for each malformed
   * sequence.
   */
  inline size_t max_utf8_buffer_length(size_t byte_length) const
  {
    return decoder_max_utf8_buffer_length(this, byte_length);
  }

  /**
   * Incrementally decode a byte stream into UTF-16 _without replacement_.
   *
   * See the documentation of the class for documentation for `decode_*`
   * methods collectively.
   */
  inline std::tuple<uint32_t, size_t, size_t> decode_to_utf16_without_replacement(
    gsl::span<const uint8_t> src, gsl::span<char16_t> dst, bool last)
  {
    size_t src_read = src.size();
    size_t dst_written = dst.size();
    uint32_t result = decoder_decode_to_utf16_without_replacement(this, src.data(), &src_read,
                                              dst.data(), &dst_written, last);
    return std::make_tuple(result, src_read, dst_written);
  }

  /**
   * Incrementally decode a byte stream into UTF-8 _without replacement_.
   *
   * See the documentation of the class for documentation for `decode_*`
   * methods collectively.
   */
  inline std::tuple<uint32_t, size_t, size_t> decode_to_utf8_without_replacement(
    gsl::span<const uint8_t> src, gsl::span<uint8_t> dst, bool last)
  {
    size_t src_read = src.size();
    size_t dst_written = dst.size();
    uint32_t result = decoder_decode_to_utf8_without_replacement(this, src.data(), &src_read,
                                             dst.data(), &dst_written, last);
    return std::make_tuple(result, src_read, dst_written);
  }

  /**
   * Incrementally decode a byte stream into UTF-16 with malformed sequences
   * replaced with the REPLACEMENT CHARACTER.
   *
   * See the documentation of the struct for documentation for `decode_*`
   * methods collectively.
   */
  inline std::tuple<uint32_t, size_t, size_t, bool>
  decode_to_utf16(gsl::span<const uint8_t> src, gsl::span<char16_t> dst,
                                   bool last)
  {
    size_t src_read = src.size();
    size_t dst_written = dst.size();
    bool had_replacements;
    uint32_t result = decoder_decode_to_utf16(
      this, src.data(), &src_read, dst.data(), &dst_written, last,
      &had_replacements);
    return std::make_tuple(result, src_read, dst_written, had_replacements);
  }

  /**
   * Incrementally decode a byte stream into UTF-8 with malformed sequences
   * replaced with the REPLACEMENT CHARACTER.
   *
   * See the documentation of the struct for documentation for `decode_*`
   * methods collectively.
   */
  inline std::tuple<uint32_t, size_t, size_t, bool>
  decode_to_utf8(gsl::span<const uint8_t> src, gsl::span<uint8_t> dst,
                                  bool last)
  {
    size_t src_read = src.size();
    size_t dst_written = dst.size();
    bool had_replacements;
    uint32_t result = decoder_decode_to_utf8(
      this, src.data(), &src_read, dst.data(), &dst_written, last,
      &had_replacements);
    return std::make_tuple(result, src_read, dst_written, had_replacements);
  }

private:
  Decoder() = delete;
};

/**
 * A converter that encodes a Unicode stream into bytes according to a
 * character encoding in a streaming (incremental) manner.
 *
 * The various `encode_*` methods take an input buffer (`src`) and an output
 * buffer `dst` both of which are caller-allocated. There are variants for
 * both UTF-8 and UTF-16 input buffers.
 *
 * An `encode_*` method encode characters from `src` into bytes characters
 * stored into `dst` until one of the following three things happens:
 *
 * 1. An unmappable character is encountered (`*_without_replacement` variants
 *    only).
 *
 * 2. The output buffer has been filled so near capacity that the decoder
 *    cannot be sure that processing an additional character of input wouldn't
 *    cause so much output that the output buffer would overflow.
 *
 * 3. All the input characters have been processed.
 *
 * The `encode_*` method then returns tuple of a status indicating which one
 * of the three reasons to return happened, how many input code units (`u8`
 * when encoding from UTF-8 and `u16` when encoding from UTF-16) were read,
 * how many output bytes were written (except when encoding into `Vec<u8>`,
 * whose length change indicates this), and in the case of the variants that
 * perform replacement, a boolean indicating whether an unmappable
 * character was replaced with a numeric character reference during the call.
 *
 * The number of bytes "written" is what's logically written. Garbage may be
 * written in the output buffer beyond the point logically written to.
 *
 * In the case of the methods whose name ends with
 * `*_without_replacement`, the status is an [`EncoderResult`][1] enumeration
 * (possibilities `Unmappable`, `OutputFull` and `InputEmpty` corresponding to
 * the three cases listed above).
 *
 * In the case of methods whose name does not end with
 * `*_without_replacement`, unmappable characters are automatically replaced
 * with the corresponding numeric character references and unmappable
 * characters do not cause the methods to return early.
 *
 * When encoding from UTF-8 without replacement, the methods are guaranteed
 * not to return indicating that more output space is needed if the length
 * of the output buffer is at least the length returned by
 * [`max_buffer_length_from_utf8_without_replacement()`][2]. When encoding from
 * UTF-8 with replacement, the length of the output buffer that guarantees the
 * methods not to return indicating that more output space is needed in the
 * absence of unmappable characters is given by
 * [`max_buffer_length_from_utf8_if_no_unmappables()`][3]. When encoding from
 * UTF-16 without replacement, the methods are guaranteed not to return
 * indicating that more output space is needed if the length of the output
 * buffer is at least the length returned by
 * [`max_buffer_length_from_utf16_without_replacement()`][4]. When encoding
 * from UTF-16 with replacement, the the length of the output buffer that
 * guarantees the methods not to return indicating that more output space is
 * needed in the absence of unmappable characters is given by
 * [`max_buffer_length_from_utf16_if_no_unmappables()`][5].
 * When encoding with replacement, applications are not expected to size the
 * buffer for the worst case ahead of time but to resize the buffer if there
 * are unmappable characters. This is why max length queries are only available
 * for the case where there are no unmappable characters.
 *
 * When encoding from UTF-8, each `src` buffer _must_ be valid UTF-8. (When
 * calling from Rust, the type system takes care of this.) When encoding from
 * UTF-16, unpaired surrogates in the input are treated as U+FFFD REPLACEMENT
 * CHARACTERS. Therefore, in order for astral characters not to turn into a
 * pair of REPLACEMENT CHARACTERS, the caller must ensure that surrogate pairs
 * are not split across input buffer boundaries.
 *
 * After an `encode_*` call returns, the output produced so far, taken as a
 * whole from the start of the stream, is guaranteed to consist of a valid
 * byte sequence in the target encoding. (I.e. the code unit sequence for a
 * character is guaranteed not to be split across output buffers. However, due
 * to the stateful nature of ISO-2022-JP, the stream needs to be considered
 * from the start for it to be valid. For other encodings, the validity holds
 * on a per-output buffer basis.)
 *
 * The boolean argument `last` indicates that the end of the stream is reached
 * when all the characters in `src` have been consumed. This argument is needed
 * for ISO-2022-JP and is ignored for other encodings.
 *
 * An `Encoder` object can be used to incrementally encode a byte stream.
 *
 * During the processing of a single stream, the caller must call `encode_*`
 * zero or more times with `last` set to `false` and then call `encode_*` at
 * least once with `last` set to `true`. If `encode_*` returns `InputEmpty`,
 * the processing of the stream has ended. Otherwise, the caller must call
 * `encode_*` again with `last` set to `true` (or treat an `Unmappable` result
 * as a fatal error).
 *
 * Once the stream has ended, the `Encoder` object must not be used anymore.
 * That is, you need to create another one to process another stream.
 *
 * When the encoder returns `OutputFull` or the encoder returns `Unmappable`
 * and the caller does not wish to treat it as a fatal error, the input buffer
 * `src` may not have been completely consumed. In that case, the caller must
 * pass the unconsumed contents of `src` to `encode_*` again upon the next
 * call.
 *
 * [1]: enum.EncoderResult.html
 * [2]: #method.max_buffer_length_from_utf8_without_replacement
 * [3]: #method.max_buffer_length_from_utf8_if_no_unmappables
 * [4]: #method.max_buffer_length_from_utf16_without_replacement
 * [5]: #method.max_buffer_length_from_utf16_if_no_unmappables
 *
 * # Infinite loops
 *
 * When converting with a fixed-size output buffer whose size is too small to
 * accommodate one character of output, an infinite loop ensues. When
 * converting with a fixed-size output buffer, it generally makes sense to
 * make the buffer fairly large (e.g. couple of kilobytes).
 */
class Encoder final
{
public:
  ~Encoder() {}

  static void operator delete(void* encoder) { encoder_free(reinterpret_cast<Encoder*>(encoder)); }

  /**
   * The `Encoding` this `Encoder` is for.
   */
  inline gsl::not_null<const Encoding*> encoding() const { return encoder_encoding(this); }

  /**
   * Query the worst-case output size when encoding from UTF-16 without
   * replacement.
   *
   * Returns the size of the output buffer in bytes that will not overflow
   * given the current state of the encoder and `u16_length` number of
   * additional input code units.
   */
  inline size_t max_buffer_length_from_utf16_without_replacement(size_t u16_length) const
  {
    return encoder_max_buffer_length_from_utf16_without_replacement(this, u16_length);
  }

  /**
   * Query the worst-case output size when encoding from UTF-8 without
   * replacement.
   *
   * Returns the size of the output buffer in bytes that will not overflow
   * given the current state of the encoder and `byte_length` number of
   * additional input code units.
   */
  inline size_t max_buffer_length_from_utf8_without_replacement(size_t byte_length) const
  {
    return encoder_max_buffer_length_from_utf8_without_replacement(this, byte_length);
  }

  /**
   * Query the worst-case output size when encoding from UTF-16 with
   * replacement.
   *
   * Returns the size of the output buffer in bytes that will not overflow
   * given the current state of the encoder and `u16_length` number of
   * additional input code units if there are no unmappable characters in
   * the input.
   */
  inline size_t max_buffer_length_from_utf16_if_no_unmappables(
    size_t u16_length) const
  {
    return encoder_max_buffer_length_from_utf16_if_no_unmappables(
      this, u16_length);
  }

  /**
   * Query the worst-case output size when encoding from UTF-8 with
   * replacement.
   *
   * Returns the size of the output buffer in bytes that will not overflow
   * given the current state of the encoder and `byte_length` number of
   * additional input code units if there are no unmappable characters in
   * the input.
   */
  inline size_t max_buffer_length_from_utf8_if_no_unmappables(
    size_t byte_length) const
  {
    return encoder_max_buffer_length_from_utf8_if_no_unmappables(
      this, byte_length);
  }

  /**
   * Incrementally encode into byte stream from UTF-16 _without replacement_.
   *
   * See the documentation of the struct for documentation for `encode_*`
   * methods collectively.
   */
  inline std::tuple<uint32_t, size_t, size_t> encode_from_utf16_without_replacement(
    gsl::span<const char16_t> src, gsl::span<uint8_t> dst, bool last)
  {
    size_t src_read = src.size();
    size_t dst_written = dst.size();
    uint32_t result = encoder_encode_from_utf16_without_replacement(this, src.data(), &src_read,
                                                dst.data(), &dst_written, last);
    return std::make_tuple(result, src_read, dst_written);
  }

  /**
   * Incrementally encode into byte stream from UTF-8 _without replacement_.
   *
   * See the documentation of the struct for documentation for `encode_*`
   * methods collectively.
   */
  inline std::tuple<uint32_t, size_t, size_t> encode_from_utf8_without_replacement(
    gsl::span<const uint8_t> src, gsl::span<uint8_t> dst, bool last)
  {
    size_t src_read = src.size();
    size_t dst_written = dst.size();
    uint32_t result = encoder_encode_from_utf8_without_replacement(this, src.data(), &src_read,
                                               dst.data(), &dst_written, last);
    return std::make_tuple(result, src_read, dst_written);
  }

  /**
   * Incrementally encode into byte stream from UTF-16 with unmappable
   * characters replaced with HTML (decimal) numeric character references.
   *
   * See the documentation of the struct for documentation for `encode_*`
   * methods collectively.
   */
  inline std::tuple<uint32_t, size_t, size_t, bool>
  encode_from_utf16(gsl::span<const char16_t> src,
                                     gsl::span<uint8_t> dst, bool last)
  {
    size_t src_read = src.size();
    size_t dst_written = dst.size();
    bool had_replacements;
    uint32_t result = encoder_encode_from_utf16(
      this, src.data(), &src_read, dst.data(), &dst_written, last,
      &had_replacements);
    return std::make_tuple(result, src_read, dst_written, had_replacements);
  }

  /**
   * Incrementally encode into byte stream from UTF-8 with unmappable
   * characters replaced with HTML (decimal) numeric character references.
   *
   * See the documentation of the struct for documentation for `encode_*`
   * methods collectively.
   */
  inline std::tuple<uint32_t, size_t, size_t, bool>
  encode_from_utf8(gsl::span<const uint8_t> src, gsl::span<uint8_t> dst,
                                    bool last)
  {
    size_t src_read = src.size();
    size_t dst_written = dst.size();
    bool had_replacements;
    uint32_t result = encoder_encode_from_utf8(
      this, src.data(), &src_read, dst.data(), &dst_written, last,
      &had_replacements);
    return std::make_tuple(result, src_read, dst_written, had_replacements);
  }

private:
  Encoder() = delete;
};

/**
 * An encoding as defined in the [Encoding Standard][1].
 *
 * An _encoding_ defines a mapping from a `uint8_t` sequence to a `char32_t`
 * sequence and, in most cases, vice versa. Each encoding has a name, an output
 * encoding, and one or more labels.
 *
 * _Labels_ are ASCII-case-insensitive strings that are used to identify an
 * encoding in formats and protocols. The _name_ of the encoding is the
 * preferred label in the case appropriate for returning from the
 * [`characterSet`][2] property of the `Document` DOM interface, except for
 * the replacement encoding whose name is not one of its labels.
 *
 * The _output encoding_ is the encoding used for form submission and URL
 * parsing on Web pages in the encoding. This is UTF-8 for the replacement,
 * UTF-16LE and UTF-16BE encodings and the encoding itself for other
 * encodings.
 *
 * [1]: https://encoding.spec.whatwg.org/
 * [2]: https://dom.spec.whatwg.org/#dom-document-characterset
 *
 * # Streaming vs. Non-Streaming
 *
 * When you have the entire input in a single buffer, you can use the
 * convenience methods [`decode()`][1], [`decode_with_bom_removal()`][2],
 * [`decode_without_bom_handling()`][3],
 * [`decode_without_bom_handling_and_without_replacement()`][4] and
 * [`encode()`][5]. (These methods are available to Rust callers only and are
 * not available in the C API.) Unlike the rest of the API available to Rust,
 * these methods perform heap allocations. You should the `Decoder` and
 * `Encoder` objects when your input is split into multiple buffers or when
 * you want to control the allocation of the output buffers.
 *
 * [1]: #method.decode
 * [2]: #method.decode_with_bom_removal
 * [3]: #method.decode_without_bom_handling
 * [4]: #method.decode_without_bom_handling_and_without_replacement
 * [5]: #method.encode
 *
 * # Instances
 *
 * All instances of `Encoding` are statically allocated and have the `'static`
 * lifetime. There is precisely one unique `Encoding` instance for each
 * encoding defined in the Encoding Standard.
 *
 * To obtain a reference to a particular encoding whose identity you know at
 * compile time, use a `static` that refers to enccoding. There is a `static`
 * for each encoding. The `static`s are named in all caps with hyphens
 * replaced with underscores (and in C/C++ have `_ENCODING` appended to the
 * name). For example, if you know at compile time that you will want to
 * decode using the UTF-8 encoding, use the `UTF_8` `static` (`UTF_8_ENCODING`
 * in C/C++).
 *
 * Additionally, there are non-reference-typed forms ending with `_INIT` to
 * work around the problem that `static`s of the type `&'static Encoding`
 * cannot be used to initialize items of an array whose type is
 * `[&'static Encoding; N]`.
 *
 * If you don't know what encoding you need at compile time and need to
 * dynamically get an encoding by label, use
 * <code>Encoding::<a href="#method.for_label">for_label</a>(<var>label</var>)</code>.
 *
 * Instances of `Encoding` can be compared with `==` (in both Rust and in
 * C/C++).
 */
class Encoding final
{
public:

  /**
   * Implements the
   * [_get an encoding_](https://encoding.spec.whatwg.org/#concept-encoding-get)
   * algorithm.
   *
   * If, after ASCII-lowercasing and removing leading and trailing
   * whitespace, the argument matches a label defined in the Encoding
   * Standard, `Some(&'static Encoding)` representing the corresponding
   * encoding is returned. If there is no match, `None` is returned.
   *
   * The argument is of type `&[u8]` instead of `&str` to save callers
   * that are extracting the label from a non-UTF-8 protocol the trouble
   * of conversion to UTF-8. (If you have a `&str`, just call `.gsl::as_bytes()`
   * on it.)
   */
  static inline const Encoding* for_label(gsl::cstring_span<> label)
  {
    return encoding_for_label(reinterpret_cast<const uint8_t*>(label.data()),
                              label.length());
  }

  /**
   * This method behaves the same as `for_label()`, except when `for_label()`
   * would return `Some(REPLACEMENT)`, this method returns `None` instead.
   *
   * This method is useful in scenarios where a fatal error is required
   * upon invalid label, because in those cases the caller typically wishes
   * to treat the labels that map to the replacement encoding as fatal
   * errors, too.
   */
  static inline const Encoding* for_label_no_replacement(gsl::cstring_span<> label)
  {
    return encoding_for_label_no_replacement(
      reinterpret_cast<const uint8_t*>(label.data()), label.length());
  }

  /**
   * If the argument matches exactly (case-sensitively; no whitespace
   * removal performed) the name of an encoding, returns
   * `&'static Encoding` representing that encoding. Otherwise panics.
   *
   * The motivating use case for this method is interoperability with
   * legacy Gecko code that represents encodings as name string instead of
   * type-safe `Encoding` objects. Using this method for other purposes is
   * most likely the wrong thing to do.
   *
   * # Panics
   *
   * Panics if the argument is not the name of an encoding.
   */
  static inline gsl::not_null<const Encoding*> for_name(gsl::cstring_span<> name)
  {
    return encoding_for_name(reinterpret_cast<const uint8_t*>(name.data()),
                             name.length());
  }

  /**
   * Performs non-incremental BOM sniffing.
   *
   * The argument must either be a buffer representing the entire input
   * stream (non-streaming case) or a buffer representing at least the first
   * three bytes of the input stream (streaming case).
   *
   * Returns `Some((UTF_8, 3))`, `Some((UTF_16LE, 2))` or
   * `Some((UTF_16BE, 3))` if the argument starts with the UTF-8, UTF-16LE
   * or UTF-16BE BOM or `None` otherwise.
   */
  static inline std::tuple<const Encoding*, size_t> for_bom(gsl::span<const uint8_t> buffer)
  {
    size_t len = buffer.size();
    const Encoding* encoding = encoding_for_bom(buffer.data(), &len);
    return std::make_tuple(encoding, len);
  }

  /**
   * Returns the name of this encoding.
   *
   * This name is appropriate to return as-is from the DOM
   * `document.characterSet` property.
   */
  inline std::string name() const
  {
    std::string name(ENCODING_NAME_MAX_LENGTH, '\0');
    // http://herbsutter.com/2008/04/07/cringe-not-vectors-are-guaranteed-to-be-contiguous/#comment-483
    size_t length = encoding_name(this, reinterpret_cast<uint8_t*>(&name[0]));
    name.resize(length);
    return name;
  }

  /**
   * Checks whether the _output encoding_ of this encoding can encode every
   * `char`. (Only true if the output encoding is UTF-8.)
   */
  inline bool can_encode_everything() const
  {
    return encoding_can_encode_everything(this);
  }

  /**
   * Checks whether the bytes 0x00...0x7F map exclusively to the characters
   * U+0000...U+007F and vice versa.
   */
  inline bool is_ascii_compatible() const
  {
    return encoding_is_ascii_compatible(this);
  }

  /**
   * Returns the _output encoding_ of this encoding. This is UTF-8 for
   * UTF-16BE, UTF-16LE and replacement and the encoding itself otherwise.
   */
  inline gsl::not_null<const Encoding*> output_encoding() const
  {
    return encoding_output_encoding(this);
  }

  /**
   * Decode complete input to `std::string` _with BOM sniffing_ and with
   * malformed sequences replaced with the REPLACEMENT CHARACTER when the
   * entire input is available as a single buffer (i.e. the end of the
   * buffer marks the end of the stream).
   *
   * This method implements the (non-streaming version of) the
   * [_decode_](https://encoding.spec.whatwg.org/#decode) spec concept.
   *
   * The second item in the returned tuple is the encoding that was actually
   * used (which may differ from this encoding thanks to BOM sniffing).
   *
   * The third item in the returned tuple indicates whether there were
   * malformed sequences (that were replaced with the REPLACEMENT CHARACTER).
   *
   * _Note:_ It is wrong to use this when the input buffer represents only
   * a segment of the input instead of the whole input. Use `new_decoder()`
   * when decoding segmented input.
   */
  inline std::tuple<std::string, const Encoding*, bool> decode(gsl::span<const uint8_t> bytes) const {
    const Encoding* encoding;
    size_t bom_length;
    std::tie(encoding, bom_length) = Encoding::for_bom(bytes);
    if (encoding) {
      bytes = bytes.subspan(bom_length);
    } else {
      encoding = this;
    }
    bool had_errors;
    std::string str;
    std::tie(str, had_errors) = encoding->decode_without_bom_handling(bytes);
    return std::make_tuple(str, encoding, had_errors);
  }

  /**
   * Decode complete input to `std::string` _with BOM removal_ and with
   * malformed sequences replaced with the REPLACEMENT CHARACTER when the
   * entire input is available as a single buffer (i.e. the end of the
   * buffer marks the end of the stream).
   *
   * When invoked on `UTF_8`, this method implements the (non-streaming
   * version of) the
   * [_UTF-8 decode_](https://encoding.spec.whatwg.org/#utf-8-decode) spec
   * concept.
   *
   * The second item in the returned pair indicates whether there were
   * malformed sequences (that were replaced with the REPLACEMENT CHARACTER).
   *
   * _Note:_ It is wrong to use this when the input buffer represents only
   * a segment of the input instead of the whole input. Use
   * `new_decoder_with_bom_removal()` when decoding segmented input.
   */
  inline std::tuple<std::string, bool> decode_with_bom_removal(gsl::span<const uint8_t> bytes) const {
    if (this == UTF_8_ENCODING && bytes.size() >= 3 && (gsl::as_bytes(bytes.first<3>()) == gsl::as_bytes(gsl::make_span("\xEF\xBB\xBF")))) {
      bytes = bytes.subspan(3, bytes.size() - 3);
    } else if (this == UTF_16LE_ENCODING && bytes.size() >= 2 && (gsl::as_bytes(bytes.first<2>()) == gsl::as_bytes(gsl::make_span("\xFF\xFE")))) {
      bytes = bytes.subspan(2, bytes.size() - 2);
    } else if (this == UTF_16BE_ENCODING && bytes.size() >= 2 && (gsl::as_bytes(bytes.first<2>()) == gsl::as_bytes(gsl::make_span("\xFE\xFF")))) {
      bytes = bytes.subspan(2, bytes.size() - 2);
    }
    return decode_without_bom_handling(bytes);
  }

  /**
   * Decode complete input to `std::string` _without BOM handling_ and
   * with malformed sequences replaced with the REPLACEMENT CHARACTER when
   * the entire input is available as a single buffer (i.e. the end of the
   * buffer marks the end of the stream).
   *
   * When invoked on `UTF_8`, this method implements the (non-streaming
   * version of) the
   * [_UTF-8 decode without BOM_](https://encoding.spec.whatwg.org/#utf-8-decode-without-bom)
   * spec concept.
   *
   * The second item in the returned pair indicates whether there were
   * malformed sequences (that were replaced with the REPLACEMENT CHARACTER).
   *
   * _Note:_ It is wrong to use this when the input buffer represents only
   * a segment of the input instead of the whole input. Use
   * `new_decoder_without_bom_handling()` when decoding segmented input.
   */
  inline std::tuple<std::string, bool> decode_without_bom_handling(gsl::span<const uint8_t> bytes) const {
    auto decoder = new_decoder_without_bom_handling();
    std::string string(decoder->max_utf8_buffer_length(bytes.size()), '\0');
    uint32_t result;
    size_t read;
    size_t written;
    bool had_errors;
    std::tie(result, read, written, had_errors) = decoder->decode_to_utf8(bytes, gsl::make_span(reinterpret_cast<uint8_t*>(&string[0]), string.size()), true);
    assert(read == static_cast<size_t>(bytes.size()));
    assert(written <= static_cast<size_t>(string.size()));
    assert(result == INPUT_EMPTY);
    string.resize(written);
    return std::make_tuple(string, had_errors);
  }

  /**
   * Decode complete input to `std::string` _without BOM handling_ and
   * _with malformed sequences treated as fatal_ when the entire input is
   * available as a single buffer (i.e. the end of the buffer marks the end
   * of the stream).
   *
   * When invoked on `UTF_8`, this method implements the (non-streaming
   * version of) the
   * [_UTF-8 decode without BOM or fail_](https://encoding.spec.whatwg.org/#utf-8-decode-without-bom-or-fail)
   * spec concept.
   *
   * Returns `None` if a malformed sequence was encountered and the result
   * of the decode as `Some(String)` otherwise.
   *
   * _Note:_ It is wrong to use this when the input buffer represents only
   * a segment of the input instead of the whole input. Use
   * `new_decoder_without_bom_handling()` when decoding segmented input.
   */
  inline std::experimental::optional<std::string> decode_without_bom_handling_and_without_replacement(gsl::span<const uint8_t> bytes) const {
    auto decoder = new_decoder_without_bom_handling();
    std::string string(decoder->max_utf8_buffer_length_without_replacement(bytes.size()), '\0');
    uint32_t result;
    size_t read;
    size_t written;
    std::tie(result, read, written) = decoder->decode_to_utf8_without_replacement(bytes, gsl::make_span(reinterpret_cast<uint8_t*>(&string[0]), string.size()), true);
    assert(read == static_cast<size_t>(bytes.size()));
    assert(written <= static_cast<size_t>(string.size()));
    assert(result != OUTPUT_FULL);
    if (result == INPUT_EMPTY) {
      string.resize(written);
      return string;
    }
  }

  /**
   * Encode complete input to `std::vector<uint8_t>` with unmappable characters
   * replaced with decimal numeric character references when the entire input
   * is available as a single buffer (i.e. the end of the buffer marks the
   * end of the stream).
   *
   * This method implements the (non-streaming version of) the
   * [_encode_](https://encoding.spec.whatwg.org/#encode) spec concept. For
   * the [_UTF-8 encode_](https://encoding.spec.whatwg.org/#utf-8-encode)
   * spec concept, it is slightly more efficient to use
   * <code><var>string</var>.gsl::as_bytes()</code> instead of invoking this
   * method on `UTF_8`.
   *
   * The second item in the returned tuple is the encoding that was actually
   * used (which may differ from this encoding thanks to some encodings
   * having UTF-8 as their output encoding).
   *
   * The third item in the returned tuple indicates whether there were
   * unmappable characters (that were replaced with HTML numeric character
   * references).
   *
   * _Note:_ It is wrong to use this when the input buffer represents only
   * a segment of the input instead of the whole input. Use `new_encoder()`
   * when encoding segmented output.
   */
  inline std::tuple<std::vector<uint8_t>, const Encoding*, bool> encode(gsl::span<const uint8_t> string) const {
    auto output_enc = output_encoding();
    if (output_enc == UTF_8_ENCODING) {
      std::vector<uint8_t> vec(string.size());
      std::memcpy(&vec[0], string.data(), string.size());
    }
    auto encoder = output_enc->new_encoder();
    std::vector<uint8_t> vec(encoder->max_buffer_length_from_utf8_if_no_unmappables(string.size()));
    bool total_had_errors = false;
    size_t total_read = 0;
    size_t total_written = 0;
    uint32_t result;
    size_t read;
    size_t written;
    bool had_errors;
    for (;;) {
      std::tie(result, read, written, had_errors) = encoder->encode_from_utf8(gsl::make_span(string).subspan(total_read), vec, true);
      total_read += read;
      total_written += written;
      total_had_errors |= had_errors;
      if (result == INPUT_EMPTY) {
        assert(total_read == static_cast<size_t>(string.size()));
        assert(total_written <= static_cast<size_t>(vec.size()));
        vec.resize(total_written);
        return std::make_tuple(vec, output_enc, total_had_errors);
      }
      auto needed = encoder->max_buffer_length_from_utf8_if_no_unmappables(string.size() - total_read);
      vec.resize(total_written + needed);
    }
  }

  /**
   * Instantiates a new decoder for this encoding with BOM sniffing enabled.
   *
   * BOM sniffing may cause the returned decoder to morph into a decoder
   * for UTF-8, UTF-16LE or UTF-16BE instead of this encoding.
   */
  inline std::unique_ptr<Decoder> new_decoder() const
  {
    std::unique_ptr<Decoder> decoder(encoding_new_decoder(this));
    return decoder;
  }

  /**
   * Instantiates a new decoder for this encoding with BOM sniffing enabled
   * into memory occupied by a previously-instantiated decoder.
   *
   * BOM sniffing may cause the returned decoder to morph into a decoder
   * for UTF-8, UTF-16LE or UTF-16BE instead of this encoding.
   */
  inline void new_decoder_into(Decoder& decoder) const
  {
    encoding_new_decoder_into(this, &decoder);
  }

  /**
   * Instantiates a new decoder for this encoding with BOM removal.
   *
   * If the input starts with bytes that are the BOM for this encoding,
   * those bytes are removed. However, the decoder never morphs into a
   * decoder for another encoding: A BOM for another encoding is treated as
   * (potentially malformed) input to the decoding algorithm for this
   * encoding.
   */
  inline std::unique_ptr<Decoder> new_decoder_with_bom_removal() const
  {
    std::unique_ptr<Decoder> decoder(encoding_new_decoder_with_bom_removal(this));
    return decoder;
  }

  /**
   * Instantiates a new decoder for this encoding with BOM removal
   * into memory occupied by a previously-instantiated decoder.
   *
   * If the input starts with bytes that are the BOM for this encoding,
   * those bytes are removed. However, the decoder never morphs into a
   * decoder for another encoding: A BOM for another encoding is treated as
   * (potentially malformed) input to the decoding algorithm for this
   * encoding.
   */
  inline void new_decoder_with_bom_removal_into(Decoder& decoder) const
  {
    encoding_new_decoder_with_bom_removal_into(this, &decoder);
  }

  /**
   * Instantiates a new decoder for this encoding with BOM handling disabled.
   *
   * If the input starts with bytes that look like a BOM, those bytes are
   * not treated as a BOM. (Hence, the decoder never morphs into a decoder
   * for another encoding.)
   *
   * _Note:_ If the caller has performed BOM sniffing on its own but has not
   * removed the BOM, the caller should use `new_decoder_with_bom_removal()`
   * instead of this method to cause the BOM to be removed.
   */
  inline std::unique_ptr<Decoder> new_decoder_without_bom_handling() const
  {
    std::unique_ptr<Decoder> decoder(encoding_new_decoder_without_bom_handling(this));
    return decoder;
  }

  /**
   * Instantiates a new decoder for this encoding with BOM handling disabled
   * into memory occupied by a previously-instantiated decoder.
   *
   * If the input starts with bytes that look like a BOM, those bytes are
   * not treated as a BOM. (Hence, the decoder never morphs into a decoder
   * for another encoding.)
   *
   * _Note:_ If the caller has performed BOM sniffing on its own but has not
   * removed the BOM, the caller should use `new_decoder_with_bom_removal()`
   * instead of this method to cause the BOM to be removed.
   */
  inline void new_decoder_without_bom_handling_into(Decoder& decoder) const
  {
    encoding_new_decoder_without_bom_handling_into(this, &decoder);
  }

  /**
   * Instantiates a new encoder for the output encoding of this encoding.
   */
  inline std::unique_ptr<Encoder> new_encoder() const
  {
    std::unique_ptr<Encoder> encoder(encoding_new_encoder(this));
    return encoder;
  }

  /**
   * Instantiates a new encoder for the output encoding of this encoding
   * into memory occupied by a previously-instantiated encoder.
   */
  inline void new_encoder_into(Encoder* encoder) const
  {
    encoding_new_encoder_into(this, encoder);
  }

  /**
   * Validates UTF-8.
   *
   * Returns the index of the first byte that makes the input malformed as
   * UTF-8 or the length of the slice if the slice is entirely valid.
   */
  static inline size_t utf8_valid_up_to(gsl::span<const uint8_t> buffer)
  {
    return encoding_utf8_valid_up_to(buffer.data(), buffer.size());
  }

  /**
   * Validates ASCII.
   *
   * Returns the index of the first byte that makes the input malformed as
   * ASCII or the length of the slice if the slice is entirely valid.
   */
  static inline size_t ascii_valid_up_to(gsl::span<const uint8_t> buffer)
  {
    return encoding_ascii_valid_up_to(buffer.data(), buffer.size());
  }

  /**
   * Validates ISO-2022-JP ASCII-state data.
   *
   * Returns the index of the first byte that makes the input not
   * representable in the ASCII state of ISO-2022-JP or the length of the
   * slice if the slice is entirely representable in the ASCII state of
   * ISO-2022-JP.
   */
  static inline size_t iso_2022_jp_ascii_valid_up_to(gsl::span<const uint8_t> buffer)
  {
    return encoding_iso_2022_jp_ascii_valid_up_to(buffer.data(), buffer.size());
  }

private:
  Encoding() = delete;
  ~Encoding() = delete;
};

#endif // encoding_rs_cpp_h_
