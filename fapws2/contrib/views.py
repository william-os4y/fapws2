
class Staticfile:
    """ Generic class that you can use to dispatch static files
    You must use it like this:
      static=Staticfile("/rootpath/")
      evhttp.http_cb("/static/",static)
    NOTE: you must be consistent between /rootpath/ and /static/ concerning the ending "/"
    """
    def __init__(self, rootpath=""):
        self.rootpath=rootpath
    def __call__(self, environ, start_response):
        fpath=self.rootpath+environ['PATH_INFO']
        try:
            f=open(fpath, "rb")
        except:
            print "ERROR in Staticfile: file %s not existing" % (fpath)
            start_response('404 File not found',[])
            f=[]
        return f
