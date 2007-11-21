#!/usr/bin/env python

import _evhttp as evhttp
from fapws2 import base
import time
import sys
sys.setcheckinterval=100000 # since we don't use threads, internal checks are no more required

from fapws2.contrib import views, zip, mycgiapp

def start():
    evhttp.start("0.0.0.0", 8080)
    
    evhttp.set_base_module(base)
    
    def generic(environ, start_response):
        #print "GENERIC ENV",environ
        return ["Page not found"]
    hello=mycgiapp.CGIApplication("./test.cgi")
    evhttp.http_cb("/hellocgi",hello)
    
    evhttp.gen_http_cb(generic)
        
    evhttp.event_dispatch()
    

if __name__=="__main__":
    start()
