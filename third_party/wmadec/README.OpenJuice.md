# wmadec (JuicedConfigExt's WMA decoder)

The fixed build's music and stream banks (`.dsb`: Windows Media Audio 1 and 2 in ASF files) are decoded without the
system's codecs by JuicedConfigExt's decoder, `external/JuicedConfigExt/src/WmaDecoder` (the git submodule of
https://gitlab.com/sidecr3w/juicedconfig-extended): a port of FFmpeg n7.1's WMA decoder with its own ASF reader.
`OpenJuice.WmaDecoder.vcxproj` builds its sources as a static library. Its licence is the GNU Lesser General Public
License 2.1 or later (`external/JuicedConfigExt/src/WmaDecoder/COPYING.LGPL`); OpenJuice's whole source is published,
so the decoder can be changed and OpenJuice built again with it.
