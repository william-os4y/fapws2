#include <Python.h>
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/types.h>
#include <sys/stat.h>
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/signal.h>
#include <unistd.h>
#include <netdb.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "event.h"
#include "evhttp.h"
#include "http-internal.h"
#include "log.h"




void
send_error(struct evhttp_request *req, char *msg)
{
        char *escaped_html = evhttp_htmlescape(req->uri);
        struct evbuffer *buf = evbuffer_new();
        evhttp_response_code(req, HTTP_NOTFOUND, "Not Found");
        evbuffer_add_printf(buf, msg, escaped_html);
        free(escaped_html);
        evhttp_send_page(req, buf);
        evbuffer_free(buf);
}

/* This is a slightly modified version of the evhttp_handle_request module of libevent
This one return once the begining of the requested uri match one of the registered ones.
*/
void
evhttp_handle_request(struct evhttp_request *req, void *arg)
{
    struct evhttp *http = arg;
    struct evhttp_cb *cb;

    if (req->uri == NULL) {
        evhttp_send_error(req, HTTP_BADREQUEST, "Bad Request");
        return;
    }

    /* Test for different URLs */
    TAILQ_FOREACH(cb, &http->callbacks, next) {
        int res;
        char *p = strchr(req->uri, '?');
        if (p == NULL)
            res = strncmp(cb->what, req->uri, strlen(cb->what)) == 0;
        else
            res = strncmp(cb->what, req->uri,
                strlen(cb->what)) == 0;
        if (res) {
            (*cb->cb)(req, cb->cbarg);
            return;
        }
    }

    /* Generic call back */
    if (http->gencb) {
        (*http->gencb)(req, http->gencbarg);
        return;
    } else {
        /* We need to send a 404 here */
        char *fmt = "<html><head>"
            "<title>404 Not Found</title>"
            "</head><body>"
            "<h1>Not Found</h1>"
            "<p>The requested URL %s was not found on this server.</p>"
            "</body></html>\n";
        send_error(req, fmt);
    }
}

/* 
  modified version of evhttp_decode_uri which does not look for the char "?"
*/
char *
decode_uri(const char *uri)
{
    char c, *ret;
    int i, j;
    
    ret = malloc(strlen(uri) + 1);
    if (ret == NULL)
        event_err(1, "%s: malloc(%d)", __func__, strlen(uri) + 1);

    for (i = j = 0; uri[i] != '\0'; i++) {
        c = uri[i];
        if (c == '+') {
            c = ' ';
        } else if (c == '%' && isxdigit((unsigned char)uri[i+1]) &&
            isxdigit((unsigned char)uri[i+2])) {
            char tmp[] = { uri[i+1], uri[i+2], '\0' };
            c = (char)strtol(tmp, NULL, 16);
            i += 2;
        }
        ret[j++] = c;
    }
    ret[j] = '\0';
    
    return (ret);
}









/*
Global declaration
*/
struct evhttp* http_server;
PyObject *py_base_module, *py_config_module;
struct cb_params {
    char *url_path;
    PyObject *pycbobj;
};
char *server_name=NULL;
short server_port;


PyObject *parse_query(char *);

PyObject *
py_parse_query(PyObject *self, PyObject *args)
{
    char *uri;

    if (!PyArg_ParseTuple(args, "s", &uri))
        return NULL;
    return parse_query(uri);
}

PyObject *
parse_query(char * uri)
{
    PyObject *pydict=PyDict_New();
    char *line;
    char *p;
    if ((line = strdup(uri)) == NULL) {
        printf("failed to strdup\n");
        return NULL;
    }
    p=line;
    
    while (p != NULL && *p != '\0') {
        char  *value, *key;
        PyObject *pyelem;
        PyObject *pydummy;
        value = strsep(&p, "&");
        key = strsep(&value, "=");
        if (value==NULL) {
            value="";
        }
        //value=evhttp_decode_uri(value);
        if ((pyelem=PyDict_GetItemString(pydict, key))==NULL) {
            pyelem=PyList_New(0);
        } else {
            Py_INCREF(pyelem); // because of GetItem 
        }
        if (!PyList_Check(pyelem))
            return NULL;
        pydummy=PyString_FromString(value);
        PyList_Append(pyelem, pydummy);
        Py_DECREF(pydummy);
        PyDict_SetItemString(pydict, key, pyelem);
        Py_DECREF(pyelem); 
    }
    free(line);
    return pydict;
}

char *
transform_header_key_to_wsgi_key(char *key) 
{
    int i=0;
    char elem, *res;

    //printf("BEFORE TRANSF:%s\n", key);
    res=malloc(strlen(key) + 1);
    strcpy(res,key);
    for (i=0; i<=strlen(key); i++) {
       elem=key[i];
       if (elem=='-')
           elem='_';
       else
           elem=toupper(elem);
       res[i]=elem;
    }
    //printf("AFTER TRANSF:%s\n", res);
    return res;
}

static PyObject *
py_from_queue_to_dict(const struct evkeyvalq *headers, char *key_head)
{
    struct evkeyval *header;
    PyObject* pydict = PyDict_New();
    PyObject *pyval;
    
    TAILQ_FOREACH(header, headers, next) {
        char *key, *head;
        head=transform_header_key_to_wsgi_key(header->key);
        key=(char*)calloc(strlen(key_head)+strlen(header->key)+1, sizeof(char));
        strcat(key, key_head);
        strcat(key,head);
        pyval = Py_BuildValue("s", header->value );
        PyDict_SetItemString(pydict, key, pyval);
        Py_DECREF(pyval);
        free(key);
        free(head);
    }
    free(header);
    return pydict;
}

PyObject *
py_build_uri_variables(struct evhttp_request *req, char *url_path)
{
    PyObject* pydict = PyDict_New();
    char *uri, *rst_uri, *path_info, *query_string;
    int len=0;
    PyObject *pydummy=NULL;

    pydummy=PyString_FromString(server_name);
    PyDict_SetItemString(pydict, "SERVER_NAME", pydummy);
    Py_DECREF(pydummy);
    pydummy=Py_BuildValue("h", server_port);
    PyDict_SetItemString(pydict, "SERVER_PORT", pydummy);
    Py_DECREF(pydummy);

    pydummy=PyString_FromString(req->uri);
    PyDict_SetItemString(pydict, "fapws.uri", pydummy);
    Py_DECREF(pydummy);
    
    //decode the uri
    uri=decode_uri(req->uri);
    // Clean up the uri 
    len=strlen(uri)-strlen(url_path)+1;
    rst_uri = calloc(len, sizeof(char));
    strncpy(rst_uri, uri + strlen(url_path), len);
    pydummy=PyString_FromString(url_path);
    PyDict_SetItemString(pydict, "SCRIPT_NAME", pydummy);
    Py_DECREF(pydummy);

    if (strchr(rst_uri, '?') == NULL) {
        pydummy=PyString_FromString(rst_uri);
        PyDict_SetItemString(pydict, "PATH_INFO", pydummy);
        Py_DECREF(pydummy);
        pydummy=PyString_FromString("");
        PyDict_SetItemString(pydict, "QUERY_STRING", pydummy);    
        Py_DECREF(pydummy);
    }
    else {
        query_string=strdup(rst_uri);
        path_info=strsep(&query_string,"?");
        pydummy=PyString_FromString(path_info);
        PyDict_SetItemString(pydict, "PATH_INFO", pydummy);
        Py_DECREF(pydummy);
        pydummy=PyString_FromString(query_string);        
        PyDict_SetItemString(pydict,"QUERY_STRING",pydummy);
        Py_DECREF(pydummy);
        pydummy=parse_query(query_string);
        PyDict_SetItemString(pydict,"fapws.params",pydummy);
        Py_DECREF(pydummy);
        free(path_info);
    }
    free(rst_uri);
    free(uri);
    return pydict;
}

PyObject *
py_build_method_variables(PyObject *pyenvdict, struct evhttp_request *req)
{
    PyObject* pydict = PyDict_New();
    PyObject *pymethod=NULL;
    PyObject *pydummy=NULL;

    char *contenttype;
    contenttype=evhttp_find_header(req->input_headers,"Content-Type");
    
    switch( req->type )
    {
        case EVHTTP_REQ_POST: 
            {
#ifdef DEBUG
                printf("POST METHOD%i\n", EVHTTP_REQ_POST);
#endif
                char length[10];
                pymethod = PyString_FromString("POST");
                PyObject *pystringio=PyDict_GetItemString(pyenvdict, "wsgi.input");
                Py_INCREF(pystringio);
                PyObject *pystringio_write=PyObject_GetAttrString(pystringio, "write");
                Py_DECREF(pystringio);
                //TODO:we should add a check against limit
                pydummy=PyBuffer_FromMemory(EVBUFFER_DATA(req->input_buffer), EVBUFFER_LENGTH(req->input_buffer));
                PyObject_CallFunction(pystringio_write, "(O)", pydummy);
                Py_DECREF(pydummy);
                Py_DECREF(pystringio_write);
                PyObject *pystringio_seek=PyObject_GetAttrString(pystringio, "seek");
                pydummy=PyInt_FromString("0", NULL, 10);
                PyObject_CallFunction(pystringio_seek, "(O)", pydummy);
                Py_DECREF(pydummy);
                Py_DECREF(pystringio_seek);
                sprintf(length, "%i", EVBUFFER_LENGTH(req->input_buffer));
                pydummy=PyString_FromString(length);
                PyDict_SetItemString(pydict, "CONTENT_LENGTH", pydummy);
                Py_DECREF(pydummy);
                //fapws.params cannot be done in case of multipart
                if (strncasecmp(contenttype, "multipart", 9)!=0) {
                    char *buff;
                    buff=malloc(EVBUFFER_LENGTH(req->input_buffer)+1);
                    strncpy(buff,(char *)EVBUFFER_DATA(req->input_buffer), EVBUFFER_LENGTH(req->input_buffer));
                    buff[EVBUFFER_LENGTH(req->input_buffer)]='\0';
                    buff=decode_uri(buff);
                    pydummy=parse_query(buff);
                    free(buff);
                    PyDict_SetItemString(pydict,"fapws.params",pydummy);
                    Py_DECREF(pydummy);
                }
            }
            break;
        case EVHTTP_REQ_HEAD: 
            {
#ifdef DEBUG
                printf("HEAD METHOD%i\n",EVHTTP_REQ_HEAD);
#endif
                pymethod = PyString_FromString("HEAD");
            }
            break;
        case EVHTTP_REQ_GET: 
            {
#ifdef DEBUG
                printf("GET METHOD%i\n", EVHTTP_REQ_GET);
#endif
                pymethod = PyString_FromString("GET");
            }
            break;
    }
    PyDict_SetItemString(pydict, "REQUEST_METHOD", pymethod);
    Py_DECREF(pymethod);
    if (contenttype) {
        pydummy=PyString_FromString(contenttype);
        PyDict_SetItemString(pydict, "CONTENT_TYPE", pydummy);
        Py_DECREF(pydummy);
    }
    return pydict;
}

PyObject *
py_get_request_info(struct evhttp_request *req)
{
    PyObject* pydict = PyDict_New();
    PyObject *pydummy=NULL;
    
    pydummy=PyString_FromString(req->remote_host);
    PyDict_SetItemString(pydict, "fapws.remote_host", pydummy);
    Py_DECREF(pydummy);
    pydummy=Py_BuildValue("H", req->remote_port);
    PyDict_SetItemString(pydict, "fapws.remote_port", pydummy);
    Py_DECREF(pydummy);
    pydummy=Py_BuildValue("b", req->major);
    PyDict_SetItemString(pydict, "fapws.http_major", pydummy);
    Py_DECREF(pydummy);
    pydummy=Py_BuildValue("b", req->minor);
    PyDict_SetItemString(pydict, "fapws.http_minor", pydummy);
    Py_DECREF(pydummy);
    return pydict;
}

static PyObject *
py_build_environ(const struct evkeyvalq *headers)
{
    return py_from_queue_to_dict(headers, "HTTP_");
}

static PyObject *
py_set_base_module(PyObject *self, PyObject *args)
{
    if (!PyArg_ParseTuple(args, "O", &py_base_module)) 
        return NULL;
    py_config_module=PyObject_GetAttrString(py_base_module, "config");
    return Py_None;    
}

void 
update_environ(PyObject *pyenviron, PyObject *pydict, char *method)
{
    PyObject *pyupdate=PyObject_GetAttrString(pyenviron, method);
    PyObject_CallFunction(pyupdate, "(O)", pydict);
    Py_DECREF(pyupdate);
    
}

void 
signal_cb(int fd, short event, void *arg)
{
        struct event *signal = arg;
        printf("got signal %d\n", EVENT_SIGNAL(signal));
        //TODO: call callback
        event_del(signal);
        evhttp_free(http_server);
}




void 
python_handler( struct evhttp_request *req, void *arg)
{
    struct evbuffer *evb=evbuffer_new();
    PyObject *pydict;
    int index=0;

    struct cb_params *params=(struct cb_params*)arg;
    //build environ
    //  1)initialise environ
    PyObject *pyenviron_class=PyObject_GetAttrString(py_base_module, "Environ");
    if (!pyenviron_class)
         event_err(1,"load Environ failed from base module");
    PyObject *pyenviron=PyObject_CallObject(pyenviron_class, NULL);
    if (!pyenviron)
         event_err(1,"Failed to create an instance of Environ");
    Py_DECREF(pyenviron_class);
    //  2)transform headers into adictionary and send it to environ.update_headers
    pydict=py_build_environ(req->input_headers);
    update_environ(pyenviron, pydict, "update_headers");
    Py_DECREF(pydict);
    //  3)transform uri and send it to environ.update_uri
    pydict=py_build_uri_variables(req, params->url_path);
    update_environ(pyenviron, pydict, "update_uri");
    Py_DECREF(pydict);
    //  4)in case of POST analyse the request and send it to environ.update_method
    pydict=py_build_method_variables(pyenviron, req);
    update_environ(pyenviron, pydict, "update_method");
    Py_DECREF(pydict);
    //  5)add some request info
    pydict=py_get_request_info(req);
    update_environ(pyenviron, pydict, "update_from_request");
    Py_DECREF(pydict);
    
    //build start_response
    PyObject *pystart_response_class=PyObject_GetAttrString(py_base_module, "Start_response");
    PyObject *pystart_response=PyInstance_New(pystart_response_class, NULL, NULL);
    Py_DECREF(pystart_response_class);
    //execute python callbacks with his parameters
    PyObject *pyarglist = Py_BuildValue("(OO)", pyenviron, pystart_response );
    PyObject *pyresult = PyEval_CallObject(params->pycbobj,pyarglist);
    Py_DECREF(pyarglist);
#ifdef DEBUG
    printf("pass callobject\n");
#endif
    if (pyresult==NULL) {
        printf("We have an error in the python code associated to the url :%s\n", req->uri);
         if (PyErr_Occurred()) { 
             //get_traceback();
             PyObject *pyerrormsg_method=PyObject_GetAttrString(py_base_module,"redirectStdErr");
             PyObject *pyerrormsg=PyObject_CallFunction(pyerrormsg_method, NULL);
             Py_DECREF(pyerrormsg_method);
             Py_DECREF(pyerrormsg);
             PyErr_Print();
             PyObject *pysys=PyObject_GetAttrString(py_base_module,"sys");
             PyObject *pystderr=PyObject_GetAttrString(pysys,"stderr");
             Py_DECREF(pysys);
/*             PyObject *pyclose_method=PyObject_GetAttrString(pystderr, "close");
             PyObject *pyclose=PyObject_CallFunction(pyclose_method, NULL);
             Py_DECREF(pyclose_method);
             Py_DECREF(pyclose);*/
             PyObject *pygetvalue=PyObject_GetAttrString(pystderr, "getvalue");
             Py_DECREF(pystderr);
             PyObject *pyres=PyObject_CallFunction(pygetvalue, NULL);
             Py_DECREF(pygetvalue);
             printf("%s\n", PyString_AsString(pyres));
             //test if we must send it to the page
             PyObject *pysendtraceback = PyObject_GetAttrString(py_config_module,"send_traceback_to_browser");
             char htmlres[2048];
             if (pysendtraceback==Py_True) {
                sprintf(htmlres, "%s%s%s", "Error for %s:<br/><pre>", PyString_AsString(pyres), "</pre>");
             } else {
                PyObject *pyshortmsg = PyObject_GetAttrString(py_config_module,"send_traceback_short");
                sprintf(htmlres, "%s%s", "Error for %s:<br/>", PyString_AsString(pyshortmsg));
                Py_DECREF(pyshortmsg);
             }
             Py_DECREF(pyres);
             send_error(req, htmlres);
             Py_DECREF(pysendtraceback);
             //need to close what was already open
             Py_DECREF(pystart_response);
             Py_DECREF(pyenviron);
             evbuffer_free(evb);
         }
        return ;
    }
    //get status_code and reason
    //set headers
    PyObject *pyresponse_headers_dict=PyObject_GetAttrString(pystart_response, "response_headers");
    PyObject *pykey, *pyvalue;
    int pos = 0;
#ifdef DEBUG
    printf("Check:%s\n", req->uri);
#endif
    if (PyDict_Check(pyresponse_headers_dict)) {
        while (PyDict_Next(pyresponse_headers_dict, &pos, &pykey, &pyvalue)) {
            evhttp_add_header(req->output_headers, PyString_AsString(pykey), PyString_AsString(pyvalue));
            //printf("ADD HEADER:%s=%s\n", PyString_AsString(pykey), PyString_AsString(pyvalue));
        }
    }
    Py_DECREF(pyresponse_headers_dict);
#ifdef DEBUG
    printf("passheader \n");
#endif
    //get start_response data
    PyObject *pystatus_code=PyObject_GetAttrString(pystart_response,"status_code");
    int status_code;
    char *status_code_str=PyString_AsString(pystatus_code);
    Py_DECREF(pystatus_code);
    status_code=atoi(status_code_str);
#ifdef DEBUG
    printf("pass status code:%i\n", status_code);
#endif
    PyObject *pystatus_reasons=PyObject_GetAttrString(pystart_response,"status_reasons");
    char *status_reasons=PyString_AsString(pystatus_reasons);
    Py_DECREF(pystatus_reasons);
#ifdef DEBUG
    printf("pass status reason:%s\n", status_reasons);
#endif
    Py_DECREF(pystart_response);
    Py_DECREF(pyenviron);
    
    evhttp_send_reply_start(req, status_code, status_reasons);
#ifdef DEBUG
    printf("send status code and status reasons\n");
#endif
    if (req->type==EVHTTP_REQ_HEAD) {
#ifdef DEBUG
        printf("Method HEAD requested. No content send\n");
#endif        
        evhttp_send_reply_end(req);
    } else { 
        if (PyList_Check(pyresult)) {
#ifdef DEBUG
            printf("wsgi output is a list\n");
#endif
            for (index=0; index<PyList_Size(pyresult); index++) {
                char *buff;
                int buflen;
                PyObject_AsReadBuffer(PyList_GetItem(pyresult, index), (const void **) &buff, &buflen);
                //printf("SIZEOF:%i=%i,%i\n", index, strlen(buff), buflen);
                evbuffer_add(evb, buff, buflen);    
                evhttp_send_reply_chunk(req, evb);
            }
            evhttp_send_reply_end(req);
        } else if (PyFile_Check(pyresult)) {
#ifdef DEBUG
            printf("wsgi output is a file\n");
#endif
            char buff[32768]="";  //this is the chunk size
            int bytes=0;
            FILE *file=PyFile_AsFile(pyresult);
            while ((bytes=fread(buff, 1, sizeof(buff), file))) {
                //printf("FILE:%i \n",bytes);
                evbuffer_add(evb, buff, bytes);    
                evhttp_send_reply_chunk(req, evb);
                buff[0]='\0';
            }
            evhttp_send_reply_end(req);
        } else {
            printf("wsgi output of %s is neither a list neither a fileobject\n",PyString_AsString(PyObject_Repr(params->pycbobj)));
            //PyErr_SetString(PyExc_TypeError, "Result must be a list or a fileobject");
            return ;
        }
    }
    Py_DECREF(pyresult);
    
    evbuffer_free(evb);    
    //free(params); generate a crash
}

static PyObject *
py_evhttp_start(PyObject *self, PyObject *args)
{

    if (!PyArg_ParseTuple(args, "si", &server_name, &server_port))
        return NULL;
    printf("start at: %s, %i\n",server_name, server_port);
    if ((http_server=evhttp_start( server_name, server_port))==NULL) {
        PyErr_SetString(PyExc_SystemError, "Error with http_start. No access to that server on that port.");
        return NULL;
    }
    return Py_None;
}

static PyObject *
py_evhttp_cb(PyObject *self, PyObject *args)
{
    PyObject *pycbobj;
    char *path;
    struct cb_params *params;

    if (!PyArg_ParseTuple(args, "sO", &path, &pycbobj)) 
        return NULL;
    if (!PyCallable_Check(pycbobj)) {
        PyErr_SetString(PyExc_TypeError, "Need a callable object!");
        return NULL;
    }
    params=calloc(1,sizeof(struct cb_params));
    params->url_path=strdup(path);
    params->pycbobj=pycbobj;
    //check path is not null
    evhttp_set_cb(http_server, path, python_handler, params);
    return Py_None;
}

static PyObject *
py_genhttp_cb(PyObject *self, PyObject *args)
{
    PyObject *pycbobj;
    struct cb_params *params;

    if (!PyArg_ParseTuple(args, "O", &pycbobj)) 
        return NULL;
    if (!PyCallable_Check(pycbobj)) {
        PyErr_SetString(PyExc_TypeError, "Need a callable object!");
        return NULL;
    }
    params=calloc(1,sizeof(struct cb_params));
    params->url_path="";
    params->pycbobj=pycbobj;
    evhttp_set_gencb(http_server, python_handler, params);
    return Py_None;
}

static PyObject *
py_evhttp_free(PyObject *self, PyObject *args)
{
    evhttp_free(http_server);
    Py_DECREF(py_base_module);
    return Py_None;
}

static PyObject *
py_evhttp_get_timeout(PyObject *self, PyObject *args)
{
    int timeout;

    timeout=http_server->timeout;
    return Py_BuildValue("i", timeout);
}

static PyObject *
py_evhttp_set_timeout(PyObject *self, PyObject *args)
{
    int timeout;

    if (!PyArg_ParseTuple(args, "i", &timeout))
        return NULL;
    evhttp_set_timeout(http_server,timeout);
    return Py_None;
}

static PyObject *
py_event_dispatch(PyObject *self, PyObject *args)
{
    struct event signal_int;

    event_set(&signal_int, SIGINT, EV_SIGNAL|EV_PERSIST, signal_cb, &signal_int);
    event_add(&signal_int, NULL);
    event_dispatch();
    return Py_None;
}


static PyObject *
py_htmlescape(PyObject *self, PyObject *args)
{
    const char *data;
    char *result;

    if (!PyArg_ParseTuple(args, "s", &data))
        return NULL;
    result = evhttp_htmlescape(data); 
    PyObject *pyres=Py_BuildValue("s", result);
    free(result);
    return pyres;
}


static PyObject *
py_evhttp_encode_uri(PyObject *self, PyObject *args)
{
    const char *data;
    char *result;

    if (!PyArg_ParseTuple(args, "s", &data))
        return NULL;
    result = evhttp_encode_uri(data);
    PyObject *pyres=Py_BuildValue("s", result);
    free(result);
    return pyres;
}

static PyObject *
py_decode_uri(PyObject *self, PyObject *args)
{
    const char *data;
    char *result;

    if (!PyArg_ParseTuple(args, "s", &data))
        return NULL;
    result = decode_uri(data);
    PyObject *pyres=Py_BuildValue("s", result);
    free(result);
    return pyres;
}

static PyMethodDef EvhttpMethods[] = {
    {"htmlescape", py_htmlescape, METH_VARARGS, "html escape"}, 
    {"start", py_evhttp_start, METH_VARARGS, "Start evhttp"},
    {"free", py_evhttp_free, METH_VARARGS, "free evhttp"},
    {"get_timeout", py_evhttp_get_timeout, METH_VARARGS, "get connection timeout"},
    {"set_timeout", py_evhttp_set_timeout, METH_VARARGS, "set connection timeout"},
    {"event_dispatch", py_event_dispatch, METH_VARARGS, "Event dispatch"},
    {"http_cb", py_evhttp_cb, METH_VARARGS, "HTTP Event callback"},
    {"gen_http_cb", py_genhttp_cb, METH_VARARGS, "Generic HTTP callback"},
    {"set_base_module", py_set_base_module, METH_VARARGS, "set you base module"},
    {"encode_uri",py_evhttp_encode_uri, METH_VARARGS, "encode the uri"},
    {"decode_uri",py_decode_uri, METH_VARARGS, "decode the uri"},
    {"parse_query", py_parse_query, METH_VARARGS, "parse query into dictionary"},

    {NULL, NULL, 0, NULL}        /* Sentinel */
};

PyMODINIT_FUNC
init_evhttp(void)
{
    event_init();
#ifdef DEBUG
    printf("init\n");
#endif
    (void) Py_InitModule("_evhttp", EvhttpMethods);
}
