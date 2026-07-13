# M1 Minimal Tests (Checklist)

> `status`: `archived`。该 checklist 不代表当前回归结果。

- wait + notify_one => ok
- wait + cancel => canceled
- wait_timeout + timeout => timeout
- wait_timeout + notify before due => ok
- notify_all => all ok
- IPC semaphore wait/post => ok
