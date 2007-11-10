#!/usr/bin/env python

import _evhttp as evhttp
from fapws2 import base
import time
import sys
sys.setcheckinterval=100000 # since we don't use threads, internal checks are no more required
from fapws2.contrib import django_handler
from fapws2.contrib import views
import django

def start():
    evhttp.start("0.0.0.0", 8080)
    
    #print evhttp.get_timeout()
    #evhttp.set_timeout(3)
    #print evhttp.get_timeout()
    
    evhttp.set_base_module(base)
    
    def generic(environ, start_response):
        #print "GENERIC ENV",environ
        #r=str(djangohandler(environ, start_response)).split('\n',1)[1]
        res=django_handler.handler(environ, start_response)
        return [res]
    
    staticfile=views.Staticfile(django.__path__[0] + '/contrib/admin/media/')
    evhttp.http_cb("/media/",staticfile)
    evhttp.gen_http_cb(generic)
        
    evhttp.event_dispatch()
    

if __name__=="__main__":
    start()
