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

        char *escaped_html = evhttp_htmlescape(req->uri);
        struct evbuffer *buf = evbuffer_new();

        evhttp_response_code(req, HTTP_NOTFOUND, "Not Found");

        evbuffer_add_printf(buf, fmt, escaped_html);

        free(escaped_html);
        
        evhttp_send_page(req, buf);

        evbuffer_free(buf);
    }
}








struct evhttp* http_server;
PyObject *py_base_module;
struct cb_params {
    char *url_path;
    PyObject *pycbobj;
};
char *server_name=NULL;
short server_port;

static PyObject *
py_from_queue_to_dict(const struct evkeyvalq *headers, char *key_head)
{
    struct evkeyval *header;
    PyObject* pydict = PyDict_New();
    PyObject *pyval;
    
    TAILQ_FOREACH(header, headers, next) {
        char *key;
        key=(char*)calloc(strlen(key_head)+strlen(header->key)+1, sizeof(char));
        strcat(key, key_head);
        strcat(key,header->key);
        pyval = Py_BuildValue("s", header->value );
        PyDict_SetItemString(pydict, key, pyval);
        Py_DECREF(pyval);
        free(key);
    }
    free(header);
    return pydict;
}

void
py_build_request_variables(PyObject *pydict, struct evhttp_request *req, char *url_path)
{
    //PyObject* pydict = PyDict_New();
    PyObject *pymethod=NULL;
    char *rst_uri, *path_info, *query_string;
    int len=0;
    PyObject *pydummy;

    pydummy=PyString_FromString(server_name);
    PyDict_SetItemString(pydict, "SERVER_NAME", pydummy);
    Py_DECREF(pydummy);
    pydummy=Py_BuildValue("h", server_port);
    PyDict_SetItemString(pydict, "SERVER_PORT", pydummy);
    Py_DECREF(pydummy);

    switch( req->type )
    {
        case EVHTTP_REQ_POST: pymethod = PyString_FromString("POST");
        case EVHTTP_REQ_HEAD: pymethod = PyString_FromString("HEAD");
        case EVHTTP_REQ_GET: pymethod = PyString_FromString("GET");
    }
    PyDict_SetItemString(pydict, "REQUEST_METHOD", pymethod);
    Py_DECREF(pymethod);

    // Clean up the uri 
    len=strlen(req->uri)-strlen(url_path)+1;
    rst_uri = calloc(len, sizeof(char));
    strncpy(rst_uri, req->uri + strlen(url_path), len);
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
        pydummy=PyString_FromString(path_info);        
        PyDict_SetItemString(pydict,"QUERY_STRING",pydummy);
        Py_DECREF(pydummy);
        free(path_info);
    }
    //free(uri_params);
    free(rst_uri);
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
    return Py_None;    
}

void 
reset_environ(PyObject *pyenviron)
{
    PyObject *pyreset=PyObject_GetAttrString(pyenviron, "reset");
    PyObject_CallFunction(pyreset, NULL);
    Py_DECREF(pyreset);
}

void 
update_environ(PyObject *pyenviron, PyObject *pydict)
{
    PyObject *pyupdate=PyObject_GetAttrString(pyenviron, "update");
    PyObject_CallFunction(pyupdate, "(O)", pydict);
    //Py_DECREF(pydict);
    Py_DECREF(pyupdate);
    
}


void 
signal_cb(int fd, short event, void *arg)
{
        struct event *signal = arg;

        printf("got signal %d\n", EVENT_SIGNAL(signal));
        //TODO: call end callback
        event_del(signal);
        evhttp_free(http_server);
}

void
python_handler( struct evhttp_request *req, void *arg)
{
    struct evbuffer *evb=evbuffer_new();
    PyObject *pyheaders_dict;
    int index=0;
    char *res="";
 
    struct cb_params *params=(struct cb_params*)arg;
    //build environ
    PyObject *pyenviron_class=PyObject_GetAttrString(py_base_module, "Environ");
    PyObject *pyenviron=PyInstance_New(pyenviron_class, NULL, NULL);
    Py_DECREF(pyenviron_class);
    reset_environ(pyenviron);
    pyheaders_dict=py_build_environ(req->input_headers);
    update_environ(pyenviron, pyheaders_dict);
    Py_DECREF(pyheaders_dict);
    // This just adds some variables that are needed
    // from parsing the uri
    
    PyObject *pyrequest_dict=PyDict_New();
    py_build_request_variables(pyrequest_dict, req, params->url_path);
    update_environ(pyenviron, pyrequest_dict);
    Py_DECREF(pyrequest_dict);

    //build start_response
    PyObject *pystart_response_class=PyObject_GetAttrString(py_base_module, "Start_response");
    PyObject *pystart_response=PyInstance_New(pystart_response_class, NULL, NULL);
    Py_DECREF(pystart_response_class);
    //execute python callbacks with his parameters
    PyObject *pyarglist = Py_BuildValue("(OO)", pyenviron, pystart_response );
    PyObject *pyresult = PyEval_CallObject(params->pycbobj,pyarglist);
    Py_DECREF(pyarglist);
    //get status_code and reason
    //set headers
    PyObject *pyresponse_headers_dict=PyObject_GetAttrString(pystart_response, "response_headers");
    PyObject *pykey, *pyvalue;
    int pos = 0;
    //printf("Check:%s\n", req->uri);
    if (PyDict_Check(pyresponse_headers_dict)) {
        while (PyDict_Next(pyresponse_headers_dict, &pos, &pykey, &pyvalue)) {
            evhttp_add_header(req->output_headers, PyString_AsString(pykey), PyString_AsString(pyvalue));
            //printf("ADD HEADER:%s=%s\n", PyString_AsString(pykey), PyString_AsString(pyvalue));
        }
       //when uncomment server crash. why ?
       //Py_DECREF(pykey);
       //Py_DECREF(pyvalue);
    }
    Py_DECREF(pyresponse_headers_dict);
    //printf("Request from %s:", req->remote_host);
    //get start_response data
    PyObject *pystatus_code=PyObject_GetAttrString(pystart_response,"status_code");
    int status_code;
    char *status_code_str=PyString_AsString(pystatus_code);
    Py_DECREF(pystatus_code);
    status_code=atoi(status_code_str);

    PyObject *pystatus_reasons=PyObject_GetAttrString(pystart_response,"status_reasons");
    char *status_reasons=PyString_AsString(pystatus_reasons);
    Py_DECREF(pystatus_reasons);
    Py_DECREF(pystart_response);
    Py_DECREF(pyenviron);
    evhttp_send_reply_start(req, status_code, status_reasons);
    if (PyList_Check(pyresult)) {
        for (index=0; index<PyList_Size(pyresult); index++) {
            res=PyString_AsString(PyList_GetItem(pyresult, index));
            evbuffer_add_printf(evb, res);    
            evhttp_send_reply_chunk(req, evb);
        }
        evhttp_send_reply_end(req);
    } else if (PyFile_Check(pyresult)) {
        //printf("FILE RESULT\n");
        char buff[2048]="";
        FILE *file=PyFile_AsFile(pyresult);
        while (fread(buff, 1, sizeof(buff), file)) {
             //printf("FILE:%i \n",strlen(buff));
             evbuffer_add_printf(evb, buff);    
             evhttp_send_reply_chunk(req, evb);
             if (feof(file))
                 evbuffer_add_printf(evb, buff);    
                 evhttp_send_reply_chunk(req, evb);
                 break;
        }
        
        evhttp_send_reply_end(req);
    } else {
        PyErr_SetString(PyExc_TypeError, "Result must be a list");
        //return NUL;
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
    http_server=evhttp_start( server_name, server_port);
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
    printf("timeout %d\n", http_server->timeout);
    return Py_BuildValue("i", timeout);
}

static PyObject *
py_evhttp_set_timeout(PyObject *self, PyObject *args)
{
    int timeout;

    if (!PyArg_ParseTuple(args, "i", &timeout))
        return NULL;
    evhttp_set_timeout(http_server,timeout);
    printf("new timeout %d\n", http_server->timeout);
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
    return Py_BuildValue("s", result);
}


static PyObject *
py_evhttp_encode_uri(PyObject *self, PyObject *args)
{
    const char *data;
    char *result;

    if (!PyArg_ParseTuple(args, "s", &data))
        return NULL;
    result = evhttp_encode_uri(data);
    return Py_BuildValue("s", result);
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
    
    {NULL, NULL, 0, NULL}        /* Sentinel */
};

PyMODINIT_FUNC
init_evhttp(void)
{
    event_init();
    printf("init\n");
    (void) Py_InitModule("_evhttp", EvhttpMethods);
}
