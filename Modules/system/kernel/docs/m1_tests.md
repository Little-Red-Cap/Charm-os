# M1 Minimal Tests (Checklist)

- wait + notify_one => ok
- wait + cancel => canceled
- wait_timeout + timeout => timeout
- wait_timeout + notify before due => ok
- notify_all => all ok
- IPC semaphore wait/post => ok
