# battle
This program is a server for a network based pokemon-style text only battle arena. The default port for the server is 30303, although a custom port may be used by passing PORT=[port_number] to the makefile. To connect to the server, use your favourite network client in noncanonical mode.

The entire server runs in a single process, never using a fork or exec system call. Working on this project required me to solve problems having to do with concurrency, handling many connections at once, and synchronizing multiple I/O streams to give the appearance of simultaneity within a single process.
