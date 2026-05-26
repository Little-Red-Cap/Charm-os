export module player.profile.hqzy_cm7_usb_storage;

import player.profile.hqzy_cm7_usb_storage.system;

export namespace player::profile::hqzy_cm7_usb_storage {
    inline int run() {
        return player::app_test_hqzy::usb_storage_system::run();
    }
}
