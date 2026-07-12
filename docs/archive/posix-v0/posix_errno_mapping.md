# POSIX errno Mapping (Draft)

This document maps `util::Errc` to POSIX errno values and lists gaps
required for BusyBox phase 2/3.

## Current `util::Errc` Coverage

| util::Errc        | POSIX errno | Notes |
|------------------|-------------|-------|
| perm             | EPERM       | basic permission error |
| noent            | ENOENT      | missing file/path |
| io               | EIO         | generic IO error |
| again            | EAGAIN      | would block |
| nomem            | ENOMEM      | allocation failure |
| busy             | EBUSY       | resource busy |
| exist            | EEXIST      | already exists |
| inval            | EINVAL      | invalid argument |
| rofs             | EROFS       | read-only filesystem |
| nametoolong      | ENAMETOOLONG| path too long |
| nosys            | ENOSYS      | not implemented |
| notsup           | ENOTSUP     | not supported |
| timeout          | ETIMEDOUT   | timeout |

## BusyBox Phase 2/3 Required errno

Minimum set to support shell, redirects, and pipelines:

- EPERM, ENOENT, EIO, EAGAIN, ENOMEM, EBUSY, EEXIST, EINVAL, ENOSYS
- EACCES, ENOTDIR, EISDIR, EPIPE, EMFILE, ENFILE, ENOSPC

## Gaps to Add or Translate

These are not present in `util::Errc` yet and should be added or
mapped explicitly in the POSIX wrapper:

- EACCES (permission denied)
- ENOTDIR (path component not a directory)
- EISDIR (is a directory)
- EPIPE (broken pipe)
- EMFILE (per-process fd table full)
- ENFILE (system fd pool full)
- ENOSPC (no space / pool exhausted)

## Wrapper Policy

- Core APIs return `util::Errc`.
- POSIX compatibility layer converts to `errno` and POSIX return patterns.
- `waitpid` uses structured `WaitStatus`; POSIX wrapper converts to `int status`.

