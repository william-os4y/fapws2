
import time
import os.path

class Log:
    def __init__(self,app):
        self.application=app
    def __call__(self, environ, start_response):
        res=self.application(environ, start_response)
        tts=time.strftime("%d/%b/%Y:%H:%M:%S", time.gmtime())
        if type(res)==type([]):
            content="".join(res)
            size=len(content)
        else:
            #this is a filetype object
            size=os.path.getsize(res.name)
        print "%s %s - [%s] \"%s %s %s/%s.%s\" %s %s \"%s\" \"%s\"" % (environ['fapws.remote_host'], environ['HTTP_HOST'], tts, environ['REQUEST_METHOD'], environ['fapws.uri'], environ['wsgi.url_scheme'], environ['fapws.http_major'], environ['fapws.http_minor'], start_response.status_code, size, environ.get("HTTP_REFERER", "-"), environ['HTTP_USER_AGENT'])
        return res
    
def log_dec(application):
    return Log(application)