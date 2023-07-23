# fmerge
_A file sync tool to keep folders synchronized across multiple computers_

---

## What does it do?

This tool does a little bit more than a standard rsync (for example) would do when run on both computers.
What it also does, is keep track of which files exist when to allow us to differntiate between the create of a file in one folder from the deletion of that same file in another.
This most importantly allows file deletions to be propagated between peers.

## How is it used?

You don't. 

Please. 

Don't use this on your files, since it is incomplete and completely untested.
I will write a notice if I ever consider it mature enough for actual use.

The tool is executed in a client-server configuration, but the computers it is run on do not take on a client-server role.
The software should in theory yield identical results both ways around.

## Current Status

Currently, the software can detect and maintain changes that occur in the specified directory and merge these changes from a peer.
There are no smart merging algorithms yet and the software does not perform any of the calculated changes. 

## Author
Me!