import os
import paste.deploy
import _evhttp as evhttp
from fapws2 import base
import sys
sys.setcheckinterval=100000 # since we don't use threads, internal checks are no more required

config_path = os.path.abspath(os.path.dirname(_args[0]))
path = '%s/%s' % (config_path, 'development.ini')
wsgi_app = paste.deploy.loadapp('config:%s' % path)

def start():
    evhttp.start("0.0.0.0", 5000)
    evhttp.set_base_module(base)
    
    def app(environ, start_response):
        environ['wsgi.multiprocess'] = False
        return wsgi_app(environ, start_response)

    evhttp.gen_http_cb(app)
    evhttp.event_dispatch()

if __name__=="__main__": start()
