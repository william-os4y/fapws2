#!/usr/bin/env python

import _evhttp as evhttp
from fapws2 import base
        

evhttp.start("0.0.0.0", 8080)
print "started"
#print evhttp.get_timeout()
#evhttp.set_timeout(3)
#print evhttp.get_timeout()

evhttp.set_base_module(base)

def generic(environ, start_response):
    return "Page not found"

def test(environ, start_response):
    #print "Header", environ.env
    start_response('200 WHYNOT', [('toto',4444)])
    return "Hello World!!"

evhttp.http_cb("/test",test)
print "cb done"
evhttp.gen_http_cb(generic)
print "gen cb done"

print "before loop"
evhttp.event_dispatch()
print "after loop"
