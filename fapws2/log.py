# stdlib imports
import sys
import time

# FAPWS imports
import config
import errors

access_logfid = sys.stdout
error_logfid = sys.stdout

def error(data):
    ts = time.strftime('%d/%b/%Y:%H:%M:%S', time.gmtime())
    error_logfid.write("%s: %s\n" % (ts, data))
    error_logfid.flush()

def access(ip, meth, url, protocol, status, length="-", referer="-", browser="-"):
    tts = time.strftime("%d/%b/%Y:%H:%M:%S", time.gmtime())
    access_logfid.write('%s - - [%s] "%s %s %s" %s %s "%s" "%s"\n' % (ip, tts, meth, url, protocol, status, length, referer, browser))
    access_logfid.flush()

def setup():
    '''Update some global variables based on command-line configuration.
    '''
    try:
        if config.conf['error_log']:
            error_logfid = open(config.conf['error_log'], 'a', 1)
        else:
            error_logfid = sys.stdout
    except Exception, e:
        errors.log_setup("can't log errors to file '%s': %s" %
            (config.conf['error_log'], e))
    try:
        if config.conf['access_log']:
            access_logfid = open(config.conf['access_log'], 'a', 1)
        else:
            access_logfid = sys.stdout
    except Exception, e:
        errors.log_setup("can't log access to file '%s': %s" %
            (config.cong['access_log'], e))
