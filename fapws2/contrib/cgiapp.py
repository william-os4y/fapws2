import os.path
import subprocess

class CGIApplication:
    def __init__(self, script):
        self.script=script
        self.dirname=os.path.dirname(script)
        self.cgi_environ={}
    def _setup_cgi_environ(self, environ):
        for key,val in environ.items():
            if type(val)==type(""):
                self.cgi_environ[key]=val
        self.cgi_environ['REQUEST_URI']=environ['fapws.uri']
        
    def _split_return(self,data):
        if '\n\n' in data:
            header,content=data.split('\n\n',1)
        else:
            header=""
            content=data
        return header, content
    def _split_header(self, header):
        i=0
        headers=[]
        firstline="HTTP/1.1 200 OK"
        for line in header.split('\n'):
            if i==0 and ':' not in line:
                firstline=line
            if ':' in line:
                name,value=line.split(':',1)
                headers.append((name,value))
            i+=1
        status=" ".join(firstline.split()[1:])
        return status, headers
        
    def __call__(self, environ, start_response):
        self._setup_cgi_environ(environ)
        proc=subprocess.Popen(
                    [self.script], 
                    stdin=subprocess.PIPE,
                    stdout=subprocess.PIPE,
                    stderr=subprocess.PIPE,
                    env=self.cgi_environ, 
                    cwd=self.dirname,
                    )
        input_len=environ.get('CONTENT_LENGTH', 0)
        if input_len:
            cgi_input=environ['wsgi.input'].read(input_len)
        else:
            cgi_input=""
        #print "cgi input", cgi_input
        stdout, stderr = proc.communicate(cgi_input)
        if stderr:
            return [stderr]
        header,content=self._split_return(stdout)
        status,headers=self._split_header(header)
        start_response(status, headers)
        return [content]
