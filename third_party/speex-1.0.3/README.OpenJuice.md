# Speex 1.0.3 (libspeex)

The stock game links libspeex 1.0.3 statically (GameSpy Voice 2's codec, `0x00580B00` .. `0x0058AF30` of the
retail build; docs/spec/28-network.md, "Voice"). The version was told from the executable: its warning
"Packet is larger than allocated buffer: " (1.0.2 and before read "Packet if larger"), and the narrowband encoder's
control (`0x00588C50`) has no `SPEEX_GET_LOOKAHEAD` (39), which 1.0.4 added; 1.0.5's `VERY_SMALL` (1e-15) is not in it.

The files of `libspeex/` are those of the release as Xiph.org published it, unchanged:
`https://downloads.xiph.org/releases/speex/speex-1.0.3.tar.gz`
(SHA-256 `6fae7dba252b660999cab4b43952dae9910103b982d2588efeada57af4e4e1a6`), less the test programs
(`testenc*.c`). Its licence is the BSD licence of `COPYING`.
