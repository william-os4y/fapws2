# copy these lines to a file named: setup.py
# To build and install the elemlist module you need the following
# setup.py file that uses the distutils:
from distutils.core import setup, Extension
import os
import os.path 
import sys

if os.environ.has_key('LIBEVENT_SRC'):
        libevent_src = os.environ['LIBEVENT_SRC']
else:
        libevent_src = './libevent'

if not os.path.exists('/usr/lib/libevent.so'):
    print "We don't find libevent installed!!!!"
    sys.exit(1)
print "We will use the followinf libevent sources:"
print libevent_src
print ""

setup (name = "fapws2",
       version = "0.1",
       maintainer = "William",
       maintainer_email = "william@opensource4you.com",
       description = "Sample Python module",
       packages= ['fapws2','fapws2/contrib'],

       ext_modules = [
	       Extension('_evhttp',
	       sources=['fapws2/_evhttp.c'],
           # I'm on an archlinux ;-)
           # Here I'm pointing to the direcoty where libevent has been build
           # In this directory wi can find sources and compiled objects (as after a "./configure; make")
	       include_dirs=[libevent_src],
	       library_dirs=["/usr/local/lib"], # add LD_RUN_PATH in your environment
	       libraries=['event'],
           #extra_compile_args=["-march=athlon-xp", "-mtune=athlon-xp", "-ggdb"],
           #define_macros=[("DEBUG", "1")],
	       )
	       ]
)

# end of file: setup.py
