pfc - The Port Forwarding Compressor

pfc is a small program designed to establish compressed TCP connections between
machines. Two copies of pfc talk to each other with a with a simple compressed
protocol. This provides for a simple means to compress typical TCP protocols
such as POP, IMAP or NNTP. It comes in handy when you're connecting over a
modem or WAN. It works both under Unix and Win32 (though the state of the Win32
port is a bit sketchy).  

See INSTALL for installation instructions.

Issues

pfc is alpha software. It has worked for me for over a year, but I don't
consider it feature complete. It lacks decent documentation, a few features,
and polish.

Improper use of pfc can create security holes. It doesn't encrypt the
connection. It allows anybody who can connect to the source port on the client
machine to connect to the server machine!  Use ssh port forwarding if you need
protections for either of these security problems.  (pfc was designed for a
totally open networking envronment.)

Download

New versions will be available from http://www.lickey.com/pfc/.

If the above page is gone, you'll probably be able to get to my home page via
http://www.bigfoot.com/~matt_armstrong, at least until bigfoot.com goes kaput.
Short of that, do a web search for the words "matt davina armstrong" -- thanks
to my wife's uncomon name, that search often brings our page near the top of
the list in search engines.

Contact

They say I'll be reachable at matt_armstrong@bigfoot.com forever.
