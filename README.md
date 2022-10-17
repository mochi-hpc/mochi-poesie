# POESIE: Embedding Scripting Languages for Mochi Services

## Installation

POESIE can easily be installed using Spack:

`spack install mochi-poesie`

This will install POESIE (and any required dependencies) with both
Python and Lua backends. Disabling one or the other can be done by
appending `~lua` or `~python`, for example:

`spack install poesie~lua`

## Architecture

Like most mochi services, POESIE relies on a client/provider architecture.
A provider, identified by its _address_ and _provider id_, manages one or more
interpreters (called _virtual machines_, or _VMs_), referenced externally
by either their name or their VM id.

## Starting a daemon using Bedrock

By installing POESIE with the `+bedrock` variant, one can deploy a daemon
by providing a JSON configuration like the following to Bedrock.

```json
{
    "libraries": [
        "libpoesie-bedrock-module.so
    ],
    "providers": [
        {
            "name": "my_poesie_provider",
            "provider_id": 0,
            "config": {
                "vms": {
                    "my_vm": {
                        "language": "python"
                    }
                }
            }
        }
    ]
}
```

## Client API

The client API is available in _poesie-client.h_.
The codes in the _test_ folder illustrate how to use it.
