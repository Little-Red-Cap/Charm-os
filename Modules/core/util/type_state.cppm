module;

export module util.type_state;

export namespace util {
    template <typename State>
    struct StateTag {
        using state = State;
    };

    template <typename State, typename Next>
    struct Transition {
        using from = State;
        using to = Next;
    };
}
