from setuptools import setup, find_packages, Extension
import sys, os

version = '0.3'


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


def read_file(name):
    return open(os.path.join(os.path.dirname(__file__),name)).read()

readme = read_file('README')

setup(name='fapws2',
      version=version,
      description="fast asynchronous wsgi server",
      long_description=readme,
classifiers=['Development Status :: 4 - Beta','Environment :: Web Environment','License :: OSI Approved :: GNU General Public License (GPL)','Programming Language :: C','Programming Language :: Python','Topic :: Internet :: WWW/HTTP :: HTTP Servers','Topic :: Internet :: WWW/HTTP :: WSGI :: Server'], # Get strings from http://pypi.python.org/pypi?%3Aaction=list_classifiers
      keywords='',
      author='William',
      author_email='william.os4y@gmail.com',
      url='http://william-os4y.livejournal.com/',
      license='GPL',
      include_package_data=True,
      zip_safe=False,
      install_requires=[
          # -*- Extra requirements: -*-
      ],
      entry_points="""
      # -*- Entry points: -*-
      """,

      packages= find_packages(),
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
