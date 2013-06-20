# Test file for extracting import statements

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