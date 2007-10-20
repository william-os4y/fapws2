# stdlib imports
import optparse

# FAPWS imports
import log
import errors

global_settings={
    'BUFSIZ': 65536,
    'LQUEUE_SIZ': 500,
    'SESSION_ID':'fapws_id',
    'DEBUG':False,
    'DEBUG2':False,
    'TRACE':False,
    'INFO': False,
}

conf = global_settings.copy()

def setup_options(config):
    '''Setup the program command line options.
    '''
    usage = "usage: %prog [options]"
    parser = optparse.OptionParser(usage)
    parser.add_option('-p', '--port', dest='port', type="int",
        default=config.get('port',0),
        help="Specifies the port number to listen on (default: 80). This will cause any interfaces specified in the configuration file or with -l to be ignored. This is for backwards compat only.")
    parser.add_option('-i', '--iface', dest='iface',
        default=config.get('iface',''),
        help="Specifies the interface listen on (default: all). This will cause any interfaces specified in the configuration file or with -l to be ignored. This is for backwards compat only.")
    parser.add_option('-l', '--listen', dest='listenaddrs', type='string',
        action='append', default=config.get('listenaddrs',[]),
        help="Specifies address and port to listen to. May be used multiple times. Addresses must be in the form of: ip:port (e.g. 127.0.0.1:80)")
    parser.add_option('-e', '--errorlogfile', dest='error_log',
        default=config.get('error_log',None),
        help="Specifies the error log file (default: sys.stdout)")
    parser.add_option('-a', '--accesslogfile', dest='access_log',
        default=config.get('access_log',None),
        help="Specifies the access log file (default: sys.stdout)")
    parser.add_option('-u', '--user', dest='user',
        default=config.get('user',None),
        help="Specifies the user to setuid to (default: don't setuid)")
    return parser

def socket_config(opts):
    global conf

    # popping the options from the opts.__dict__ keeps the final config
    # dictionary from being cluttered with entries not used anywhere else
    override_iface = opts.__dict__.pop('iface')
    override_port = opts.__dict__.pop('port')
    listenaddrs = opts.__dict__.pop('listenaddrs')

    # keep old behavior for backwards compatability
    if override_iface or override_port:
        if not override_port:
            override_port = 80
        conf['socketConfig'] = [(override_iface, override_port)]
        return

    conf['socketConfig'] =  []
    # no listenaddrs. We do this instead of a default value, because if any
    # listenaddrs are specified, the default value would still be in the list
    # this is not desirable behavior
    if len(listenaddrs) == 0:
        listenaddrs.append('0.0.0.0:80')
    # set up sockConfig
    for addr in listenaddrs:
        pair = addr.split(':')
        # unexpected format for an address
        if len(pair) > 2:
            errors.parser_error('listenaddr: %s improperly formatted' % (addr,))
        # properly formatted, just need to convert the port to an integer
        elif len(pair) == 2:
            pair[1] = int(pair[1])
        # else had a -l option that was in the form of 192.168.1.1
        else:
            pair.append(80)
        # make the pair into a tuple for easy use, and put it in the expected
        # place
        conf['socketConfig'].append(tuple(pair))
    return

def setup(localconfig):
    '''Point of entry.  Handle command line options, update the global
    configuration, set up the listening socket, setuid if necessary,
    and start the event loop.
    '''
    global conf

    # any user config overrides the global defaults
    conf.update(localconfig)

    parser = setup_options(conf)
    errors.parser_error = parser.error
    (options, args) = parser.parse_args()

    # do special configuration, specifically socket configs need some extra
    # processing done to them
    socket_config(options)
    conf.update(options.__dict__)
    log.setup()

