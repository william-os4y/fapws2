#!/usr/bin/env python

import _evhttp as evhttp
from fapws2 import base
import time
import sys
sys.setcheckinterval=100000 # since we don't use threads, internal checks are no more required

def start():
    evhttp.start("0.0.0.0", 8080)
    
    #print evhttp.get_timeout()
    #evhttp.set_timeout(3)
    #print evhttp.get_timeout()
    
    evhttp.set_base_module(base)
    
    def generic(environ, start_response):
        return ["Page not found"]
    
    def hello(environ, start_response):
        #print "Header", environ.env
        #print "params",environ.env["fapws.params"]
        #print "query",environ.env["QUERY_STRING"]
        #time.sleep(1)
        #start_response('200 WHYNOT', [('toto',4444)])
        return ["Hello World!!"]
    
    def staticfile(environ, start_response):
        print environ.env
        try:
            f=open(environ.env['PATH_INFO'], "rb")
        except:
            f=["Page not found"]
        return f
    def testpost(environ, start_response):
        print "INPUT DATA",environ.env["wsgi.input"].read()
        print "fapws.PARAMS",environ.env["fapws.params"]
        return ["OK"]
    class Test:
        def __init__(self):
            pass
        def __call__(self, environ, start_response):
            return ["Hello from Test"]
    
    evhttp.http_cb("/hello",hello)
    evhttp.http_cb("/testpost", testpost)
    evhttp.http_cb("/static/",staticfile)
    evhttp.http_cb("/class", Test())
    
    evhttp.gen_http_cb(generic)
        
    evhttp.event_dispatch()
    

if __name__=="__main__":
    start()