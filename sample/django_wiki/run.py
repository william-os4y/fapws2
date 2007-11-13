
import _evhttp as evhttp
from fapws2 import base
import time
import sys
sys.setcheckinterval=100000 # since we don't use threads, internal checks are no more required
from fapws2.contrib import django_handler
from fapws2.contrib import views, log, zip
import django

def start():
    evhttp.start("0.0.0.0", 8080)
    
    evhttp.set_base_module(base)
    
    #@log.Log(open("access.log","a"))
    #@zip.Gzip()
    def generic(environ, start_response):
        #print "GENERIC ENV",environ
        res=django_handler.handler(environ, start_response)
        return [res]
    
    #here log will got to the standard output
    @log.Log()
    def staticfile(environ, start_response):
        #we ask the browser to store those static files in his cache for 1 hour
        res=views.Staticfile(django.__path__[0] + '/contrib/admin/media/', maxage=3600)
        return res(environ, start_response)
    
    evhttp.http_cb("/media/",staticfile)
    evhttp.gen_http_cb(generic)
        
    evhttp.event_dispatch()
    

if __name__=="__main__":
    start()
