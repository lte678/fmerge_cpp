#pragma once

enum class State {
    AwaitingVersion,
    SendTree,
    ResolvingConflicts,
    SyncUserWait,
    SyncingFiles,
    Finished,
    Exiting,
};