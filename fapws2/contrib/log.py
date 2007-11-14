
import time
import os.path
import sys

class Log:
    def __init__(self,output=sys.stdout):
        self.output=output
    def __call__(self, f):
        def func(environ, start_response):
            res=f(environ, start_response)
            tts=time.strftime("%d/%b/%Y:%H:%M:%S", time.gmtime())
            if type(res)==type([]):
                content="".join(res)
                size=len(content)
            else:
                #this is a filetype object
                size=os.path.getsize(res.name)
            #this is provided by a proxy or direct
            remote_host=environ.get('HTTP_X_FORWARDED_FOR',environ['fapws.remote_host'])
            self.output.write("%s %s - [%s GMT] \"%s %s %s/%s.%s\" %s %s \"%s\" \"%s\"\n" % (remote_host, environ['HTTP_HOST'], tts, environ['REQUEST_METHOD'], environ['fapws.uri'], environ['wsgi.url_scheme'], environ['fapws.http_major'], environ['fapws.http_minor'], start_response.status_code, size, environ.get("HTTP_REFERER", "-"), environ['HTTP_USER_AGENT']))
            self.output.flush()
            return res
        return func
    
