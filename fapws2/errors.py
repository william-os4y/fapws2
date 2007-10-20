import sys
#import event

def fatal(phase, message):
    event.abort()
    print 'Error during: %s' %(phase,)
    print message
    print 'Exiting.'
    sys.exit(1)

def socket_error(error, ip, port):
    phase = 'socket creation'
    message = '%s encountered while creating socket for: %s:%s\n %s' % \
        (error.__class__.__name__, ip, port, str(error))
    fatal(phase, message)

def setuid_error(message):
    phase = 'setuid'
    fatal(phase, message)

def log_setup(message):
    phase = 'log setup'
    fatal(phase, message)

# this is overriden once the config.setup is run
parser_error = lambda *args: None
