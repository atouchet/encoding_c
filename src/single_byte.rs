// Copyright 2015-2016 Mozilla Foundation. See the COPYRIGHT
// file at the top-level directory of this distribution.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use handles::*;
use data::*;
use variant::*;
use super::*;

pub struct SingleByteDecoder {
    table: &'static [u16; 128],
}

impl SingleByteDecoder {
    pub fn new(encoding: &'static Encoding, data: &'static [u16; 128]) -> Decoder {
        Decoder::new(encoding,
                     VariantDecoder::SingleByte(SingleByteDecoder { table: data }))
    }

    pub fn reset(&mut self) {}

    pub fn max_utf16_buffer_length(&self, byte_length: usize) -> usize {
        byte_length
    }

    pub fn max_utf8_buffer_length(&self, byte_length: usize) -> usize {
        byte_length * 3
    }

    pub fn max_utf8_buffer_length_with_replacement(&self, byte_length: usize) -> usize {
        byte_length * 3
    }

    decoder_functions!({},
                       {},
                       {
                           if b < 0x80 {
                               // XXX optimize ASCII
                               destination_handle.write_ascii(b);
                           } else {
                               let mapped = self.table[b as usize - 0x80usize];
                               if mapped == 0u16 {
                                   return (DecoderResult::Malformed(1),
                                           unread_handle.consumed(),
                                           destination_handle.written());
                               }
                               destination_handle.write_bmp_excl_ascii(mapped);
                           }
                       },
                       self,
                       src_consumed,
                       dest,
                       b,
                       destination_handle,
                       unread_handle,
                       check_space_bmp);
}

pub struct SingleByteEncoder {
    table: &'static [u16; 128],
}

impl SingleByteEncoder {
    pub fn new(encoding: &'static Encoding, data: &'static [u16; 128]) -> Encoder {
        Encoder::new(encoding,
                     VariantEncoder::SingleByte(SingleByteEncoder { table: data }))
    }

    pub fn max_buffer_length_from_utf16(&self, u16_length: usize) -> usize {
        0 // TODO
    }

    pub fn max_buffer_length_from_utf8(&self, byte_length: usize) -> usize {
        0 // TODO
    }

    pub fn max_buffer_length_from_utf16_with_replacement_if_no_unmappables(&self,
                                                                           u16_length: usize)
                                                                           -> usize {
        0 // TODO
    }

    pub fn max_buffer_length_from_utf8_with_replacement_if_no_unmappables(&self,
                                                                          byte_length: usize)
                                                                          -> usize {
        0 // TODO
    }

    pub fn encode_from_utf16(&mut self,
                             src: &[u16],
                             dst: &mut [u8],
                             last: bool)
                             -> (EncoderResult, usize, usize) {
        // XXX
        (EncoderResult::InputEmpty, 0, 0)
    }

    pub fn encode_from_utf8(&mut self,
                            src: &str,
                            dst: &mut [u8],
                            last: bool)
                            -> (EncoderResult, usize, usize) {
        // XXX
        (EncoderResult::InputEmpty, 0, 0)
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use super::super::*;

}