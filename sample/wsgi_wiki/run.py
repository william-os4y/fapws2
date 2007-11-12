#!/usr/bin/env python

import _evhttp as evhttp
from fapws2 import base
import sys
sys.setcheckinterval=100000 # since we don't use threads, internal checks are no more required

import views

def start():
    evhttp.start("0.0.0.0", 8080)
    
    evhttp.set_base_module(base)
    
    def generic(environ, start_response):
        return ["page not found"]
    
    def index(environ, start_response):
        return views.index(environ, start_response)
    
    def display(environ, start_response):
        return views.display(environ, start_response)
    def edit(environ, start_response):
        r=views.Edit()
        return r(environ, start_response)
    
    def staticfile(environ, start_response):
        try:
            f=open("static/"+ environ['PATH_INFO'], "rb")
        except:
            f=["Page not found"]
        return f
    def favicon(environ, start_response):
        return open("static/img/favicon.ico", "rb")
    
    evhttp.http_cb("/display/",display)
    evhttp.http_cb("/edit", edit)
    evhttp.http_cb("/static/",staticfile)
    evhttp.http_cb('/favicon.ico',favicon),

    evhttp.http_cb("/", index)
    evhttp.gen_http_cb(generic)
        
    evhttp.event_dispatch()
    

if __name__=="__main__":
    start()