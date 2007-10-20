try:
    import cStringIO as StringIO
except ImportError:
    import StringIO
import log
import utils
import _evhttp as evhttp

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


class Environ:
    def __init__(self):
        self.env={}
    def reset(self):
        self.env={}
        #self.env['wsgi.version'] = (1,0)
        #self.env['wgsi.errors'] = log.error_logfid
        #self.env['wsgi.input'] = StringIO.StringIO('')
        #self.env['wsgi.multithread'] = False
        self.env['wsgi.multiprocess'] = True
        self.env['wsgi.run_once'] = False
    def update(self, data):
        self.env.update(data)

#the following name can not change
environ=Environ()

class Start_response:
    def __init__(self):
        self.data="toto"

#The following name can not change
start_response=Start_response()

