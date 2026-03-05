module;

export module block.device;

import fs_block;
import fs_errno;
import fs_stream;

export namespace block {
    using Device = fs::BlockDevice;
    using Status = fs::Status;
    using Errc = fs::Errc;
}
