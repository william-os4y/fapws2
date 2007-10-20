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
PyObject *base_module;
struct cb_params {
    char *url_path;
    PyObject *pycbobj;
};

static PyObject *
from_queue_to_dict(const struct evkeyvalq *headers, char *key_head)
{
    struct evkeyval *header;
    PyObject* dict = PyDict_New();
    PyObject *val;
    //char *key_head="http_";
    
    TAILQ_FOREACH(header, headers, next) {
        char *key;
        key=(char*)calloc(strlen(key_head)+strlen(header->key)+1, sizeof(char));
        strcat(key, key_head);
        strcat(key,header->key);
        val = Py_BuildValue("s", header->value );
        PyDict_SetItemString(dict, key, val);
        //printf("DICT:%s:%s\n",key, val);
    }
    return dict;
}

static PyObject *
build_request_variables(struct evhttp_request *req, char *url_path)
{
    PyObject* dict = PyDict_New();
    PyObject *method=NULL;
    char *full_uri, *path_info, *query_string;
    struct evkeyvalq *uri_params;
    PyObject *query_dict=PyDict_New();
    
    switch( req->type )
    {
        case EVHTTP_REQ_POST: method = PyString_FromString("POST");
        case EVHTTP_REQ_HEAD: method = PyString_FromString("HEAD");
        case EVHTTP_REQ_GET: method = PyString_FromString("GET");
    }
    PyDict_SetItemString(dict, "REQUEST_METHOD", method);
    
    /* Clean up the uri */
    full_uri = strdup(req->uri);
    PyDict_SetItemString(dict, "SCRIPT_NAME", PyString_FromString(url_path));
    if (strchr(full_uri, '?') == NULL) {
        PyDict_SetItemString(dict, "PATH_INFO", PyString_FromString(full_uri));
        PyDict_SetItemString(dict, "QUERY_STRING", PyString_FromString(""));    
        PyDict_SetItemString(dict,"QUERY_DICT",query_dict);
    }
    else {
        // TODO Fix path info so it doesn't there is no query info
        query_string=strdup(full_uri);
        path_info=strsep(&query_string,"?");
        PyDict_SetItemString(dict, "PATH_INFO", PyString_FromString(path_info));        
        PyDict_SetItemString(dict,"QUERY_STRING",PyString_FromString(query_string));
        evhttp_parse_query(req->uri, uri_params);
        query_dict=from_queue_to_dict(uri_params,"");
        PyDict_SetItemString(dict,"QUERY_DICT",query_dict);
        
    }
    //printf("path inf %s\n\n", path_info);
    return dict;
}


static PyObject *
build_environ(const struct evkeyvalq *headers)
{
    return from_queue_to_dict(headers, "http_");
}

static PyObject *
set_base_module(PyObject *self, PyObject *args)
{
    if (!PyArg_ParseTuple(args, "O", &base_module)) 
        return NULL;
    return Py_None;    
}

void 
reset_environ(PyObject *environ)
{
    PyObject *reset=PyObject_GetAttrString(environ, "reset");
    PyObject_CallFunction(reset, NULL);
}

void 
update_environ(PyObject *environ, PyObject *dict)
{
    PyObject *update=PyObject_GetAttrString(environ, "update");
    PyObject_CallFunction(update, "(O)", dict);
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
    PyObject *headers_dict, *request_dict;
 
    struct cb_params *params=(struct cb_params*)arg;
    //build environ
    PyObject *environ=PyObject_GetAttrString(base_module, "environ");
    reset_environ(environ);
    headers_dict=build_environ(req->input_headers);
    update_environ(environ, headers_dict);
    // This just adds some variables that are needed
    // from parsing the uri
    
    request_dict = build_request_variables(req, params->url_path);
    update_environ(environ, request_dict);

    //build start_response
    PyObject *start_response=PyObject_GetAttrString(base_module, "start_response");
    //execute python callbacks with his parameters
    PyObject *arglist = Py_BuildValue("(OO)", environ, start_response );
    PyObject *result = PyEval_CallObject(params->pycbobj,arglist);
    if (!PyString_Check(result)) {
          PyErr_SetString(PyExc_TypeError, "Result must be a string");
          //return NULL;
    }
    char *res=PyString_AsString(result);
    evbuffer_add_printf(evb, res);
    //printf("Request from %s:", req->remote_host);
    //TODO get start_response data
    evhttp_send_reply(req, HTTP_OK, "OK", evb);
    evbuffer_free(evb);
}

static PyObject *
py_evhttp_start(PyObject *self, PyObject *args)
{
    const char *address;
    short port;

    if (!PyArg_ParseTuple(args, "si", &address, &port))
        return NULL;
    printf("start at: %s, %i\n",address, port);
    http_server=evhttp_start( address, port);
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
    {"set_base_module", set_base_module, METH_VARARGS, "set you base module"},
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
