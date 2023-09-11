# fmerge [![Build](https://github.com/lte678/fmerge_cpp/actions/workflows/cmake-single-platform.yml/badge.svg)](https://github.com/lte678/fmerge_cpp/actions/workflows/cmake-single-platform.yml)
_A file sync tool to keep folders synchronized across multiple computers_

---

## What does it do?

This tool does a little bit more than a standard rsync (for example) would do when run on both computers.
What it also does, is keep track of which files exist when to allow us to differntiate between the create of a file in one folder from the deletion of that same file in another.
This most importantly allows file deletions to be propagated between peers.

It also presents a conflict resolution dialog to unify conflicting changes.

## How is it used?

With much care. It is currently not thoroughly tested.
In theory, in it's current state it should be able to somewhat robustly preform the afore-mentioned functions.

The tool is executed in a client-server configuration, but these have no special meaning; The software should in theory yield identical results both ways around.

## Current Status

Currently, fmerge can reliably keep two computers in sync, but still has numerous issues and is missing the following features:

- Large file support (file size limited by RAM currently)
- Automatic and clean termination of application
- Daemon mode for server

## Author
Me!
