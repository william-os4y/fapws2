import _evhttp as evhttp
from fapws2 import base
import sys
sys.setcheckinterval=100000 # since we don't use threads, internal checks are no more required
from fapws2.contrib import views

from MoinMoin.server.wsgi import moinmoinApp
from MoinMoin import config

def start():
    evhttp.start("0.0.0.0", 8080)
    
    evhttp.set_base_module(base)

    static=views.Staticfile("/usr/share/moin/htdocs")
    evhttp.http_cb("/wiki", static)
    
    evhttp.gen_http_cb(moinmoinApp)
        
    evhttp.event_dispatch()
    

if __name__=="__main__":
    start()
