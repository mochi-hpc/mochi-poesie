# POESIE: Embedding Scripting Languages for Mochi Services

## Installation

POESIE can easily be installed using Spack:

`spack install poesie`

This will install POESIE (and any required dependencies) with both
Python and Lua backends. Disabling one or the other can be done by
appending `~lua` or `~python`, for example:

`spack install poesie~lua`

## Architecture

Like most mochi services, POESIE relies on a client/provider architecture.
A provider, identified by its _address_ and _multiplex id_, manages one or more
interpreters (called _virtual machines_, or _VMs_), referenced externally 
by either their name or their VM id.

## Starting a daemon

POESIE ships with a default daemon program that can setup providers and
databases. This daemon can be started as follows:

`poesie-server-daemon [OPTIONS] <listen_addr> <VM name 1>[:lua|:py] <VM name 2>[:py] ...`

For example:

`poesie-server-daemon tcp://localhost:1234 foo:lua bar:py`

listen_addr is the address at which to listen; VM names should be provided in the form
_name:language_ where _language_ is _py_ (Python) or _lua_ (Lua).

The following additional option is accepted:

* `-f` provides the name of the file in which to write the address of the daemon.

Note that you can start the daemon without any VM to manage; the daemon will
simply respond to client requests by creating temporary VMs to run the code sent.

## Client API

The client API is available in _poesie-client.h_.
The codes in the _test_ folder illustrate how to use it.

## Provider API

The server-side API is available in _poesie-server.h_.
The code of the daemon (_src/poesie-server-daemon.c_) can be used as an example.

