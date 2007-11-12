
import datetime
from Cookie import SimpleCookie, CookieError
try:
    import cStringIO as StringIO
except ImportError:
    import StringIO
import traceback, sys, string


from fapws2 import log
from fapws2 import utils
import _evhttp as evhttp

import config


status_reasons = {
    100: 'Continue',
    101: 'Switching Protocols',
    102: 'Processing',
    200: 'OK',
    201: 'Created',
    202: 'Accepted',
    203: 'Non-Authoritative Information',
    204: 'No Content',
    205: 'Reset Content',
    206: 'Partial Content',
    207: 'Multi-Status',
    300: 'Multiple Choices',
    301: 'Moved Permanently',
    302: 'Moved Temporarily',
    303: 'See Other',
    304: 'Not Modified',
    305: 'Use Proxy',
    307: 'Temporary Redirect',
    400: 'Bad Request',
    401: 'Unauthorized',
    402: 'Payment Required',
    403: 'Forbidden',
    404: 'Not Found',
    405: 'Method Not Allowed',
    406: 'Not Acceptable',
    407: 'Proxy Authentication Required',
    408: 'Request Time-out',
    409: 'Conflict',
    410: 'Gone',
    411: 'Length Required',
    412: 'Precondition Failed',
    413: 'Request Entity Too Large',
    414: 'Request-URI Too Large',
    415: 'Unsupported Media Type',
    416: 'Requested range not satisfiable',
    417: 'Expectation Failed',
    422: 'Unprocessable Entity',
    423: 'Locked',
    424: 'Failed Dependency',
    500: 'Internal Server Error',
    501: 'Not Implemented',
    502: 'Bad Gateway',
    503: 'Service Unavailable',
    504: 'Gateway Time-out',
    505: 'HTTP Version not supported',
    507: 'Insufficient Storage',
}


class Environ(dict):
    def __init__(self, *arg, **kw):
        self['wsgi.version'] = (1,0)
        #self['wgsi.errors'] = log.error_logfid
        self['wsgi.input'] = StringIO.StringIO()
        self['wsgi.multithread'] = False
        self['wsgi.multiprocess'] = True
        self['wsgi.run_once'] = False
        self['wsgi.url_scheme']="http"   #TODO:  support of https
        self['fapws.params']={}
    #here after some entry point before the Environ update
    def update_headers(self, data):
        dict.update(self,data)
    def update_uri(self, data):
        dict.update(self,data)
    def update_method(self, data):
        #note that wsgi.input is not passed by data, but directly update in self.env
        dict.update(self,data)
    def update_from_request(self, data):
        dict.update(self,data)

class Start_response:
    def __init__(self):
        self.status_code = "200"
        self.status_reasons = "OK"
        self.response_headers = {}
        self.exc_info = None
        self.cookies = SimpleCookie()
        # NEW -- sent records whether or not the headers have been send to the
        # client
        self.sent= False
        self.response_headers['Date'] = datetime.datetime.now().strftime('%a, %d %b %Y %H:%M:%S GMT')
        self.response_headers['Server'] = config.SERVER_IDENT

    def __call__(self, status, response_headers, exc_info=None):
        self.status_code, self.status_reasons = status.split(" ",1)
        self.status_code=str(self.status_code)
        for key,val in response_headers:
            #if type(key)!=type(""):
            key=str(key)
            #if type(val)!=type(""):
            val=str(val)
            self.response_headers[key] = val
        self.exc_info = exc_info #TODO: to implement
    def add_header(self, key,val):
        key=str(key)
        val=str(val)
        self.response_headers[key]=val
    def set_cookie(self, key, value='', max_age=None, expires=None, path='/', domain=None, secure=None):
        self.cookies[key] = value
        self.response_headers['Set-Cookie'] = self.cookies
        if max_age:
            self.cookies[key]['max-age'] = max_age
        if expires:
            if isinstance(expires, str):
                self.cookies[key]['expires'] = expires
            elif isinstance(expires, datetime.datetime):
                expires = expires.strftime('%a, %d %b %Y %H:%M:%S GMT')
            else:
                raise CookieError, 'expires must be a datetime object or a string'
            self.cookies[key]['expires'] = expires
        if path:
            self.cookies[key]['path'] = path
        if domain:
            self.cookies[key]['domain'] = domain
        if secure:
            self.cookies[key]['secure'] = secure
    def delete_cookie(self, key):
        if self.cookies:
            self.cookies[key] = ''
        self.cookies[key]['max-age'] = "0"
    def __str__(self):
        #res = "HTTP/1.1 %s %s\r\n" % (self.status_code, self.status_reasons)
        res=""
        for key, val in self.response_headers.items():
            if key.upper() == "SET-COOKIE":
                res += "%s\r\n" % val
            else:
                res += '%s: %s\r\n' % (key,val)
        res += "\r\n"
        return res
        
def redirectStdErr():
    """
    This methods allow use to redirect messages sent to stderr into a string
    Mandatory methods of the sys.stderr object are: 
        write: to insert data
        getvalue; to retreive all data
    """
    sys.stderr=StringIO.StringIO()

if __name__=="__main__":
    try:
        r=1/0
    except:
        print errorMsg()