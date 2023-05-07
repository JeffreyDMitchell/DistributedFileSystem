import re
from random import randint

class entry:
    def __init__(self, f_str):
        captures = re.search(r"([0-9]+)\.([0-9]+)\.(.+)", f_str)
        self.chunk = captures.group(1)
        self.timestamp = captures.group(2)
        self.f_name = captures.group(3)
    def toString(self):
        return f"{self.chunk}.{self.timestamp}.{self.f_name}"

def key_entry(entry):
    return entry.f_name, int(entry.timestamp), int(entry.chunk)

SERVER_CT = 4
f_strings = [
    '0.9213354657969466.baz',
    '1.9213354657969466.baz',
    '2.9213354657969466.baz',
    '0.2334086413411495.bar',
    '1.2334086413411495.bar',
    '2.2334086413411495.bar',
    '0.7475219534057661.bar',
    '1.7475219534057661.bar',
    '2.7475219534057661.bar',
    '0.5925646187044382.baz',
    '1.5925646187044382.baz',
    '2.5925646187044382.baz',
    '3.5925646187044382.baz',
    '0.3607929459051794.index',
    '1.3607929459051794.index',
    '0.9550735329462691.index',
    '1.9550735329462691.index',
    '0.2261696403266304.buzz',
    '1.2261696403266304.buzz',
    '0.7412785127868601.buzz',
    '1.7412785127868601.buzz',
    '2.7412785127868601.buzz',
    '0.7018631345596767.foo'
]

def generateNames(ct):
    fnames = ['foo', 'bar', 'baz', 'buzz', 'wine', 'index']
    for n in range(ct):
        name = fnames[randint(0, len(fnames) -1)]
        timestamp = randint(1E15, 1E16 - 1)
        for i in range(randint(0, SERVER_CT)):
            print(f"{str(i)}.{str(timestamp)}.{name}")

# generateNames(10)

# e = entry('0.7018631345596767.foo')

# print(f"{e.chunk}\n{e.timestamp}\n{e.f_name}\n")

entries = [entry(s) for s in f_strings]

for e in entries:
    print(e.toString())

entries = sorted(entries, key=key_entry)

print("\n\n\n")

for e in entries:
    print(e.toString())