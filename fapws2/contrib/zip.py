try:
    import cStringIO as StringIO
except:
    import StringIO
import gzip

class Gzip:
    #wsgi gzip middelware
    def __call__(self, f):
        def func(environ, start_response):
            content=f(environ, start_response)
            if 'gzip' in environ.get('HTTP_ACCEPT_ENCODING', ''):
                content="".join(content)
                sio = StringIO.StringIO()
                comp_file = gzip.GzipFile(mode='wb', compresslevel=6, fileobj=sio)
                comp_file.write(content)
                comp_file.close()
                start_response('200 OK', [('Content-Encoding', 'gzip')])
                res=sio.getvalue()
                return [res]
            else:
                return content
        return func
