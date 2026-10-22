"""Read the user's retail assets without modifying archives."""
from pathlib import Path
import struct
import zlib


def name_hash(name):
    data = name.lower().replace('/', '\\').encode('ascii')
    total = 0
    for i in range(len(data) - 1):
        for j in range(i, len(data)):
            total = (total + (data[j] + j) * (data[j] + 19) * (data[i] + 7) * (data[i] + i)) & 0xffffffff
    return total % 4000000000


class Retail:
    def __init__(self, root):
        self.root = Path(root)
        self.archives = {}

    def read(self, name):
        path = Path(name)
        for i in range(1, len(path.parts)):
            archive = self.root.joinpath(*path.parts[:i]).with_suffix('.dat')
            if not archive.is_file():
                continue
            if archive not in self.archives:
                with archive.open('rb') as f:
                    count, offset = struct.unpack('<II', f.read(8))
                    f.seek(offset)
                    self.archives[archive] = {e[0]: e[1:] for e in struct.iter_unpack('<5I', f.read(count * 20))}
            key = name_hash('/'.join(path.parts[i:]))
            if key not in self.archives[archive]:
                continue
            offset, count, packed, size = self.archives[archive][key]
            with archive.open('rb') as f:
                f.seek(offset)
                chunks = list(struct.iter_unpack('<II', f.read(count * 8)))
                data = bytearray()
                for pos, length in chunks:
                    f.seek(pos)
                    data.extend(zlib.decompress(f.read(length)))
                if len(data) != size:
                    raise ValueError('Invalid retail archive entry: ' + name)
                return bytes(data)
        return (self.root / path).read_bytes()
