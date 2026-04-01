module;

export module player.app_test_hqzy.app_entry;

import player.app_test_hqzy.app_system;

export namespace player::app_test_hqzy::app_entry {
    inline int run() {
        return player::app_test_hqzy::app_system::run();
    }
}
