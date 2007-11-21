#!/usr/bin/env python

import _evhttp as evhttp
from fapws2 import base
import time
import sys
sys.setcheckinterval=100000 # since we don't use threads, internal checks are no more required

from fapws2.contrib import views, zip

def start():
    evhttp.start("0.0.0.0", 8080)
    
    #print evhttp.get_timeout()
    #evhttp.set_timeout(3)
    #print evhttp.get_timeout()
    
    evhttp.set_base_module(base)
    
    def generic(environ, start_response):
        #print "GENERIC ENV",environ
        return ["Page not found"]
    
    @zip.Gzip()
    def hello(environ, start_response):
        #print "Header", environ
        if environ["PATH_INFO"]!="":
            return generic(environ, start_response)
        #print "params",environ["fapws.params"]
        #print "query",environ["QUERY_STRING"]
        #time.sleep(1)
        start_response('200 WHYNOT', [('toto',4444)])
        return ["Hello World!!"]
    
    def staticlong(environ, start_response):
        try:
            f=open("long.txt", "rb")
        except:
            f=["Page not found"]
        return f
        
    @zip.Gzip()    
    def staticlongzipped(environ, start_response):
        try:
            f=open("long.txt", "rb")
        except:
            f=["Page not found"]
        return f
        
    def staticshort(environ, start_response):
        f=open("short.txt", "rb")
        return f
    def testpost(environ, start_response):
        print "INPUT DATA",environ["wsgi.input"].getvalue()
        return ["OK. params are:%s" % (environ["fapws.params"])]
    class Test:
        def __init__(self):
            pass
        def __call__(self, environ, start_response):
            return ["Hello from Test"]
    
    evhttp.http_cb("/hello",hello)
    evhttp.http_cb("/longzipped", staticlongzipped)
    evhttp.http_cb("/long", staticlong)
    evhttp.http_cb("/short", staticshort)
    t=Test()
    evhttp.http_cb("/class", t)
    staticform=views.Staticfile("test.html")
    evhttp.http_cb("/staticform", staticform)
    evhttp.http_cb("/testpost", testpost)
    evhttp.http_cb("/",hello)
    
    evhttp.gen_http_cb(generic)
        
    evhttp.event_dispatch()
    

if __name__=="__main__":
    start()