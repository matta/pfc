# pfc - The Port Forwarding Compressor

pfc is a small program designed to establish compressed TCP connections
between machines. Two copies of pfc talk to each other with a with a simple
compressed protocol. This provides for a simple means to compress typical
TCP protocols such as POP, IMAP or NNTP. It comes in handy when you're
connecting over a modem or WAN. It works both under Unix and Win32 (though
the state of the Win32
port is a bit sketchy).

See INSTALL for installation instructions.

# Issues

pfc is abandoned, alpha software. It has worked for me for over a year (as
of 1999, and I haven't used it since), but I don't consider it feature
complete. It lacks decent documentation, a few features, and polish.

Improper use of pfc can create security holes. It doesn't encrypt the
connection. It allows anybody who can connect to the source port on the
client machine to connect to the server machine!  Use ssh port forwarding
if you need protections for either of these security problems.  (pfc was
designed for a totally open networking envronment.)
