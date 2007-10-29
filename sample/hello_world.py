#!/usr/bin/env python

import _evhttp as evhttp
from fapws2 import base
import time
import sys
sys.setcheckinterval=100000 # since we don't use threads, internal checks are no more required

def start():
    evhttp.start("0.0.0.0", 8080)
    print "started"
    #print evhttp.get_timeout()
    #evhttp.set_timeout(3)
    #print evhttp.get_timeout()
    
    evhttp.set_base_module(base)
    
    def generic(environ, start_response):
        return ["Page not found"]
    
    def test(environ, start_response):
        #print "Header", environ.env
        #print environ.env['QUERY_DICT']
        #time.sleep(1)
        start_response('200 WHYNOT', [('toto',4444)])
        return ["Hello"," World!!"]
    
    evhttp.http_cb("/test",test)
    print "cb done"
    evhttp.gen_http_cb(generic)
    print "gen cb done"
    
    print "before loop"
    evhttp.event_dispatch()
    print "after loop"

if __name__=="__main__":
    start()