# Test file for extracting Python syntactical properties (Python 3 version)

import direct
import direct1, direct2
import direct3 as alias
import direct4 as alias4, direct5 as alias5
import direct.nested
from from1 import id1
from from2 import id3, id4
from from3 import id5 as alias6, id6 as alias7
from from4.nested1 import id7
from all1 import *
from . import relative1
from .relative import nested
from .relative2 import *
from .relative.nested import id8, id9
from .. import relative3
from ..relative4 import id10

direct.call()
print(direct.read)
direct.write = 1
nestedattr0.nestedattr1.nestedattr2 = nestedattr3.nestedattr4.nestedattr5

def outer():
    x = 1
    def inner():
        nonlocal x # valid in Python 3, invalid in Python 2
        x = 2
