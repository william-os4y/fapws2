from django.core.handlers import wsgi 

djhand=wsgi.WSGIHandler()
def handler(environ, start_response):
    res=djhand(environ, start_response)
    for key,val in res.headers.items():
        start_response.response_headers[key]=val
    start_response.cookies=res.cookies
    return res.content
