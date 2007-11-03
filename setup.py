# copy these lines to a file named: setup.py
# To build and install the elemlist module you need the following
# setup.py file that uses the distutils:
from distutils.core import setup, Extension
import os
 
if os.environ.has_key('LIBEVENT_SRC'):
        libevent_src = os.environ['LIBEVENT_SRC']
else:
        libevent_src = '/var/abs/local/libevent/src/libevent-1.3e'

setup (name = "fapws2",
       version = "0.1",
       maintainer = "William",
       maintainer_email = "william@opensource4you.com",
       description = "Sample Python module",
       packages= ['fapws2'],

       ext_modules = [
	       Extension('_evhttp',
	       sources=['fapws2/_evhttp.c'],
           # I'm on an archlinux ;-)
           # Here I'm pointing to the direcoty where libevent has been build
           # In this directory wi can find sources and compiled objects (as after a "./configure; make")
	       include_dirs=[libevent_src],
	       library_dirs=[libevent_src],
	       libraries=['event'],
           #extra_compile_args=["-ggdb"],
           #define_macros=[("DEBUG", "1")],
	       )
	       ]
)

# end of file: setup.py
